// SPDX-License-Identifier: GPL-3.0-or-later
// Fichero label printer: UART <-> BLE bridge for the Flipper Zero app.
//
// Deliberately dumb. The Flipper owns all printer-protocol logic; this
// firmware only (1) finds and connects to the printer, (2) writes whatever
// bytes it is given to the printer's GATT write characteristic, and (3)
// forwards the printer's notifications back. See docs/UART_PROTOCOL.md.
//
// Written against NimBLE-Arduino 1.4.x (h2zero).

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "bridge_frame.h"

#ifndef PIN_UART_RX
#define PIN_UART_RX 20
#endif
#ifndef PIN_UART_TX
#define PIN_UART_TX 21
#endif
#ifndef UART_BAUD
#define UART_BAUD 115200
#endif

static constexpr uint32_t SCAN_SECONDS = 8;
static constexpr uint16_t PREFERRED_MTU = 247;

static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* writeChr = nullptr;
static NimBLERemoteCharacteristic* notifyChr = nullptr;
static volatile bool linkLost = false;
static String peerName;

static SemaphoreHandle_t txLock;
static frame::Parser parser;

// ---------------------------------------------------------------- UART out

static void sendFrame(uint8_t type, const uint8_t* payload, size_t len) {
    uint8_t buf[frame::MAX_FRAME];
    if (len > frame::MAX_PAYLOAD) len = frame::MAX_PAYLOAD;
    size_t n = frame::encode(buf, type, payload, len);
    // BLE callbacks run on another task; keep frames from interleaving.
    xSemaphoreTake(txLock, portMAX_DELAY);
    Serial1.write(buf, n);
    xSemaphoreGive(txLock);
}

static void sendState(frame::State state) {
    uint8_t buf[3 + 24];
    uint16_t mtu = (client && client->isConnected()) ? client->getMTU() : 0;
    buf[0] = state;
    buf[1] = mtu & 0xFF;
    buf[2] = mtu >> 8;
    size_t n = 0;
    if (state == frame::Connected) {
        n = peerName.length() > 24 ? 24 : peerName.length();
        memcpy(&buf[3], peerName.c_str(), n);
    }
    sendFrame(frame::STATE, buf, 3 + n);
}

// ----------------------------------------------------------------- BLE side

class LinkCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*) override { linkLost = true; }
};
static LinkCallbacks linkCallbacks;

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    sendFrame(frame::NOTIFY, data, len);
}

static bool nameMatches(const std::string& name, const String& prefix) {
    return prefix.length() == 0 || name.compare(0, prefix.length(), prefix.c_str()) == 0;
}

// Find the printer: by name prefix, or (for an empty prefix) by the 0x18F0 service.
static bool findPrinter(const String& prefix, NimBLEAddress& addr, std::string& name) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    NimBLEScanResults results = scan->start(SCAN_SECONDS, false);

    bool found = false;
    int bestRssi = -127;
    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);
        bool match = prefix.length() ? nameMatches(dev.getName(), prefix)
                                     : dev.isAdvertisingService(NimBLEUUID((uint16_t)0x18F0));
        if (match && dev.getRSSI() > bestRssi) {
            bestRssi = dev.getRSSI();
            addr = dev.getAddress();
            name = dev.getName();
            found = true;
        }
    }
    scan->clearResults();
    return found;
}

// Known profiles first (service 18F0: write 2AF1 / notify 2AF0; service FF00:
// write FF02 / notify FF01), then any service with a write-no-response and a
// notify characteristic.
static bool discover() {
    struct Profile {
        uint16_t svc, wr, nt;
    };
    static const Profile known[] = {{0x18F0, 0x2AF1, 0x2AF0}, {0xFF00, 0xFF02, 0xFF01}};
    writeChr = notifyChr = nullptr;

    for (const Profile& p : known) {
        NimBLERemoteService* svc = client->getService(NimBLEUUID(p.svc));
        if (!svc) continue;
        writeChr = svc->getCharacteristic(NimBLEUUID(p.wr));
        notifyChr = svc->getCharacteristic(NimBLEUUID(p.nt));
        if (writeChr) break;
    }
    if (!writeChr) {
        for (NimBLERemoteService* svc : *client->getServices(true)) {
            for (NimBLERemoteCharacteristic* c : *svc->getCharacteristics(true)) {
                if (!writeChr && c->canWriteNoResponse()) writeChr = c;
                if (!notifyChr && c->canNotify()) notifyChr = c;
            }
            if (writeChr) break;
            notifyChr = nullptr;
        }
    }
    if (!writeChr) return false;
    if (notifyChr) notifyChr->subscribe(true, onNotify);
    return true;
}

static void disconnectPrinter() {
    if (client && client->isConnected()) client->disconnect();
    writeChr = notifyChr = nullptr;
}

static void connectPrinter(const String& prefix) {
    disconnectPrinter();
    linkLost = false;

    sendState(frame::Scanning);
    NimBLEAddress addr;
    std::string name;
    if (!findPrinter(prefix, addr, name)) {
        sendState(frame::NotFound);
        return;
    }

    sendState(frame::Connecting);
    if (!client) {
        client = NimBLEDevice::createClient();
        client->setClientCallbacks(&linkCallbacks, false);
        client->setConnectTimeout(8);
    }
    if (!client->connect(addr) || !discover()) {
        disconnectPrinter();
        sendState(frame::Error);
        return;
    }
    peerName = name.c_str();
    sendState(frame::Connected);
}

static void handleWrite(const uint8_t* data, size_t len) {
    uint8_t status = 0;
    if (!client || !client->isConnected() || !writeChr) status = 2;
    else if (!writeChr->writeValue(data, len, false)) status = 1;
    sendFrame(frame::WRITE_ACK, &status, 1);
}

// ------------------------------------------------------------------ dispatch

static void handleFrame() {
    switch (parser.type) {
        case frame::HELLO: {
            uint8_t v = frame::PROTO_VERSION;
            sendFrame(frame::HELLO_ACK, &v, 1);
            break;
        }
        case frame::CONNECT: {
            String prefix;
            for (uint16_t i = 0; i < parser.len; i++) prefix += (char)parser.payload[i];
            connectPrinter(prefix);
            break;
        }
        case frame::DISCONNECT:
            disconnectPrinter();
            break;
        case frame::WRITE:
            handleWrite(parser.payload, parser.len);
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);  // USB console, not the Flipper link
    txLock = xSemaphoreCreateMutex();
    Serial1.setRxBufferSize(1024);
    Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

    NimBLEDevice::init("");
    NimBLEDevice::setMTU(PREFERRED_MTU);
    Serial.println("fichero bridge ready");
}

void loop() {
    while (Serial1.available()) {
        if (parser.feed((uint8_t)Serial1.read())) handleFrame();
    }
    if (linkLost) {
        linkLost = false;
        writeChr = notifyChr = nullptr;
        sendState(frame::Disconnected);
    }
    delay(1);
}
