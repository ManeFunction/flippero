// SPDX-License-Identifier: GPL-3.0-or-later
// Cross-check: the bridge's C++ framing must interoperate byte-for-byte with
// the Flipper's C framing (core/uart_frame.c).
#include "../../../bridge/src/bridge_frame.h"

extern "C" {
#include "../../core/uart_frame.h"
}

#include <cstdio>
#include <initializer_list>
#include <cstring>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
    static_assert(frame::MAX_PAYLOAD == UART_FRAME_MAX_PAYLOAD, "payload size mismatch");
    static_assert(frame::WRITE == FRAME_WRITE && frame::NOTIFY == FRAME_NOTIFY &&
                  frame::STATE == FRAME_STATE && frame::HELLO_ACK == FRAME_HELLO_ACK &&
                  frame::WRITE_ACK == FRAME_WRITE_ACK && frame::CONNECT == FRAME_CONNECT &&
                  frame::DISCONNECT == FRAME_DISCONNECT && frame::HELLO == FRAME_HELLO,
                  "frame type mismatch");
    static_assert((int)frame::Idle == (int)BridgeStateIdle && (int)frame::Scanning == (int)BridgeStateScanning &&
                  (int)frame::Connecting == (int)BridgeStateConnecting &&
                  (int)frame::Connected == (int)BridgeStateConnected &&
                  (int)frame::Disconnected == (int)BridgeStateDisconnected &&
                  (int)frame::NotFound == (int)BridgeStateNotFound && (int)frame::Error == (int)BridgeStateError,
                  "state mismatch");

    uint8_t payload[200];
    for (int i = 0; i < 200; i++) payload[i] = (uint8_t)(i * 31 + 5);

    for (size_t len : {0u, 1u, 2u, 17u, 199u, 200u}) {
        uint8_t a[frame::MAX_FRAME], b[UART_FRAME_MAX_LEN];

        // identical bytes on the wire
        size_t na = frame::encode(a, frame::WRITE, payload, len);
        size_t nb = uart_frame_encode(b, FRAME_WRITE, payload, (uint16_t)len);
        CHECK(na == nb && !memcmp(a, b, na));

        // C encoder -> C++ parser
        frame::Parser p;
        int got = 0;
        for (size_t i = 0; i < nb; i++) got += p.feed(b[i]);
        CHECK(got == 1 && p.type == frame::WRITE && p.len == len && !memcmp(p.payload, payload, len));

        // C++ encoder -> C parser
        UartFrameParser cp;
        uart_frame_parser_reset(&cp);
        got = 0;
        for (size_t i = 0; i < na; i++) got += uart_frame_parser_feed(&cp, a[i]);
        CHECK(got == 1 && cp.frame.len == len && !memcmp(cp.frame.payload, payload, len));
    }

    // corrupt CRC rejected by the C++ parser
    uint8_t a[frame::MAX_FRAME];
    size_t n = frame::encode(a, frame::NOTIFY, payload, 4);
    a[5] ^= 1;
    frame::Parser p;
    int got = 0;
    for (size_t i = 0; i < n; i++) got += p.feed(a[i]);
    CHECK(got == 0);

    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("bridge framing interop OK\n");
    return 0;
}
