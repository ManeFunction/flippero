# Flipper Zero → Fichero label printer app (app name: Flippero)

Goal: a Flipper Zero app (FAP, C) that prints simple labels — text, QR codes,
small graphics — on a Fichero thermal label printer, with an on-screen preview.

## Printer
- Fichero D11s = rebadged AiYin D11s (Xiamen Print Future Technology), LuckPrinter SDK
- This unit advertises as `FICHERO_5836_BLE`; confirmed working with the
  0xMH web GUI (https://0xmh.github.io/fichero-printer/)
- 96 px printhead, 203 DPI, 1-bit raster, default labels 14 mm x 30 mm
- Transports: BLE (service 000018f0-0000-1000-8000-00805f9b34fb) and Classic SPP
- Protocol reference (MIT): https://github.com/0xMH/fichero-printer
  - docs/PROTOCOL.md: command reference and print sequence
  - fichero/printer.py: framing and GATT writes
  - fichero/imaging.py: bitmap format
- Density 0–2; paper modes gap / black mark / continuous

## Architecture constraints
- Flipper Zero's STM32WB55 is BLE-only, and stock firmware exposes no BLE
  central role to apps, so it can't connect to the printer by itself
  (re-check custom firmwares before assuming otherwise)
- Planned approach: Flipper app (UI, rendering, preview, protocol)
  → UART over GPIO → ESP32 with Bluetooth (ESP32 / C3 / S3) acting as a
  thin UART↔BLE bridge → printer
- The official Flipper Wi-Fi dev board (ESP32-S2) has NO Bluetooth: not usable
- Preferred split: keep ESP32 firmware dumb (forward packets), keep protocol
  logic in the Flipper app

- Exception: the Flipper Blue++ custom firmware (full ST BLE stack, BLE central
  API exported to .fap apps; Unleashed/Momentum do NOT help, the limit is in the
  radio coprocessor's firmware) allows direct BLE. Supported as a second build
  flavour (`FICHERO_DIRECT_BLE=1`), see README

## Rendering
- Render into an in-memory 1-bit buffer sized to the 96 px printhead,
  independent of the 128x64 screen; screen shows a scaled-down framed preview of the
  printable area only (no zoom)
- Needs: Flipper built-in fonts for text, a small C QR library, simple bitmaps

## Decisions
- ESP32 board: Seeed XIAO ESP32-C3 by default (any BLE ESP32 works; see
  `bridge/platformio.ini`)
- UART framing: `0xA5 | type | len16 | payload | crc8`, one WRITE frame = one
  BLE write, acked per frame. Spec in `docs/UART_PROTOCOL.md`
- App structure: SceneManager with scenes main_menu, edit, text_edit, settings,
  preview, job, wiring; pure-C `core/` (no SDK) for everything testable on a host
- Text rendering uses an embedded 5x7 font, not Flipper built-in fonts:
  `Canvas` can't be read back in the SDK, so built-ins can't draw into the
  print buffer
- QR: Nayuki qrcodegen in `third_party/` (a `lib/` dir is reserved by ufbt)

- Custom UI views in `ui/`: `option_list` (wide value zone; the stock VariableItemList
  value area is a fixed ~42 px) and `keyboard` (the stock text input has no `.` or `/`).
  Keys are drawn with the embedded 5x7 font because Flipper fonts overflow a key row
- Printer name field removed: connect prefix is fixed to `FICHERO`
- Transport abstraction: `LinkOps` (`link.h`) with two backends: UART/ESP32
  (`bridge.c`) and direct BLE (`ble_link.c` + vendored Blue++ `libble`, GPLv3,
  so the project is GPL-3.0-or-later)

## Repository and publishing
- Layout: app in `src/` (appid `flippero`), ESP32 firmware in `bridge/`, docs in `docs/`.
  Models the `clock-o-dial--fz` repo: release workflow, changelog scripts, TRADEMARKS.md
- Catalog files live in the app folder (manifest `sourcecode.location.subdir: src`, `@file`
  paths resolve from there): `manifest.yml`, `changelog.md`, `docs/catalog_description.md` (limited
  Markdown: headers, bold/italic, lists, links; no code/tables/images), `icons/app_10x10.png`,
  `screenshots/` (512x256, qFlipper style)
- The catalog builds plain `ufbt` against the official SDK = ESP32 flavour only. The BLE flavour needs the
  Blue++ SDK (built from source), so `.github/workflows/build-ble.yml` builds that SDK once for a pinned Blue++
  commit (`BLUEPP_COMMIT`, cached), builds the app against it and release.yml attaches `flippero-ble.fap`.
  A failed BLE build does not block the release. Bump `BLUEPP_COMMIT` on purpose: the .fap only loads on firmware
  built from a compatible commit. build-ble.yml can also be run by hand (workflow_dispatch)
- Releasing: bump `fap_version` in application.fam, add the `X.Y:` entry to changelog.md, tag `vX.Y`;
  release.yml checks they agree. Commits follow Conventional Commits (the release notes are built from them)
- Repo URL: github.com/ManeFunction/flippero (manifest.yml, application.fam fap_weburl, README, info screen QR)
- Version is `0.9` (beta): fap_version must be plain major.minor, so "beta" lives in the changelog/README, not in the
  version string. Bump to 1.0 once the ESP32 bridge is verified on hardware

- Catalog submission is on hold until the ESP32 board arrives and the bridge is tested on hardware; the README says
  Flippero is not in the catalog yet. Remove that note when it is accepted

## Still open
- Verify the ESP32 firmware (compile in CI, run end to end) on real hardware (see README)
