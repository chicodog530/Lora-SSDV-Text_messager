# LoRa Digital Image, Text & Remote Camera Transceiver

An off-grid, two-way communicator and remote camera system built with ESP32 and UART LoRa modules. This project turns two ESP32s into autonomous walkie-talkies that serve their own Wi-Fi web app to your smartphone, allowing you to send photos, text messages, and remotely trigger the camera on another node over miles of distance without any internet or cellular connection.

## Features
- **Completely Off-Grid:** Generates its own Wi-Fi Access Point. No router or internet required.
- **Two-Way Communication:** Both nodes can send and receive images and text seamlessly.
- **Remote Camera Mode:** Leave a spare Android phone connected to one node. From your primary phone, remotely request a photo. The spare phone will silently auto-snap a photo and beam it back to you instantly.
- **High Resolution:** Supports sending JPEGs up to 800x600 resolution.
- **Fast Mode:** Instantly downscales images to 400x300 for rapid transmission.
- **Retro 2-Bit Grayscale:** Applies a custom 4-color Gameboy-style filter to images for massive compression and an awesome aesthetic.
- **Infinite Text Messaging:** Send massive text messages. The web app automatically slices them into 96-byte "LoRa Tweets" and orchestrates delivery in the background.
- **Reliable Custom Protocol:** Built-in packet fragmentation, CRC32 checksums, ACKs, and missing-packet recovery to ensure your files arrive uncorrupted.

## Parts List
To build a complete two-way system, you need two of everything:
1. **2x ESP32 Development Boards** (Standard ESP32, ESP32-CAM, etc.)
2. **2x UART LoRa Modules**: Specifically the **Loongtrek SX1262 DX-LR32** (433~470MHz or 868/915MHz depending on your region, 22dBm, LR32-900T22D(I)).
3. **2x Antennas** (Usually included with the LoRa modules)
4. Jumper wires and USB power banks.

## Wiring Guide
The Loongtrek SX1262 UART module connects easily to the ESP32.

| LoRa Module | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **VCC** | **5V / VIN** | Module requires 5V power. |
| **GND** | **GND** | Common ground. |
| **TX** | **Pin 21** | ESP32 RX receives from LoRa TX. |
| **RX** | **Pin 22** | ESP32 TX transmits to LoRa RX. |

*(Note: Depending on your specific ESP32 board, you can change the RX/TX pins at the top of the `transceiver_v3.ino` sketch).*

## Setup Instructions
1. **Wire it up:** Connect the LoRa modules to your ESP32s according to the wiring guide above. Don't forget to attach the antennas!
2. **Flash the Firmware:**
   - Open `transceiver_v3.ino` in the Arduino IDE.
   - Select your ESP32 board in the tools menu.
   - Compile and upload the sketch to **both** of your ESP32 boards.
3. **Power On:** Provide power to both boards via USB or battery.
4. **Connect to Wi-Fi:** 
   - On your smartphone, look for a new Wi-Fi network called `LoRa_Chat_XXXX`.
   - Connect to it using the default password: `12345678`.
5. **Open the Web App:**
   - Open your smartphone's Chrome/Safari browser.
   - Navigate to `http://192.168.4.1`.
6. **Start Chatting:** Use the web interface to type text messages, select photos from your camera roll, apply filters, and send them over the airwaves!

## How to use Remote Camera Mode
To use the remote camera feature, you need a dedicated "Camera Node" (e.g. a spare Android phone left on a counter) and a "Remote Node" (your daily phone).
1. On the spare Android phone, connect to the ESP32 Wi-Fi and open `http://192.168.4.1` in Chrome.
2. In Chrome, go to `chrome://flags`, search for "Insecure origins treated as secure", add `http://192.168.4.1`, and enable it. (This is required because Chrome blocks camera access on non-HTTPS websites).
3. Relaunch Chrome, go back to the chat page, and check the **"Enable Remote Camera Mode"** box.
4. Plug the phone into a charger and use Android's Developer Options to turn on "Stay Awake" so the screen never turns off.
5. On your daily phone (connected to the other ESP32), click **"Request Remote Photo"**. The spare phone will automatically take a picture and beam it back to you over LoRa!

## Protocol Details
This project implements a highly specialized UDP/TCP-hybrid application layer over the raw LoRa UART serial stream. It slices large JPEGs into chunks, tags them with sequence numbers and CRC32 checksums, and uses a `MISSING` frame packet mechanism to ask the transmitter to re-send only the specific chunks that were lost to radio interference, drastically reducing transmission times for large files.
