**Design a label on your Flipper, print it on a pocket label printer.**

Flippero makes simple labels for the Fichero D11s thermal label printer: text, QR codes and small icons, with a preview of exactly what will be printed.

## What you need

- A **Fichero D11s** label printer (it shows up as FICHERO_xxxx_BLE).
- A small **ESP32 board with Bluetooth** (a Seeed XIAO ESP32-C3 works well) wired to the Flipper's GPIO pins. The Flipper's own radio can't connect to the printer on official firmware, so the ESP32 relays the data over Bluetooth.
- The bridge firmware from the project page (link below), flashed to the ESP32 once.

## Features

- **Four layouts:** text, QR code, QR code with text, icon with text.
- **Preview** of the printable area before you print.
- **Keyboard with punctuation:** type URLs and e-mail addresses with . / @ : - _ and more (the stock Flipper keyboard has none of these).
- **Settings:** print density, paper type (gap, black mark, continuous), label length, copies and orientation.
- **Printer info:** model, battery level and status.
- Your label and settings are remembered between runs.

## Wiring

The Wiring help screen in the app shows this too.

- Flipper pin 13 (TX) to the ESP32 RX
- Flipper pin 14 (RX) to the ESP32 TX
- Flipper pin 9 (3V3) to the ESP32 3V3
- Flipper pin 8 (GND) to the ESP32 GND

## Tips

- Switch the printer on first. Its LED blinks green while it waits for a connection and stays solid while connected.
- Flippero does not keep a connection open. It connects only when you print (or read the printer info) and disconnects right afterwards, so every print starts with a few seconds of connecting, and the printer is free for other devices in between.
- Only one device can be connected to the printer at a time. Close any phone app or web page that is using it.
- The printer starts printing about 2 mm after the label's edge, so the layouts leave room at the end of the label.
- For the biggest QR code leave **QR ECC** on Low.

## Links

- [Project page](https://github.com/ManeFunction/flippero) with step-by-step instructions, the ESP32 bridge firmware to flash and the source code.

The project page also describes another version of the app that connects to the printer directly, without an ESP32 board. It needs a different setup on the Flipper, so it is not part of this catalog version. Follow the link above to find it.
