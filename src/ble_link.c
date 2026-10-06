// SPDX-License-Identifier: GPL-3.0-or-later
//
// Direct-BLE backend for the print job: the Flipper's own radio acts as the BLE
// central and talks to the printer, no ESP32 needed. Requires the Flipper Blue++
// firmware (full BLE stack + exported ST command API) and an SDK built from it;
// see README. Compiled only with FICHERO_DIRECT_BLE.

#ifdef FICHERO_DIRECT_BLE

#include "link.h"
#include "third_party/bluepp/ble_central.h"

#include <furi_hal.h>

#define TAG "BleLink"

#define WRITE_CHUNK 200 /* matches the 200 B chunks the reference driver uses */
#define WRITE_PAUSE 20 /* ms between chunks */
#define WRITE_TRIES 5 /* write-without-response has no flow control; retry if the stack's TX queue is full */
#define CONNECT_MS  8000
#define CONNECT_ATTEMPTS 3
#define CONNECT_RETRY_MS 2000
#define NOTIFY_MAX  32

/* GATT characteristic property bits */
#define PROP_WRITE_NO_RESP 0x04
#define PROP_NOTIFY        0x10
#define PROP_INDICATE      0x20

typedef enum {
    EvtCancel = (1 << 0),
    EvtNotify = (1 << 1),
} BleEvt;

typedef struct {
    uint8_t len;
    uint8_t data[NOTIFY_MAX];
} NotifyMsg;

struct Link {
    BleCentral* bc;
    bool was_advertising;
    FuriEventFlag* flags;
    FuriMessageQueue* notify_q;
    LinkStateCallback state_cb;
    void* state_cb_ctx;

    uint16_t write_handle;
    bool write_with_resp;
    uint16_t notify_handle;
    char peer_name[BC_NAME_MAX + 1];
};

static bool cancelled(Link* l) {
    return furi_event_flag_get(l->flags) & EvtCancel;
}

static void report(Link* l, LinkState s) {
    if(l->state_cb) l->state_cb(s, l->state_cb_ctx);
}

static void on_notify(uint16_t handle, const uint8_t* data, uint8_t len, void* ctx) {
    Link* l = ctx;
    if(handle != l->notify_handle) return;
    NotifyMsg m;
    m.len = len > NOTIFY_MAX ? NOTIFY_MAX : len;
    memcpy(m.data, data, m.len);
    if(furi_message_queue_put(l->notify_q, &m, 0) == FuriStatusOk)
        furi_event_flag_set(l->flags, EvtNotify);
}

/* ---- lifecycle ------------------------------------------------------------ */

static Link* ble_alloc(void) {
    if(!bc_supported()) {
        FURI_LOG_E(TAG, "full BLE stack not installed");
        return NULL;
    }
    Link* l = malloc(sizeof(Link));
    memset(l, 0, sizeof(Link));
    l->flags = furi_event_flag_alloc();
    l->notify_q = furi_message_queue_alloc(8, sizeof(NotifyMsg));

    /* Only one owner of the radio at a time: pause the phone-facing advertising. */
    l->was_advertising = furi_hal_bt_is_active();
    furi_hal_bt_stop_advertising();
    furi_delay_ms(100);
    l->bc = bc_alloc(on_notify, l);
    return l;
}

static void ble_free(Link* l) {
    bc_free(l->bc); /* also disconnects */
    if(l->was_advertising) furi_hal_bt_start_advertising();
    furi_message_queue_free(l->notify_q);
    furi_event_flag_free(l->flags);
    free(l);
}

static void ble_set_state_cb(Link* l, LinkStateCallback cb, void* ctx) {
    l->state_cb = cb;
    l->state_cb_ctx = ctx;
}

static void ble_cancel(Link* l) {
    furi_event_flag_set(l->flags, EvtCancel);
}

static void ble_clear_cancel(Link* l) {
    furi_event_flag_clear(l->flags, EvtCancel);
}

static LinkResult ble_ready(Link* l, uint32_t timeout_ms) {
    UNUSED(l);
    UNUSED(timeout_ms);
    return LinkOk; /* the full stack was checked in alloc() */
}

/* ---- finding and connecting ---------------------------------------------- */

/* Name-prefix match, or (empty prefix) any connectable device advertising service 0x18F0. */
static bool is_printer(const BcDevice* d, const char* prefix) {
    if(d->evt_type > 1) return false; /* ADV_IND / ADV_DIRECT_IND only: must be connectable */
    size_t n = strlen(prefix);
    if(n) return strncmp(d->name, prefix, n) == 0;

    const uint8_t* const ads[2] = {d->adv, d->rsp};
    const uint8_t lens[2] = {d->adv_len, d->rsp_len};
    for(int a = 0; a < 2; a++) {
        for(uint8_t type = 0x02; type <= 0x03; type++) { /* 16-bit service UUID lists */
            uint8_t len;
            const uint8_t* p = bc_adv_find(ads[a], lens[a], type, &len);
            for(uint8_t i = 0; p && i + 1 < len; i += 2)
                if(p[i] == 0xF0 && p[i + 1] == 0x18) return true;
        }
    }
    return false;
}

/* 16-bit value of a UUID given in ATT (little-endian) form, or 0 if it isn't a SIG-base UUID. */
static uint16_t uuid16_of(const uint8_t* u, uint8_t len) {
    if(len == 2) return u[0] | (u[1] << 8);
    static const uint8_t base[12] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
    if(len == 16 && memcmp(u, base, 12) == 0 && u[14] == 0 && u[15] == 0) return u[12] | (u[13] << 8);
    return 0;
}

static int find_char(const BcChar* chars, int n, uint16_t uuid16) {
    for(int i = 0; i < n; i++)
        if(uuid16_of(chars[i].uuid, chars[i].uuid_len) == uuid16) return i;
    return -1;
}

typedef struct {
    bool valid;
    uint16_t write_handle;
    bool write_with_resp;
    bool has_notify;
    BcChar notify;
    uint16_t notify_end;
} Selection;

static void select_chars(
    Selection* sel,
    const BcChar* chars,
    int n,
    int w,
    int nt,
    uint16_t svc_end) {
    sel->valid = true;
    sel->write_handle = chars[w].value_handle;
    sel->write_with_resp = !(chars[w].props & PROP_WRITE_NO_RESP);
    sel->has_notify = nt >= 0;
    if(nt >= 0) {
        sel->notify = chars[nt];
        sel->notify_end = (nt + 1 < n) ? chars[nt + 1].decl_handle - 1 : svc_end;
    }
}

/* Known printer profiles first (service 18F0: write 2AF1 / notify 2AF0; service FF00:
 * write FF02 / notify FF01), otherwise the first service with a write-no-response and a
 * notify characteristic. Same order as the ESP32 bridge. */
static bool discover(Link* l) {
    BcService* svcs = malloc(sizeof(BcService) * BC_MAX_SERVICES);
    BcChar* chars = malloc(sizeof(BcChar) * BC_MAX_CHARS);
    Selection known = {0}, fallback = {0};

    int ns = bc_discover_services(l->bc, svcs, BC_MAX_SERVICES);
    for(int s = 0; s < ns && !known.valid && !cancelled(l); s++) {
        int nc = bc_discover_chars(l->bc, &svcs[s], chars, BC_MAX_CHARS);
        if(nc <= 0) continue;

        uint16_t svc16 = uuid16_of(svcs[s].uuid, svcs[s].uuid_len);
        int w = -1, nt = -1;
        if(svc16 == 0x18F0) {
            w = find_char(chars, nc, 0x2AF1);
            nt = find_char(chars, nc, 0x2AF0);
        } else if(svc16 == 0xFF00) {
            w = find_char(chars, nc, 0xFF02);
            nt = find_char(chars, nc, 0xFF01);
        }
        if(w >= 0) {
            select_chars(&known, chars, nc, w, nt, svcs[s].end);
            continue;
        }
        if(fallback.valid) continue;
        for(int i = 0; i < nc; i++) {
            if(w < 0 && (chars[i].props & PROP_WRITE_NO_RESP)) w = i;
            if(nt < 0 && (chars[i].props & (PROP_NOTIFY | PROP_INDICATE))) nt = i;
        }
        if(w >= 0) select_chars(&fallback, chars, nc, w, nt, svcs[s].end);
    }
    free(svcs);
    free(chars);

    Selection* sel = known.valid ? &known : (fallback.valid ? &fallback : NULL);
    if(!sel) {
        FURI_LOG_E(TAG, "no writable characteristic found");
        return false;
    }
    l->write_handle = sel->write_handle;
    l->write_with_resp = sel->write_with_resp;
    if(sel->has_notify) {
        l->notify_handle = sel->notify.value_handle;
        bool ok = bc_set_notify(l->bc, &sel->notify, sel->notify_end, true);
        if(!ok && sel->notify_end > sel->notify.value_handle) {
            /* Descriptor discovery over a one-handle range is rejected by the ST stack
             * (this printer has exactly one handle, the CCCD, between its notify value and
             * the next characteristic). The CCCD conventionally sits right after the value. */
            sel->notify.cccd_handle = sel->notify.value_handle + 1;
            ok = bc_set_notify(l->bc, &sel->notify, sel->notify_end, true);
        }
        if(!ok) {
            FURI_LOG_E(TAG, "subscribe failed: 0x%02X", bc_last_error(l->bc));
            return false; /* enable/stop replies arrive as notifications; printing would stall */
        }
    }
    return true;
}

static LinkResult ble_connect(Link* l, const char* prefix, uint32_t timeout_ms, LinkState* state_out) {
    LinkState dummy;
    if(!state_out) state_out = &dummy;

    report(l, BridgeStateScanning);
    if(!bc_scan_start(l->bc)) {
        *state_out = BridgeStateError;
        return LinkFailed;
    }

    BcDevice* devs = malloc(sizeof(BcDevice) * BC_MAX_DEVICES);
    BcDevice target;
    bool found = false;
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(!found) {
        if(cancelled(l)) {
            free(devs);
            bc_scan_stop(l->bc);
            return LinkCancelled;
        }
        if((int32_t)(deadline - furi_get_tick()) <= 0) break;

        size_t n = bc_snapshot(l->bc, devs, BC_MAX_DEVICES);
        int best_rssi = -128;
        for(size_t i = 0; i < n; i++) {
            if(is_printer(&devs[i], prefix) && devs[i].rssi > best_rssi) {
                best_rssi = devs[i].rssi;
                target = devs[i];
                found = true;
            }
        }
        if(!found) furi_delay_ms(150);
    }
    free(devs);
    bc_scan_stop(l->bc);

    if(!found) {
        *state_out = BridgeStateNotFound;
        return LinkFailed;
    }

    report(l, BridgeStateConnecting);
    /* The printer drops the link right after connecting if a previous session ended only
     * moments ago; it recovers after a couple of seconds, so back off and retry. */
    for(int attempt = 1;; attempt++) {
        if(!bc_connect(l->bc, &target, CONNECT_MS)) {
            FURI_LOG_E(TAG, "connect failed: 0x%02X", bc_last_error(l->bc));
            *state_out = BridgeStateError;
            return LinkFailed;
        }
        if(discover(l)) break;

        uint8_t err = bc_last_error(l->bc);
        bc_disconnect(l->bc);
        if(cancelled(l)) return LinkCancelled;
        if(err != 0xFD || attempt >= CONNECT_ATTEMPTS) { /* 0xFD: link dropped */
            *state_out = BridgeStateError;
            return LinkFailed;
        }
        furi_delay_ms(CONNECT_RETRY_MS);
    }

    snprintf(l->peer_name, sizeof(l->peer_name), "%s", target.name[0] ? target.name : "Printer");
    *state_out = BridgeStateConnected;
    report(l, BridgeStateConnected);
    return LinkOk;
}

/* ---- data path ------------------------------------------------------------ */

static LinkResult ble_write(Link* l, const uint8_t* data, size_t len) {
    size_t mtu = bc_mtu(l->bc);
    size_t chunk = mtu > 23 ? mtu - 3 : 20; /* ATT payload is MTU-3 */
    if(chunk > WRITE_CHUNK) chunk = WRITE_CHUNK;

    while(len) {
        if(cancelled(l)) return LinkCancelled;
        size_t n = len < chunk ? len : chunk;

        bool ok = false;
        for(int t = 0; t < WRITE_TRIES && !ok; t++) {
            ok = bc_write(l->bc, l->write_handle, data, (uint8_t)n, l->write_with_resp);
            if(!ok) {
                if(!bc_is_connected(l->bc)) return LinkFailed;
                furi_delay_ms(WRITE_PAUSE);
            }
        }
        if(!ok) return LinkFailed;

        data += n;
        len -= n;
        furi_delay_ms(WRITE_PAUSE);
    }
    return LinkOk;
}

static LinkResult ble_wait_notify(Link* l, uint8_t* buf, size_t* len, uint32_t timeout_ms) {
    NotifyMsg m;
    uint32_t deadline = furi_get_tick() + timeout_ms;
    for(;;) {
        if(furi_message_queue_get(l->notify_q, &m, 0) == FuriStatusOk) {
            size_t n = m.len < *len ? m.len : *len;
            memcpy(buf, m.data, n);
            *len = n;
            return LinkOk;
        }
        int32_t left = (int32_t)(deadline - furi_get_tick());
        if(left <= 0) return LinkTimeout;

        uint32_t got = furi_event_flag_wait(
            l->flags, EvtNotify | EvtCancel, FuriFlagWaitAny | FuriFlagNoClear, left);
        if(got & FuriFlagError) return LinkTimeout;
        if(got & EvtCancel) return LinkCancelled;
        furi_event_flag_clear(l->flags, EvtNotify);
    }
}

static void ble_drain_notify(Link* l) {
    furi_message_queue_reset(l->notify_q);
    furi_event_flag_clear(l->flags, EvtNotify);
}

static const char* ble_peer_name(Link* l) {
    return l->peer_name;
}

const LinkOps link_ble_ops = {
    .name = "Direct BLE",
    .alloc = ble_alloc,
    .free = ble_free,
    .set_state_callback = ble_set_state_cb,
    .cancel = ble_cancel,
    .clear_cancel = ble_clear_cancel,
    .ready = ble_ready,
    .connect = ble_connect,
    .write = ble_write,
    .wait_notify = ble_wait_notify,
    .drain_notify = ble_drain_notify,
    .peer_name = ble_peer_name,
};

#endif /* FICHERO_DIRECT_BLE */
