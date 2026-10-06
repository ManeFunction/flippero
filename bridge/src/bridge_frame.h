// SPDX-License-Identifier: GPL-3.0-or-later
// Flipper <-> ESP32 UART framing (see docs/UART_PROTOCOL.md).
// Header-only and free of Arduino dependencies so it can be tested on a host
// against src/core/uart_frame.c (see tests/host).
//
//   0xA5 | type | len_lo | len_hi | payload[len] | crc8(type,len,payload)
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace frame {

constexpr uint8_t SOF = 0xA5;
constexpr size_t MAX_PAYLOAD = 200;
constexpr size_t MAX_FRAME = MAX_PAYLOAD + 5;
constexpr uint8_t PROTO_VERSION = 1;

// Flipper -> bridge
constexpr uint8_t HELLO = 0x01;
constexpr uint8_t CONNECT = 0x02;
constexpr uint8_t DISCONNECT = 0x03;
constexpr uint8_t WRITE = 0x04;
// Bridge -> Flipper
constexpr uint8_t HELLO_ACK = 0x81;
constexpr uint8_t STATE = 0x82;
constexpr uint8_t WRITE_ACK = 0x84;
constexpr uint8_t NOTIFY = 0x85;

enum State : uint8_t {
    Idle = 0,
    Scanning = 1,
    Connecting = 2,
    Connected = 3,
    Disconnected = 4,
    NotFound = 5,
    Error = 6,
};

inline uint8_t crc8Step(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    return crc;
}

// `out` must hold at least len + 5 bytes. Returns the frame length.
inline size_t encode(uint8_t* out, uint8_t type, const uint8_t* payload, size_t len) {
    out[0] = SOF;
    out[1] = type;
    out[2] = len & 0xFF;
    out[3] = (len >> 8) & 0xFF;
    uint8_t crc = 0;
    crc = crc8Step(crc, out[1]);
    crc = crc8Step(crc, out[2]);
    crc = crc8Step(crc, out[3]);
    for (size_t i = 0; i < len; i++) {
        out[4 + i] = payload[i];
        crc = crc8Step(crc, payload[i]);
    }
    out[4 + len] = crc;
    return len + 5;
}

class Parser {
   public:
    uint8_t type = 0;
    uint16_t len = 0;
    uint8_t payload[MAX_PAYLOAD];

    // Feed one byte; true when a complete CRC-valid frame is available.
    bool feed(uint8_t b) {
        switch (stage_) {
            case Sof:
                if (b == SOF) stage_ = Type;
                break;
            case Type:
                type = b;
                crc_ = crc8Step(0, b);
                stage_ = LenLo;
                break;
            case LenLo:
                len = b;
                crc_ = crc8Step(crc_, b);
                stage_ = LenHi;
                break;
            case LenHi:
                len |= (uint16_t)b << 8;
                crc_ = crc8Step(crc_, b);
                got_ = 0;
                if (len > MAX_PAYLOAD) stage_ = Sof;
                else stage_ = len ? Payload : Crc;
                break;
            case Payload:
                payload[got_++] = b;
                crc_ = crc8Step(crc_, b);
                if (got_ == len) stage_ = Crc;
                break;
            case Crc:
                stage_ = Sof;
                return b == crc_;
        }
        return false;
    }

   private:
    enum Stage { Sof, Type, LenLo, LenHi, Payload, Crc } stage_ = Sof;
    uint8_t crc_ = 0;
    uint16_t got_ = 0;
};

}  // namespace frame
