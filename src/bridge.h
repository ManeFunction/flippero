// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "link.h"

/* Blocking client for the ESP32 UART<->BLE bridge (see docs/UART_PROTOCOL.md);
 * exposed to the print job as link_uart_ops (link.h).
 * Creating a Bridge takes over the Flipper's GPIO UART (pins 13/14) and
 * suspends the expansion-module service; freeing it hands both back. */

typedef struct Bridge Bridge;

/* Called from the bridge's RX thread whenever the bridge reports a state. */
typedef void (*BridgeStateCallback)(BridgeState state, void* context);

Bridge* bridge_alloc(void);
void bridge_free(Bridge* bridge);

void bridge_set_state_callback(Bridge* bridge, BridgeStateCallback cb, void* context);

/* Make any in-flight or future wait return LinkCancelled (thread-safe). */
void bridge_cancel(Bridge* bridge);
void bridge_clear_cancel(Bridge* bridge);

/* Ping the ESP32. */
LinkResult bridge_hello(Bridge* bridge, uint32_t timeout_ms);

/* Scan for a printer whose BLE name starts with `name_prefix` and connect.
 * On LinkFailed, *state_out says why (BridgeStateNotFound / Error / ...). */
LinkResult bridge_connect(
    Bridge* bridge,
    const char* name_prefix,
    uint32_t timeout_ms,
    BridgeState* state_out);
void bridge_disconnect(Bridge* bridge);

const char* bridge_peer_name(Bridge* bridge); /* valid after connect */

/* Write to the printer's GATT write characteristic, split into MTU-sized
 * chunks with a short pause between them. */
LinkResult bridge_write(Bridge* bridge, const uint8_t* data, size_t len);

/* Wait for one notification from the printer. *len is capacity in, size out. */
LinkResult
    bridge_wait_notify(Bridge* bridge, uint8_t* buf, size_t* len, uint32_t timeout_ms);
void bridge_drain_notify(Bridge* bridge);
