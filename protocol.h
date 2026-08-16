#pragma once

#include <Arduino.h>


namespace DigiImage {


// ###########################################################################
// SHARED RF PROTOCOL CONSTANTS AND FRAME TYPES
// This file must be identical on the transmitter and receiver.
// ###########################################################################

constexpr uint8_t MAGIC1 = 0xD1;
constexpr uint8_t MAGIC2 = 0x47;
constexpr uint8_t VERSION = 1;

constexpr size_t MAX_PAYLOAD = 96;


enum Type : uint8_t {
  BEGIN = 1,
  DATA = 2,
  END = 3,
  ACK = 4,
  NACK = 5,
  IMAGE_OK = 6,
  IMAGE_FAIL = 7,
  PING = 8,
  PONG = 9,
  MISSING = 10,
  ABORT = 11,
  REQUEST_IMAGE = 12,
  TEXT_MSG = 13
};


struct Frame {
  uint8_t type = 0;
  uint32_t imageId = 0;
  uint16_t seq = 0;
  uint16_t len = 0;

  uint8_t data[MAX_PAYLOAD];
};


// ###########################################################################
// CRC16 CALCULATION
// Detects damaged or incomplete radio packets.
// ###########################################################################

inline uint16_t crc16Update(
  uint16_t crc,
  uint8_t dataByte
) {
  crc ^= (uint16_t)dataByte << 8;

  for (int bit = 0; bit < 8; bit++) {
    if (crc & 0x8000) {
      crc = (crc << 1) ^ 0x1021;
    } else {
      crc <<= 1;
    }
  }

  return crc;
}


inline uint16_t crc16(
  const uint8_t* data,
  size_t length
) {
  uint16_t crc = 0xFFFF;

  while (length--) {
    crc = crc16Update(
      crc,
      *data++
    );
  }

  return crc;
}


// ###########################################################################
// BIG-ENDIAN INTEGER HELPERS
// Ensures both ESP32 boards encode numbers in the same byte order.
// ###########################################################################

inline void put16(
  uint8_t* destination,
  uint16_t value
) {
  destination[0] =
    value >> 8;

  destination[1] =
    value;
}


inline void put32(
  uint8_t* destination,
  uint32_t value
) {
  destination[0] =
    value >> 24;

  destination[1] =
    value >> 16;

  destination[2] =
    value >> 8;

  destination[3] =
    value;
}


inline uint16_t get16(
  const uint8_t* source
) {
  return
    ((uint16_t)source[0] << 8) |
    source[1];
}


inline uint32_t get32(
  const uint8_t* source
) {
  return
    ((uint32_t)source[0] << 24) |
    ((uint32_t)source[1] << 16) |
    ((uint32_t)source[2] << 8) |
    source[3];
}


// ###########################################################################
// BINARY FRAME ENCODER
// Adds synchronization bytes, metadata, payload, and CRC.
// ###########################################################################

inline void sendFrame(
  Stream& output,
  uint8_t type,
  uint32_t imageId,
  uint16_t sequenceNumber,
  const uint8_t* payload,
  uint16_t payloadLength
) {
  if (payloadLength > MAX_PAYLOAD) {
    return;
  }

  uint8_t header[12] = {
    MAGIC1,
    MAGIC2,
    VERSION,
    type
  };

  put32(
    header + 4,
    imageId
  );

  put16(
    header + 8,
    sequenceNumber
  );

  put16(
    header + 10,
    payloadLength
  );

  uint16_t calculatedCrc =
    crc16(
      header,
      sizeof(header)
    );

  for (
    uint16_t index = 0;
    index < payloadLength;
    index++
  ) {
    calculatedCrc =
      crc16Update(
        calculatedCrc,
        payload[index]
      );
  }

  output.write(
    header,
    sizeof(header)
  );

  if (payloadLength > 0) {
    output.write(
      payload,
      payloadLength
    );
  }

  uint8_t crcBytes[2];

  put16(
    crcBytes,
    calculatedCrc
  );

  output.write(
    crcBytes,
    2
  );

  output.flush();
}


// ###########################################################################
// STREAMING FRAME DECODER
// Reassembles individual UART bytes into complete validated frames.
// ###########################################################################

class Parser {
  enum State {
    M1,
    M2,
    HEADER,
    PAYLOAD,
    CRC
  };

  State state = M1;

  uint8_t header[10];

  uint8_t headerIndex = 0;
  uint8_t payloadIndex = 0;
  uint8_t crcIndex = 0;

  uint8_t receivedCrc[2];

  Frame frame;


public:

  bool feed(
    uint8_t incomingByte,
    Frame& outputFrame
  ) {

    // -----------------------------------------------------------------------
    // WAIT FOR FIRST MAGIC BYTE
    // -----------------------------------------------------------------------

    if (state == M1) {
      if (incomingByte == MAGIC1) {
        state = M2;
      }

      return false;
    }


    // -----------------------------------------------------------------------
    // WAIT FOR SECOND MAGIC BYTE
    // -----------------------------------------------------------------------

    if (state == M2) {
      if (incomingByte == MAGIC2) {
        state = HEADER;
      } else {
        state = M1;
      }

      headerIndex = 0;

      return false;
    }


    // -----------------------------------------------------------------------
    // READ THE REMAINING HEADER BYTES
    // -----------------------------------------------------------------------

    if (state == HEADER) {
      header[headerIndex++] =
        incomingByte;

      if (headerIndex < 10) {
        return false;
      }

      if (header[0] != VERSION) {
        state = M1;
        return false;
      }

      frame.type =
        header[1];

      frame.imageId =
        get32(header + 2);

      frame.seq =
        get16(header + 6);

      frame.len =
        get16(header + 8);

      if (frame.len > MAX_PAYLOAD) {
        state = M1;
        return false;
      }


      // Critical fix:
      // Reset both counters for every new frame, including ACK frames
      // with zero-length payloads.

      payloadIndex = 0;
      crcIndex = 0;

      if (frame.len > 0) {
        state = PAYLOAD;
      } else {
        state = CRC;
      }

      return false;
    }


    // -----------------------------------------------------------------------
    // READ FRAME PAYLOAD
    // -----------------------------------------------------------------------

    if (state == PAYLOAD) {
      frame.data[payloadIndex++] =
        incomingByte;

      if (payloadIndex == frame.len) {
        crcIndex = 0;
        state = CRC;
      }

      return false;
    }


    // -----------------------------------------------------------------------
    // READ AND VERIFY FRAME CRC
    // -----------------------------------------------------------------------

    receivedCrc[crcIndex++] =
      incomingByte;

    if (crcIndex < 2) {
      return false;
    }

    uint8_t completeHeader[12] = {
      MAGIC1,
      MAGIC2
    };

    memcpy(
      completeHeader + 2,
      header,
      10
    );

    uint16_t calculatedCrc =
      crc16(
        completeHeader,
        12
      );

    for (
      uint16_t index = 0;
      index < frame.len;
      index++
    ) {
      calculatedCrc =
        crc16Update(
          calculatedCrc,
          frame.data[index]
        );
    }

    uint16_t expectedCrc =
      get16(receivedCrc);

    state = M1;

    if (
      calculatedCrc != expectedCrc
    ) {
      return false;
    }

    outputFrame = frame;

    return true;
  }
};


}