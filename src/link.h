// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/uart_frame.h"

#include <furi.h>

/* Transport between the print job and the printer. Two backends:
 *   - link_uart_ops: Flipper UART -> ESP32 bridge -> BLE   (bridge.c, works on stock firmware)
 *   - link_ble_ops:  Flipper's own radio as BLE central    (ble_link.c, needs Blue++ firmware,
 *                    built only with FICHERO_DIRECT_BLE)
 * The print job only talks to this interface, so it is identical for both. */

typedef enum {
    LinkOk,
    LinkTimeout,
    LinkCancelled,
    LinkFailed, /* link reported an error / printer not found / write failed */
} LinkResult;

/* Same values as the UART protocol's state byte. */
typedef BridgeState LinkState;

typedef struct Link Link; /* opaque, owned by the backend */

typedef void (*LinkStateCallback)(LinkState state, void* context);

typedef enum {
    LinkKindUart, /* ESP32 bridge */
    LinkKindBle, /* direct BLE (Blue++ firmware) */
    LinkKindCount,
} LinkKind;

typedef struct {
    const char* name;

    /* NULL if the hardware can't be taken (UART busy / no full BLE stack). */
    Link* (*alloc)(void);
    void (*free)(Link* link);

    void (*set_state_callback)(Link* link, LinkStateCallback cb, void* context);
    void (*cancel)(Link* link); /* make any in-flight or future wait return LinkCancelled */
    void (*clear_cancel)(Link* link);

    /* Check the transport is alive (ping the ESP32). May be called repeatedly. */
    LinkResult (*ready)(Link* link, uint32_t timeout_ms);

    /* Find a printer whose BLE name starts with `name_prefix` and connect. */
    LinkResult (*connect)(Link* link, const char* name_prefix, uint32_t timeout_ms, LinkState* state_out);

    /* Write to the printer's characteristic, chunked to the MTU with pauses. */
    LinkResult (*write)(Link* link, const uint8_t* data, size_t len);

    /* Wait for one printer notification. *len is capacity in, size out. */
    LinkResult (*wait_notify)(Link* link, uint8_t* buf, size_t* len, uint32_t timeout_ms);
    void (*drain_notify)(Link* link);

    const char* (*peer_name)(Link* link); /* valid after connect */
} LinkOps;

extern const LinkOps link_uart_ops;
#ifdef FICHERO_DIRECT_BLE
extern const LinkOps link_ble_ops;
#endif

/* Direct BLE is only available in builds made against the Blue++ SDK. */
static inline bool link_kind_available(LinkKind kind) {
#ifdef FICHERO_DIRECT_BLE
    return kind == LinkKindUart || kind == LinkKindBle;
#else
    return kind == LinkKindUart;
#endif
}

static inline const LinkOps* link_ops_for(LinkKind kind) {
#ifdef FICHERO_DIRECT_BLE
    if(kind == LinkKindBle) return &link_ble_ops;
#endif
    (void)kind;
    return &link_uart_ops;
}
