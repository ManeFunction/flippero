// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Flipper <-> ESP32 bridge framing. See docs/UART_PROTOCOL.md.
 *
 *   0xA5 | type | len_lo | len_hi | payload[len] | crc8
 *
 * crc8 is CRC-8 (poly 0x07, init 0) over type, len and payload. */

#define UART_FRAME_SOF         0xA5
#define UART_FRAME_MAX_PAYLOAD 200
#define UART_FRAME_OVERHEAD    5
#define UART_FRAME_MAX_LEN     (UART_FRAME_MAX_PAYLOAD + UART_FRAME_OVERHEAD)
#define UART_PROTO_VERSION     1

/* Flipper -> bridge */
#define FRAME_HELLO      0x01 /* payload: none */
#define FRAME_CONNECT    0x02 /* payload: name prefix (ASCII, may be empty) */
#define FRAME_DISCONNECT 0x03
#define FRAME_WRITE      0x04 /* payload: bytes for one GATT write (no response) */

/* Bridge -> Flipper */
#define FRAME_HELLO_ACK 0x81 /* payload: version(1) */
#define FRAME_STATE     0x82 /* payload: state(1) mtu_lo mtu_hi name... */
#define FRAME_WRITE_ACK 0x84 /* payload: status(1), 0 = ok */
#define FRAME_NOTIFY    0x85 /* payload: bytes notified by the printer */

typedef enum {
    BridgeStateIdle = 0,
    BridgeStateScanning = 1,
    BridgeStateConnecting = 2,
    BridgeStateConnected = 3,
    BridgeStateDisconnected = 4,
    BridgeStateNotFound = 5,
    BridgeStateError = 6,
} BridgeState;

uint8_t uart_frame_crc8(const uint8_t* data, size_t len);

/* Returns total frame length written to `out` (>= UART_FRAME_MAX_LEN bytes). */
size_t uart_frame_encode(uint8_t* out, uint8_t type, const uint8_t* payload, uint16_t len);

typedef struct {
    uint8_t type;
    uint16_t len;
    uint8_t payload[UART_FRAME_MAX_PAYLOAD];
} UartFrame;

typedef struct {
    enum { ParseSof, ParseType, ParseLenLo, ParseLenHi, ParsePayload, ParseCrc } stage;
    uint8_t crc;
    uint16_t got;
    UartFrame frame;
} UartFrameParser;

void uart_frame_parser_reset(UartFrameParser* p);

/* Feed one byte. Returns true when a complete, CRC-valid frame is in
 * p->frame (valid until the next call). Garbage and bad frames resync on
 * the next SOF. */
bool uart_frame_parser_feed(UartFrameParser* p, uint8_t byte);
