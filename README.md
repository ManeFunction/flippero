# Flippero

**Design a label on your Flipper Zero, print it on a pocket label printer.**

Flippero is a Flipper Zero app that makes simple labels (text, QR codes, small icons) and prints them on a
**Fichero D11s** thermal label printer, with an on-screen preview of exactly what will be printed.

![](https://raw.githubusercontent.com/wiki/ManeFunction/flippero/screenshot-1.png)&nbsp;&nbsp;![](https://raw.githubusercontent.com/wiki/ManeFunction/flippero/screenshot-2.png)&nbsp;&nbsp;![](https://raw.githubusercontent.com/wiki/ManeFunction/flippero/screenshot-3.png)

![](https://raw.githubusercontent.com/wiki/ManeFunction/flippero/photo.jpeg)

> **Beta (0.9).** Printing works (verified with direct BLE on the Flipper Blue++ firmware), but the ESP32 bridge
> firmware has not been tested on hardware yet. See [Status](#status).
>
> **Flippero is not in the official Flipper app catalog yet.** I'm waiting for an ESP32 board to arrive so I can test
> the bridge on real hardware first, and I'll submit the app to the catalog after that. Until then, install it from
> the [Releases](../../releases) page or [build it from source](#building).

If you are able to test this app with ESP32 board or with other D11 printer models, please contact me with results!
Open an issue here or write to [ilia@inkedkettle.games](mailto:ilia@inkedkettle.games).

## Features

- **Four layouts:** text, QR code, QR code + text, icon + text.
- **Preview** of the printable area before you print, scaled to the Flipper's screen.
- **Keyboard with punctuation.** The stock Flipper keyboard has no `.` or `/`, so Flippero has its own, with a
  symbols page, upper case and cursor keys. Meant for URLs and e-mail addresses.
- **Printer settings:** density, paper type (gap / black mark / continuous), label length (20-50 mm),
  copies, orientation.
- **Printer info:** model, battery and status, read from the printer.
- Remembers your label and settings between runs (only one yet).
- Two ways to reach the printer (see below): an **ESP32 bridge** on any firmware, or **direct BLE** on the
  Flipper Blue++ firmware.

## What you need

- A **Fichero D11s** (a rebadged AiYin D11s; it advertises as `FICHERO_xxxx_BLE`). Tested with one unit; other
  AiYin/LuckPrinter-based D11 models may work but are untested.
- **Either**
  - an **ESP32 board with Bluetooth** (ESP32, C3 or S3; the Seeed XIAO ESP32-C3 is the default) and four
    wires, **or**
  - a Flipper running the [Flipper Blue++](https://github.com/andydixon/flipper-bluepp) firmware, which lets
    apps talk BLE directly (no extra hardware).

The Flipper's own Bluetooth radio can't connect to the printer on official firmware (it only supports being
connected *to*), which is why a bridge or custom firmware is needed. The official Flipper Wi-Fi dev board
(ESP32-S2) has no Bluetooth and can't be used as the bridge.

## Install

**From the Flipper app catalog:** not available yet (see the note at the top). It will be the ESP32-bridge build,
under *Apps > GPIO > Flippero*.

**From a release:** download a `.fap` from the [Releases](../../releases) page and copy it to
`SD Card/apps/GPIO/` on the Flipper. `flippero.fap` is the ESP32-bridge build and works on any firmware;
`flippero-ble.fap` is the direct-BLE build for the [Flipper Blue++](#direct-ble-flipper-blue-firmware) firmware.

**From source:** see [Building](#building).

### The ESP32 bridge

Wire the ESP32 to the Flipper's GPIO header (the app's *Wiring help* screen shows the same):

| Flipper | ESP32 |
|---------|-------|
| pin 13 (TX) | RX (XIAO C3: D7 / GPIO20) |
| pin 14 (RX) | TX (XIAO C3: D6 / GPIO21) |
| pin 9 (3V3) | 3V3 |
| pin 8 (GND) | GND |

Flash the firmware once (needs [PlatformIO](https://platformio.org/)):

```sh
cd bridge
pio run -e xiao_esp32c3 -t upload     # or esp32dev / esp32s3
```

See [bridge/README.md](bridge/README.md). The protocol between the Flipper and the ESP32 is described in
[docs/UART_PROTOCOL.md](docs/UART_PROTOCOL.md).

## Using it

1. Switch the printer on. Its LED blinks green while it waits for a connection and stays solid while connected.
   Only one device can be connected at a time, so close any other app that is using it.
2. **Edit label:** pick a layout, then edit the fields.
3. **Preview & print:** shows the part of the label that actually prints. OK prints, Back returns. While
   printing, Back cancels.
4. **Printer settings** and **Printer info** are in the main menu too, and **App info** shows who made the app and a
   QR code to this page.

> **Flippero does not keep a connection to the printer.** It connects only when a job starts (**Preview & print** or
> **Printer info**) and disconnects as soon as the job finishes or is cancelled. In practice:
> - the printer's LED is solid green only during a job and goes back to blinking afterwards;
> - every print starts with a few seconds of finding and connecting to the printer;
> - between jobs the printer is free, so a phone app (or any other device) can use it;
> - editing, previewing and changing settings never need the printer to be on.

### Keyboard

| Key | Action |
|-----|--------|
| Arrows | move over the keys |
| OK | press the key |
| up-arrow key | toggle UPPER case |
| `#+=` key | symbols page (and `abc` to return) |
| `<-` `->` keys | move the cursor |
| `OK` key (bottom right) | save |
| Back | cancel |

The letters page has `@ . / - _ :` on it, so most URLs and addresses need no symbols page.

### Layout

Text is drawn with an embedded 5x7 bitmap font scaled up to fit (`Size: Auto` picks the biggest that fits).
QR codes are limited to version 6 and at least 2 dots per module so they stay scannable at 203 dpi.

## Tips and troubleshooting

- **"Printer not found":** is it on, in range, and not connected to something else? The LED should be blinking
  (Flippero disconnects after every job, so it is never the one holding the connection between jobs).
- **QR code too small:** set **QR ECC** to *Low*. A higher error-correction level can push a URL into a bigger
  QR version, which has to be drawn with smaller dots. Low gives the biggest code for any text.
- **Labels come out upside down:** set *Orient.* to *Flipped*.
- **Stuck connecting after an aborted print:** wait a few seconds; the printer needs a moment to release the
  previous connection. The app retries on its own.

## Direct BLE (Flipper Blue++ firmware)

The catalog build uses the ESP32 bridge. If your Flipper runs
[Flipper Blue++](https://github.com/andydixon/flipper-bluepp) (official firmware 1.4.3 with ST's full BLE stack
and the BLE API exported to apps), a second flavour talks to the printer directly, with no extra hardware.

**Install Blue++ first.** Its update package also replaces the radio stack, so install the **whole update
folder or `.tgz`** (copy to `SD:/update/` and open `update.fuf`, or qFlipper's *Install from file*), not just
the `.dfu`. Going back to official firmware restores the stock stack.

**Then get the app:** releases include **`flippero-ble.fap`**, built by CI against a pinned Blue++ commit (named
in the release notes, API 87.2; the notes say so if that build failed and the file is missing). Copy it to `SD Card/apps/GPIO/`. If it refuses to load, your Blue++ firmware was
built from a different commit than the app: build from source instead, as below.

**Or build it yourself.** Build the SDK with Blue++'s `./build.sh` (it only builds; don't pass `flash`) and point
a separate ufbt home at it, so your normal SDK is untouched:

```sh
export UFBT_HOME=~/.ufbt-bluepp
ufbt update --local=/path/to/flipperzero-firmware/dist/f7-C/flipper-z-f7-sdk-local.zip --hw-target=f7
cd src
FICHERO_DIRECT_BLE=1 ufbt launch
```

The direct-BLE build **only loads on the Blue++ firmware** (it imports symbols official firmware doesn't export)
and adds **Printer settings > Link: BLE / ESP32**. The vendored GATT client in
[`src/third_party/bluepp/`](src/third_party/bluepp/) is GPL-3.0-or-later, which is why this project is too. While a
BLE job runs, the Flipper's own advertising is paused and a phone link drops; both come back afterwards.

## How it works

```
Flipper app ──UART (GPIO 13/14)──> ESP32 (BLE central) ──> Fichero D11s
 UI, rendering,                     dumb relay              FICHERO_xxxx_BLE
 printer protocol
   or, on Blue++:  Flipper app ──BLE (Flipper's own radio)──> Fichero D11s
```

- The label is rendered into a 1-bit buffer (`length x 96` dots, 8 dots/mm), then rotated 90 degrees clockwise
  into the printer's raster (12 bytes per row, MSB first, 1 = black).
- Print sequence per job, as in the reference driver: status check, density, paper type, wake, enable, raster
  (200-byte BLE writes, 20 ms apart), feed, stop.
- Everything not tied to the Flipper SDK (renderer, font, protocol, framing) is plain C in
  [`src/core/`](src/core/) and is unit-tested on the host.
- The printer protocol was worked out by [0xMH/fichero-printer](https://github.com/0xMH/fichero-printer) (MIT).

## Building

This is a Flipper Zero external app (FAP), built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
pip install ufbt
cd src
ufbt              # ESP32-bridge build, official SDK -> dist/flippero.fap
ufbt launch       # build, install and run on a connected Flipper
```

Or connect the Flipper and run `src/launch_app.sh`.

Host tests for the pure-C core (renderer, protocol, framing, and a check that the ESP32 firmware's framing matches
the Flipper's), under ASan/UBSan:

```sh
src/tests/host/run.sh        # add -v to dump rendered labels as ASCII
```

## Project layout

| Path | What |
|------|------|
| `src/` | The Flipper app (C, builds with `ufbt`) |
| `src/core/` | Pure C, no SDK: bitmap, font, label renderer, printer protocol, UART framing |
| `src/ui/` | Custom views: option list and keyboard |
| `src/scenes/` | Screens (main menu, edit, text edit, settings, preview, job, wiring) |
| `src/tests/host/` | Host tests |
| `bridge/` | ESP32 firmware (PlatformIO, NimBLE-Arduino) |
| `docs/UART_PROTOCOL.md` | Flipper-to-ESP32 framing spec |

## Status

**Beta, version 0.9.** 

Verified on hardware: direct-BLE printing and Printer info on the Flipper Blue++ firmware with a Fichero D11s
(label orientation, QR codes, text fitting, the keyboard and the settings screens).

Not yet verified on hardware: the **ESP32 bridge firmware**. Its framing is cross-tested on the host against the
Flipper's, and CI compiles it, but the BLE side has not been run end to end.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). Third-party code is listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The "Flippero" name and icon are not covered by the license, see
[TRADEMARKS.md](TRADEMARKS.md).

Flippero is an independent project. It is not affiliated with or endorsed by Flipper Devices Inc., Fichero or
AiYin; those names are used only to say what the app works with.

## Repository info

This repo follows the [Conventional Commits](https://www.conventionalcommits.org/) specification.

[![GitHub Sponsors](https://img.shields.io/github/sponsors/ManeFunction?label=Sponsor&logo=GitHubSponsors&style=flat)](https://github.com/sponsors/ManeFunction)
