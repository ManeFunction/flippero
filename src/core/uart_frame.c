// SPDX-License-Identifier: GPL-3.0-or-later
#include "uart_frame.h"

#include <string.h>

static uint8_t crc_step(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for(int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    return crc;
}

uint8_t uart_frame_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for(size_t i = 0; i < len; i++) crc = crc_step(crc, data[i]);
    return crc;
}

size_t uart_frame_encode(uint8_t* out, uint8_t type, const uint8_t* payload, uint16_t len) {
    out[0] = UART_FRAME_SOF;
    out[1] = type;
    out[2] = len & 0xFF;
    out[3] = len >> 8;
    if(len) memcpy(&out[4], payload, len);
    out[4 + len] = uart_frame_crc8(&out[1], 3 + len);
    return 5 + (size_t)len;
}

void uart_frame_parser_reset(UartFrameParser* p) {
    p->stage = ParseSof;
    p->crc = 0;
    p->got = 0;
}

bool uart_frame_parser_feed(UartFrameParser* p, uint8_t byte) {
    switch(p->stage) {
    case ParseSof:
        if(byte == UART_FRAME_SOF) {
            p->crc = 0;
            p->stage = ParseType;
        }
        break;
    case ParseType:
        p->frame.type = byte;
        p->crc = crc_step(0, byte);
        p->stage = ParseLenLo;
        break;
    case ParseLenLo:
        p->frame.len = byte;
        p->crc = crc_step(p->crc, byte);
        p->stage = ParseLenHi;
        break;
    case ParseLenHi:
        p->frame.len |= (uint16_t)byte << 8;
        p->crc = crc_step(p->crc, byte);
        if(p->frame.len > UART_FRAME_MAX_PAYLOAD) {
            uart_frame_parser_reset(p);
        } else {
            p->got = 0;
            p->stage = p->frame.len ? ParsePayload : ParseCrc;
        }
        break;
    case ParsePayload:
        p->frame.payload[p->got++] = byte;
        p->crc = crc_step(p->crc, byte);
        if(p->got == p->frame.len) p->stage = ParseCrc;
        break;
    case ParseCrc: {
        bool ok = (byte == p->crc);
        uart_frame_parser_reset(p);
        return ok;
    }
    }
    return false;
}
