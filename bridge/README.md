# Flippero ESP32 bridge

Firmware for an ESP32 that relays bytes between the Flipper's GPIO UART and the Fichero D11s printer over
Bluetooth Low Energy. It is deliberately dumb: the Flipper app does all the printer-protocol work, and this
firmware only finds and connects to the printer, writes what it is given to the printer's GATT characteristic,
and sends the printer's notifications back.

The wire protocol between the Flipper and this firmware is in [../docs/UART_PROTOCOL.md](../docs/UART_PROTOCOL.md).

## Hardware

Any ESP32 with Bluetooth: ESP32, ESP32-C3 or ESP32-S3. The default target is the **Seeed XIAO ESP32-C3**.
The official Flipper Wi-Fi dev board (ESP32-S2) has no Bluetooth and cannot be used.

| Flipper | ESP32 (XIAO C3) |
|---------|-----------------|
| pin 13 (TX) | D7 / GPIO20 (RX) |
| pin 14 (RX) | D6 / GPIO21 (TX) |
| pin 9 (3V3) | 3V3 |
| pin 8 (GND) | GND |

UART: 115200 8N1, 3.3 V logic. Pins for other boards are set per environment in `platformio.ini`.

## Build and flash

With [PlatformIO](https://platformio.org/):

```sh
pio run -e xiao_esp32c3 -t upload      # Seeed XIAO ESP32-C3
pio run -e esp32dev -t upload          # generic ESP32 dev board
pio run -e esp32s3 -t upload           # ESP32-S3 DevKitC
```

Open the USB serial monitor (`pio device monitor`) to see `fichero bridge ready`. The USB console is separate from
the Flipper link.

## Behaviour

- `HELLO` is answered with the protocol version.
- `CONNECT` scans for 8 seconds for a device whose name starts with the given prefix (the app sends `FICHERO`),
  picks the strongest match, connects, requests an MTU of 247 and discovers the characteristics.
- GATT profiles tried, in order: service `0x18F0` (write `0x2AF1`, notify `0x2AF0`), service `0xFF00` (write
  `0xFF02`, notify `0xFF01`), then any service with a write-without-response and a notify characteristic.
- Each `WRITE` frame is one BLE write; the bridge answers with `WRITE_ACK`.

Written against NimBLE-Arduino 1.4.x. The framing in `src/bridge_frame.h` is cross-tested against the Flipper's
implementation by the host tests in the repository (`src/tests/host/run.sh`).
