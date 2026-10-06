// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define TAG "Bridge"

#define BRIDGE_BAUD        115200
#define BRIDGE_WRITE_CHUNK 200 /* matches the 200 B BLE chunks the reference driver uses */
#define BRIDGE_WRITE_PAUSE 20 /* ms between BLE chunks */
#define BRIDGE_ACK_TIMEOUT 2000
#define BRIDGE_PEER_NAME_MAX 24
#define BRIDGE_NOTIFY_MAX  32

typedef enum {
    EvtHello = (1 << 0),
    EvtState = (1 << 1),
    EvtWriteAck = (1 << 2),
    EvtNotify = (1 << 3),
    EvtCancel = (1 << 4),
    EvtStop = (1 << 5),
} BridgeEvt;

typedef struct {
    uint8_t len;
    uint8_t data[BRIDGE_NOTIFY_MAX];
} NotifyMsg;

struct Bridge {
    Expansion* expansion;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* rx_thread;
    FuriEventFlag* flags;
    FuriMutex* lock;
    FuriMessageQueue* notify_queue;
    UartFrameParser parser;

    /* guarded by lock */
    BridgeState state;
    uint16_t mtu;
    char peer_name[BRIDGE_PEER_NAME_MAX + 1];
    uint8_t write_status;

    BridgeStateCallback state_cb;
    void* state_cb_ctx;
};

static void bridge_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    Bridge* b = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(b->rx_stream, &byte, 1, 0);
    }
}

static void bridge_handle_frame(Bridge* b, const UartFrame* f) {
    switch(f->type) {
    case FRAME_HELLO_ACK:
        furi_event_flag_set(b->flags, EvtHello);
        break;

    case FRAME_STATE: {
        if(f->len < 3) break;
        furi_mutex_acquire(b->lock, FuriWaitForever);
        b->state = f->payload[0];
        b->mtu = f->payload[1] | (f->payload[2] << 8);
        size_t n = f->len - 3;
        if(n > BRIDGE_PEER_NAME_MAX) n = BRIDGE_PEER_NAME_MAX;
        memcpy(b->peer_name, &f->payload[3], n);
        b->peer_name[n] = 0;
        BridgeState state = b->state;
        furi_mutex_release(b->lock);
        furi_event_flag_set(b->flags, EvtState);
        if(b->state_cb) b->state_cb(state, b->state_cb_ctx);
        break;
    }

    case FRAME_WRITE_ACK:
        furi_mutex_acquire(b->lock, FuriWaitForever);
        b->write_status = f->len ? f->payload[0] : 1;
        furi_mutex_release(b->lock);
        furi_event_flag_set(b->flags, EvtWriteAck);
        break;

    case FRAME_NOTIFY: {
        NotifyMsg msg = {0};
        msg.len = f->len > BRIDGE_NOTIFY_MAX ? BRIDGE_NOTIFY_MAX : f->len;
        memcpy(msg.data, f->payload, msg.len);
        if(furi_message_queue_put(b->notify_queue, &msg, 0) == FuriStatusOk) {
            furi_event_flag_set(b->flags, EvtNotify);
        }
        break;
    }

    default:
        FURI_LOG_W(TAG, "unknown frame 0x%02X", f->type);
        break;
    }
}

static int32_t bridge_rx_thread(void* context) {
    Bridge* b = context;
    uint8_t byte;
    while(!(furi_event_flag_get(b->flags) & EvtStop)) {
        if(furi_stream_buffer_receive(b->rx_stream, &byte, 1, 50) == 1) {
            if(uart_frame_parser_feed(&b->parser, byte)) {
                bridge_handle_frame(b, &b->parser.frame);
            }
        }
    }
    return 0;
}

static void bridge_send(Bridge* b, uint8_t type, const uint8_t* payload, uint16_t len) {
    uint8_t frame[UART_FRAME_MAX_LEN];
    size_t n = uart_frame_encode(frame, type, payload, len);
    furi_hal_serial_tx(b->serial, frame, n);
    furi_hal_serial_tx_wait_complete(b->serial);
}

Bridge* bridge_alloc(void) {
    Bridge* b = malloc(sizeof(Bridge));
    memset(b, 0, sizeof(Bridge));

    /* The expansion-module service owns the USART by default. */
    b->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(b->expansion);

    b->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!b->serial) {
        FURI_LOG_E(TAG, "USART busy");
        expansion_enable(b->expansion);
        furi_record_close(RECORD_EXPANSION);
        free(b);
        return NULL;
    }

    b->flags = furi_event_flag_alloc();
    b->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    b->notify_queue = furi_message_queue_alloc(8, sizeof(NotifyMsg));
    b->rx_stream = furi_stream_buffer_alloc(512, 1);
    uart_frame_parser_reset(&b->parser);
    b->state = BridgeStateIdle;

    b->rx_thread = furi_thread_alloc_ex("BridgeRx", 1536, bridge_rx_thread, b);
    furi_thread_start(b->rx_thread);

    furi_hal_serial_init(b->serial, BRIDGE_BAUD);
    furi_hal_serial_async_rx_start(b->serial, bridge_rx_isr, b, false);
    return b;
}

void bridge_free(Bridge* b) {
    if(!b) return;
    bridge_send(b, FRAME_DISCONNECT, NULL, 0);

    furi_hal_serial_async_rx_stop(b->serial);
    furi_event_flag_set(b->flags, EvtStop);
    furi_thread_join(b->rx_thread);
    furi_thread_free(b->rx_thread);

    furi_hal_serial_deinit(b->serial);
    furi_hal_serial_control_release(b->serial);

    expansion_enable(b->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_stream_buffer_free(b->rx_stream);
    furi_message_queue_free(b->notify_queue);
    furi_mutex_free(b->lock);
    furi_event_flag_free(b->flags);
    free(b);
}

void bridge_set_state_callback(Bridge* b, BridgeStateCallback cb, void* context) {
    b->state_cb = cb;
    b->state_cb_ctx = context;
}

void bridge_cancel(Bridge* b) {
    furi_event_flag_set(b->flags, EvtCancel);
}

void bridge_clear_cancel(Bridge* b) {
    furi_event_flag_clear(b->flags, EvtCancel);
}

/* Wait for `evt` or cancel. Consumes `evt` on success. */
static LinkResult bridge_wait(Bridge* b, uint32_t evt, uint32_t timeout_ms) {
    uint32_t got = furi_event_flag_wait(
        b->flags, evt | EvtCancel, FuriFlagWaitAny | FuriFlagNoClear, timeout_ms);
    if(got & FuriFlagError) return LinkTimeout; /* FuriFlagErrorTimeout also lands here */
    if(got & EvtCancel) return LinkCancelled;
    furi_event_flag_clear(b->flags, evt);
    return LinkOk;
}

LinkResult bridge_hello(Bridge* b, uint32_t timeout_ms) {
    furi_event_flag_clear(b->flags, EvtHello);
    bridge_send(b, FRAME_HELLO, NULL, 0);
    return bridge_wait(b, EvtHello, timeout_ms);
}

LinkResult bridge_connect(
    Bridge* b,
    const char* name_prefix,
    uint32_t timeout_ms,
    BridgeState* state_out) {
    furi_event_flag_clear(b->flags, EvtState);
    bridge_send(b, FRAME_CONNECT, (const uint8_t*)name_prefix, strlen(name_prefix));

    uint32_t deadline = furi_get_tick() + timeout_ms;
    for(;;) {
        uint32_t now = furi_get_tick();
        if((int32_t)(deadline - now) <= 0) return LinkTimeout;
        LinkResult r = bridge_wait(b, EvtState, deadline - now);
        if(r != LinkOk) return r;

        furi_mutex_acquire(b->lock, FuriWaitForever);
        BridgeState s = b->state;
        furi_mutex_release(b->lock);
        if(state_out) *state_out = s;

        if(s == BridgeStateConnected) return LinkOk;
        if(s == BridgeStateNotFound || s == BridgeStateError || s == BridgeStateDisconnected)
            return LinkFailed;
        /* Scanning / Connecting: keep waiting */
    }
}

void bridge_disconnect(Bridge* b) {
    bridge_send(b, FRAME_DISCONNECT, NULL, 0);
}

const char* bridge_peer_name(Bridge* b) {
    return b->peer_name;
}

LinkResult bridge_write(Bridge* b, const uint8_t* data, size_t len) {
    furi_mutex_acquire(b->lock, FuriWaitForever);
    size_t chunk = BRIDGE_WRITE_CHUNK;
    /* ATT payload is MTU-3; fall back to 20 B if the bridge never negotiated */
    size_t att = b->mtu > 23 ? b->mtu - 3 : 20;
    if(att < chunk) chunk = att;
    furi_mutex_release(b->lock);

    while(len) {
        size_t n = len < chunk ? len : chunk;
        furi_event_flag_clear(b->flags, EvtWriteAck);
        bridge_send(b, FRAME_WRITE, data, n);

        LinkResult r = bridge_wait(b, EvtWriteAck, BRIDGE_ACK_TIMEOUT);
        if(r != LinkOk) return r;

        furi_mutex_acquire(b->lock, FuriWaitForever);
        uint8_t status = b->write_status;
        furi_mutex_release(b->lock);
        if(status != 0) return LinkFailed;

        data += n;
        len -= n;
        furi_delay_ms(BRIDGE_WRITE_PAUSE);
    }
    return LinkOk;
}

LinkResult bridge_wait_notify(Bridge* b, uint8_t* buf, size_t* len, uint32_t timeout_ms) {
    NotifyMsg msg;
    uint32_t deadline = furi_get_tick() + timeout_ms;
    for(;;) {
        if(furi_message_queue_get(b->notify_queue, &msg, 0) == FuriStatusOk) {
            size_t n = msg.len < *len ? msg.len : *len;
            memcpy(buf, msg.data, n);
            *len = n;
            return LinkOk;
        }
        uint32_t now = furi_get_tick();
        if((int32_t)(deadline - now) <= 0) return LinkTimeout;
        LinkResult r = bridge_wait(b, EvtNotify, deadline - now);
        if(r != LinkOk) return r;
    }
}

void bridge_drain_notify(Bridge* b) {
    furi_message_queue_reset(b->notify_queue);
    furi_event_flag_clear(b->flags, EvtNotify);
}

/* ---- LinkOps adapter ---------------------------------------------------- */

static Link* uart_alloc(void) {
    return (Link*)bridge_alloc();
}
static void uart_free(Link* l) {
    bridge_free((Bridge*)l);
}
static void uart_set_state_cb(Link* l, LinkStateCallback cb, void* ctx) {
    bridge_set_state_callback((Bridge*)l, cb, ctx);
}
static void uart_cancel(Link* l) {
    bridge_cancel((Bridge*)l);
}
static void uart_clear_cancel(Link* l) {
    bridge_clear_cancel((Bridge*)l);
}
static LinkResult uart_ready(Link* l, uint32_t timeout_ms) {
    return bridge_hello((Bridge*)l, timeout_ms);
}
static LinkResult uart_connect(Link* l, const char* prefix, uint32_t timeout_ms, LinkState* st) {
    return bridge_connect((Bridge*)l, prefix, timeout_ms, st);
}
static LinkResult uart_write(Link* l, const uint8_t* data, size_t len) {
    return bridge_write((Bridge*)l, data, len);
}
static LinkResult uart_wait_notify(Link* l, uint8_t* buf, size_t* len, uint32_t timeout_ms) {
    return bridge_wait_notify((Bridge*)l, buf, len, timeout_ms);
}
static void uart_drain_notify(Link* l) {
    bridge_drain_notify((Bridge*)l);
}
static const char* uart_peer_name(Link* l) {
    return bridge_peer_name((Bridge*)l);
}

const LinkOps link_uart_ops = {
    .name = "ESP32 bridge",
    .alloc = uart_alloc,
    .free = uart_free,
    .set_state_callback = uart_set_state_cb,
    .cancel = uart_cancel,
    .clear_cancel = uart_clear_cancel,
    .ready = uart_ready,
    .connect = uart_connect,
    .write = uart_write,
    .wait_notify = uart_wait_notify,
    .drain_notify = uart_drain_notify,
    .peer_name = uart_peer_name,
};
