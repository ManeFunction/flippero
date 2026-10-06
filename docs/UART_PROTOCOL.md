# Flipper <-> ESP32 bridge protocol

The ESP32 is a deliberately dumb relay. All Fichero printer-protocol logic
(density, paper mode, raster layout, delays) lives in the Flipper app; the
bridge only finds/connects to the printer and moves bytes.

Link: UART, 115200 8N1, 3V3 logic. Flipper pin 13 (TX) -> ESP32 RX,
pin 14 (RX) <- ESP32 TX, common GND.

## Frame

```
0xA5 | type | len_lo | len_hi | payload[len] | crc8
```

- `len` is little-endian, at most **200**.
- `crc8` is CRC-8 (poly 0x07, init 0x00, no reflection) over `type`, `len`
  and `payload`. Check value: `crc8("123456789") == 0xF4`.
- A receiver that sees garbage, a bad CRC or an oversized length simply
  resyncs on the next `0xA5`.

## Flipper -> bridge

| Type | Name       | Payload                                   | Bridge replies with |
|------|------------|-------------------------------------------|---------------------|
| 0x01 | HELLO      | none                                      | `HELLO_ACK`         |
| 0x02 | CONNECT    | ASCII BLE-name prefix (may be empty)      | `STATE` (several)   |
| 0x03 | DISCONNECT | none                                      | nothing             |
| 0x04 | WRITE      | bytes for **one** GATT write-no-response  | `WRITE_ACK`         |

`CONNECT` with an empty prefix matches a device advertising service `0x18F0`.
With a prefix (default `FICHERO`) it matches on the advertised name; the
strongest-signal match wins.

## Bridge -> Flipper

| Type | Name      | Payload                                            |
|------|-----------|----------------------------------------------------|
| 0x81 | HELLO_ACK | `version` (currently 1)                            |
| 0x82 | STATE     | `state`, `mtu_lo`, `mtu_hi`, then peer name (ASCII, only when Connected) |
| 0x84 | WRITE_ACK | `status`: 0 ok, 1 GATT write failed, 2 not connected |
| 0x85 | NOTIFY    | bytes notified by the printer                      |

`state`: 0 idle, 1 scanning, 2 connecting, 3 connected, 4 disconnected
(link lost), 5 printer not found, 6 error (connect/discovery failed).

## Flow control

One `WRITE` frame = one BLE write, and the Flipper waits for its `WRITE_ACK`
before sending the next, then pauses 20 ms. Chunk size is
`min(200, mtu - 3)`, so the bridge needs no buffering beyond one frame.

## GATT profile

The bridge tries, in order:

1. service `0x18F0`: write `0x2AF1`, notify `0x2AF0`
2. service `0xFF00`: write `0xFF02`, notify `0xFF01`
3. any service with a write-no-response and a notify characteristic

It requests an ATT MTU of 247 on connect; the negotiated MTU is reported in
`STATE`.

## Reference implementations

- Flipper (C): `src/core/uart_frame.[ch]`, `bridge.c`
- ESP32 (C++): `bridge/src/bridge_frame.h`, `bridge/src/main.cpp`
- `src/tests/host/run.sh` checks the two framing
  implementations produce and accept identical bytes.
