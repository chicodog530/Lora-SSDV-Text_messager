#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "secrets.h"
#include "protocol.h"
#include "web_app.h"

using namespace DigiImage;

// ===========================================================================
// HARDWARE SETTINGS
// ===========================================================================
#define LORA_RX 21
#define LORA_TX 22
#define LORA_UART_BAUD 9600

#define AP_CHANNEL 6
#define RADIO_TURNAROUND_DELAY 300

HardwareSerial Radio(2);
WebServer server(80);
Parser radioParser;
Preferences preferences;

// ===========================================================================
// STATE VARIABLES
// ===========================================================================
enum State { STATE_IDLE, STATE_RX, STATE_TX };
volatile State currentState = STATE_IDLE;

// TX State
uint32_t txImageId = 0;
uint32_t txLength = 0;
uint32_t txCrc = 0;
uint32_t txSent = 0;
uint16_t txW = 0, txH = 0;
uint8_t txFmt = 2, txComp = 1;

// RX State
uint32_t rxImageId = 0;
uint32_t completedRxImageId = 0;
uint32_t rxLength = 0;
uint32_t rxCrc = 0;
uint32_t rxReceived = 0;
uint16_t rxW = 0, rxH = 0;
uint8_t rxFmt = 2, rxComp = 1;
uint8_t sharedBuffer[65536];
uint16_t rxExpectedPackets = 0;
uint8_t rxMask[(65536 / MAX_PAYLOAD) / 8 + 1];

// Text Messaging State
String rxTextMessage = "";
uint32_t rxTextId = 0;

// Remote Camera State
bool remoteImageRequested = false;

// ===========================================================================
// CRC32 CALCULATION
// ===========================================================================
uint32_t crc32Update(uint32_t crc, uint8_t dataByte) {
    crc ^= dataByte;
    for (int bit = 0; bit < 8; bit++) {
        if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
        else crc >>= 1;
    }
    return crc;
}

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32Update(crc, data[i]);
    }
    return crc ^ 0xFFFFFFFF;
}

// ===========================================================================
// TX: RADIO HELPERS
// ===========================================================================
bool waitReply(uint16_t sequenceNumber, uint8_t wantedReply, uint32_t timeout) {
    uint32_t startTime = millis();
    Frame receivedFrame;

    while (millis() - startTime < timeout) {
        while (Radio.available()) {
            if (radioParser.feed(Radio.read(), receivedFrame)) {
                if (receivedFrame.imageId == txImageId && receivedFrame.seq == sequenceNumber) {
                    if (receivedFrame.type == wantedReply) {
                        delay(RADIO_TURNAROUND_DELAY);
                        return true;
                    }
                    if (receivedFrame.type == NACK || receivedFrame.type == IMAGE_FAIL) {
                        return false;
                    }
                }
            }
        }
        delay(1);
    }
    return false;
}

bool waitEndReply(uint16_t sequenceNumber, uint32_t timeout, Frame& outFrame) {
    uint32_t startTime = millis();
    while (millis() - startTime < timeout) {
        while (Radio.available()) {
            if (radioParser.feed(Radio.read(), outFrame)) {
                if (outFrame.imageId == txImageId && outFrame.seq == sequenceNumber) {
                    delay(RADIO_TURNAROUND_DELAY);
                    return true;
                }
            }
        }
        delay(1);
    }
    return false;
}

bool reliableSend(uint8_t packetType, uint16_t sequenceNumber, const uint8_t* packetData, uint16_t packetLength, uint8_t wantedReply = ACK) {
    for (int attempt = 0; attempt < 7; attempt++) {
        sendFrame(Radio, packetType, txImageId, sequenceNumber, packetData, packetLength);
        if (waitReply(sequenceNumber, wantedReply, 2500)) return true;
        delay(150 + (attempt * 75));
    }
    return false;
}

// ===========================================================================
// TX: BACKGROUND SEND TASK
// ===========================================================================
void sendTask(void*) {
    // 1. Send BEGIN packet
    uint8_t metadata[18];
    put16(metadata, txW);
    put16(metadata + 2, txH);
    metadata[4] = txFmt;
    metadata[5] = txComp;
    put32(metadata + 6, txLength);
    put32(metadata + 10, txCrc);
    put32(metadata + 14, txImageId);

    if (!reliableSend(BEGIN, 0xFFFF, metadata, sizeof(metadata))) {
        currentState = STATE_IDLE;
        vTaskDelete(nullptr);
        return;
    }

    // 2. Blast all DATA packets continuously
    uint8_t packetBuffer[MAX_PAYLOAD];
    uint16_t sequenceNumber = 0;
    txSent = 0;

    while (txSent < txLength) {
        size_t bytesLeft = txLength - txSent;
        size_t bytesToRead = min((size_t)MAX_PAYLOAD, bytesLeft);
        
        memcpy(packetBuffer, &sharedBuffer[txSent], bytesToRead);
        sendFrame(Radio, DATA, txImageId, sequenceNumber, packetBuffer, bytesToRead);

        txSent += bytesToRead;
        sequenceNumber++;
        delay(130); // Pacing
    }

    // 3. Send END packet and handle MISSING packets
    bool transferComplete = false;
    int endAttempts = 0;

    while (!transferComplete && endAttempts < 10) {
        endAttempts++;
        sendFrame(Radio, END, txImageId, sequenceNumber, nullptr, 0);

        Frame replyFrame;
        if (waitEndReply(sequenceNumber, 15000, replyFrame)) {
            if (replyFrame.type == ACK) {
                transferComplete = true;
            } 
            else if (replyFrame.type == MISSING) {
                uint16_t missingCount = replyFrame.len / 2;
                for (uint16_t i = 0; i < missingCount; i++) {
                    uint16_t missingSeq = get16(replyFrame.data + (i * 2));
                    if (missingSeq < sequenceNumber) {
                        size_t offset = missingSeq * MAX_PAYLOAD;
                        size_t bytesLeft = txLength - offset;
                        size_t bytesToRead = min((size_t)MAX_PAYLOAD, bytesLeft);
                        
                        memcpy(packetBuffer, &sharedBuffer[offset], bytesToRead);
                        sendFrame(Radio, DATA, txImageId, missingSeq, packetBuffer, bytesToRead);
                        delay(130);
                    }
                }
            }
        }
    }

    currentState = STATE_IDLE;
    vTaskDelete(nullptr);
}

// ===========================================================================
// RX: LORA RECEIVE LOOP
// ===========================================================================
void doReceive() {
    static uint32_t lastReceiveTime = 0;
    
    if (currentState == STATE_RX && (millis() - lastReceiveTime > 60000)) {
        currentState = STATE_IDLE; // Timeout
    }

    Frame rxFrame;
    while (Radio.available()) {
        if (radioParser.feed(Radio.read(), rxFrame)) {
            lastReceiveTime = millis();
            
            if (rxFrame.type == REQUEST_IMAGE) {
                if (currentState == STATE_TX) continue;
                remoteImageRequested = true;
                delay(RADIO_TURNAROUND_DELAY);
                sendFrame(Radio, ACK, rxFrame.imageId, rxFrame.seq, nullptr, 0);

            } else if (rxFrame.type == TEXT_MSG) {
                if (currentState == STATE_TX) continue;
                
                String newText = "";
                for (int i = 0; i < rxFrame.len; i++) {
                    newText += (char)rxFrame.data[i];
                }
                
                // Only update if it's a new text ID
                if (rxFrame.imageId != rxTextId) {
                    rxTextMessage = newText;
                    rxTextId = rxFrame.imageId;
                }
                
                delay(RADIO_TURNAROUND_DELAY);
                sendFrame(Radio, ACK, rxFrame.imageId, rxFrame.seq, nullptr, 0);

            } else if (rxFrame.type == BEGIN) {
                if (currentState == STATE_TX) continue; // Ignore if we are transmitting

                rxW = get16(rxFrame.data);
                rxH = get16(rxFrame.data + 2);
                rxFmt = rxFrame.data[4];
                rxComp = rxFrame.data[5];
                rxLength = get32(rxFrame.data + 6);
                rxCrc = get32(rxFrame.data + 10);
                
                // Using the txImageId field (offset 14 in metadata array) for rxImageId
                uint32_t newRxId = get32(rxFrame.data + 14);
                
                if (rxLength > sizeof(sharedBuffer)) continue;

                rxImageId = newRxId;
                rxExpectedPackets = (rxLength + MAX_PAYLOAD - 1) / MAX_PAYLOAD;
                memset(rxMask, 0, sizeof(rxMask));
                rxReceived = 0;
                
                currentState = STATE_RX;
                sendFrame(Radio, ACK, rxImageId, 0xFFFF, nullptr, 0); // Acknowledge BEGIN

            } else if (currentState == STATE_RX && rxFrame.imageId == rxImageId) {
                if (rxFrame.type == DATA) {
                    uint16_t seq = rxFrame.seq;
                    if (seq < rxExpectedPackets) {
                        uint8_t byteIdx = seq / 8;
                        uint8_t bitMask = 1 << (seq % 8);
                        
                        if ((rxMask[byteIdx] & bitMask) == 0) {
                            rxMask[byteIdx] |= bitMask;
                            rxReceived += rxFrame.len;
                            memcpy(&sharedBuffer[seq * MAX_PAYLOAD], rxFrame.data, rxFrame.len);
                        }
                    }
                } else if (rxFrame.type == END) {
                    uint16_t missingCount = 0;
                    uint8_t missingPayload[255];
                    
                    for (uint16_t i = 0; i < rxExpectedPackets; i++) {
                        if ((rxMask[i / 8] & (1 << (i % 8))) == 0) {
                            if (missingCount < 48) { 
                                put16(missingPayload + (missingCount * 2), i);
                                missingCount++;
                            }
                        }
                    }
                    
                    delay(RADIO_TURNAROUND_DELAY);

                    if (missingCount > 0) {
                        sendFrame(Radio, MISSING, rxImageId, rxFrame.seq, missingPayload, missingCount * 2);
                    } else {
                        sendFrame(Radio, ACK, rxImageId, rxFrame.seq, nullptr, 0);
                        completedRxImageId = rxImageId;
                        currentState = STATE_IDLE; // Successfully received into sharedBuffer!
                    }
                }
            }
        }
    }
}

// ===========================================================================
// WEB ENDPOINTS
// ===========================================================================
void handleRoot() {
    server.send(200, "text/html", WEB_APP_HTML);
}

void handleStatus() {
    String json = "{";
    if (currentState == STATE_TX) {
        json += "\"mode\":\"tx\", \"sent\":" + String(txSent) + ", \"total\":" + String(txLength) + ",";
    } else if (currentState == STATE_RX) {
        json += "\"mode\":\"rx\", \"sent\":" + String(rxReceived) + ", \"total\":" + String(rxLength) + ",";
    } else {
        json += "\"mode\":\"idle\",";
    }
    json += "\"rxImageId\":" + String(completedRxImageId) + ",";
    
    String escapedText = rxTextMessage;
    escapedText.replace("\\", "\\\\");
    escapedText.replace("\"", "\\\"");
    escapedText.replace("\n", "\\n");
    escapedText.replace("\r", "\\r");
    
    json += "\"rxTextId\":" + String(rxTextId) + ",";
    json += "\"rxText\":\"" + escapedText + "\",";
    json += "\"imageRequested\":" + String(remoteImageRequested ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

void handleUploadData() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (currentState != STATE_IDLE) return;
        txLength = 0;
        rxLength = 0;
        completedRxImageId = 0; // Clear the incoming photo since we are overwriting the shared buffer
        remoteImageRequested = false; // Reset request flag since we are fulfilling it
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (currentState != STATE_IDLE) return;
        if (txLength + upload.currentSize > sizeof(sharedBuffer)) return;
        memcpy(&sharedBuffer[txLength], upload.buf, upload.currentSize);
        txLength += upload.currentSize;
    } else if (upload.status == UPLOAD_FILE_END) {
        if (currentState != STATE_IDLE) return;
        txCrc = crc32(sharedBuffer, txLength);
        txImageId = millis();
    }
}

void handleUpload() {
    if (currentState != STATE_IDLE) {
        server.send(400, "text/plain", "Busy");
        return;
    }
    
    if (txLength == 0 || txLength > sizeof(sharedBuffer)) {
        server.send(400, "text/plain", "Upload failed or too large");
        return;
    }

    txW = server.header("X-Width").toInt();
    txH = server.header("X-Height").toInt();
    
    currentState = STATE_TX;
    server.send(200, "text/plain", "OK");

    xTaskCreatePinnedToCore(sendTask, "sendImage", 8192, nullptr, 1, nullptr, 0);
}

void handleRxImage() {
    if (rxLength == 0) {
        server.send(404, "text/plain", "No image");
        return;
    }
    
    String mimeType = (rxFmt == 2) ? "image/jpeg" : "application/octet-stream";
    server.send_P(200, mimeType.c_str(), (const char*)sharedBuffer, rxLength);
}

void handleRename() {
    if (server.hasArg("name")) {
        String newName = server.arg("name");
        preferences.begin("wifi", false);
        preferences.putString("ssid", newName);
        preferences.end();
        server.send(200, "text/plain", "OK. Rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing name");
    }
}

void handleSendText() {
    if (currentState != STATE_IDLE) {
        server.send(400, "text/plain", "Busy");
        return;
    }
    if (server.hasArg("text")) {
        String msg = server.arg("text");
        if (msg.length() > MAX_PAYLOAD) msg = msg.substring(0, MAX_PAYLOAD);
        
        txImageId = millis(); // Use millis as a unique text ID
        currentState = STATE_TX;
        
        bool success = reliableSend(TEXT_MSG, 0, (const uint8_t*)msg.c_str(), msg.length(), ACK);
        
        currentState = STATE_IDLE;
        
        if (success) {
            server.send(200, "text/plain", "OK");
        } else {
            server.send(500, "text/plain", "No ACK");
        }
    } else {
        server.send(400, "text/plain", "No text provided");
    }
}

void handleRequestImage() {
    if (currentState != STATE_IDLE) {
        server.send(400, "text/plain", "Busy");
        return;
    }
    currentState = STATE_TX;
    // Send a blank REQUEST_IMAGE frame
    bool success = reliableSend(REQUEST_IMAGE, 0, nullptr, 0, ACK);
    currentState = STATE_IDLE;
    
    if (success) {
        server.send(200, "text/plain", "OK");
    } else {
        server.send(500, "text/plain", "No ACK");
    }
}

// ===========================================================================
// SETUP
// ===========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n--- UNIFIED LORA TRANSCEIVER ---");
    
    Radio.begin(38400, SERIAL_8N1, LORA_RX, LORA_TX);
    
    // Generate default AP name based on MAC address
    uint64_t mac = ESP.getEfuseMac();
    char defaultAp[32];
    sprintf(defaultAp, "LoRa_Chat_%04X", (uint16_t)(mac >> 32));

    preferences.begin("wifi", true);
    String apName = preferences.getString("ssid", defaultAp);
    preferences.end();

    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.softAP(apName.c_str(), "12345678", AP_CHANNEL);
    
    Serial.println(String("\nWi-Fi AP Started: ") + apName + " / 12345678 / Channel " + AP_CHANNEL);

    const char * headerkeys[] = {"X-Width", "X-Height", "Content-Length"};
    size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
    server.collectHeaders(headerkeys, headerkeyssize);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/upload", HTTP_POST, handleUpload, handleUploadData);
    server.on("/rx_image.bin", HTTP_GET, handleRxImage);
    server.on("/rename", HTTP_POST, handleRename);
    server.on("/send_text", HTTP_POST, handleSendText);
    server.on("/request_image", HTTP_POST, handleRequestImage);
    server.begin();
}

void loop() {
    server.handleClient();
    if (currentState != STATE_TX) {
        doReceive();
    }
}