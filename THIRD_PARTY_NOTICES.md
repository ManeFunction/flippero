# Third-party notices

Flippero itself is GPL-3.0-or-later (see [LICENSE](LICENSE)). It includes, links against or builds with the
following. Each bundled component keeps its own license header in its files.

## Bundled in this repository

### QR Code generator library (C), by Project Nayuki
- Files: `src/third_party/qrcodegen/`
- License: MIT. Copyright (c) Project Nayuki.
- Source: https://www.nayuki.io/page/qr-code-generator-library (https://github.com/nayuki/QR-Code-generator)
- Used in every build.

### libble (BLE central / GATT client), from Flipper Blue++
- Files: `src/third_party/bluepp/` (`ble_central.c`, `ble_central.h`), copied unmodified from commit
  `61545f5`.
- License: GPL-3.0-or-later. Copyright (C) 2026 Andy Dixon.
- Source: https://github.com/andydixon/flipper-bluepp
- Compiled **only** in the optional direct-BLE build (`FICHERO_DIRECT_BLE=1`); it is not part of the ESP32-bridge
  build that is published to the Flipper app catalog or attached to releases. Because the repository contains
  it, the repository as a whole is distributed under GPL-3.0-or-later.

### 5x7 bitmap font
- File: `src/core/font5x7.c`
- A classic column-encoded 5x7 ASCII font (the layout used by many embedded graphics libraries), reproduced
  for this project. Used for label text and for the on-screen keyboard's keys.

## Referenced, not bundled

### fichero-printer, by 0xMH
- https://github.com/0xMH/fichero-printer (MIT)
- The D11s command set, BLE profile and bitmap format were taken from its protocol documentation and reference
  implementations. No code was copied.

### Flipper Zero firmware and SDK
- Copyright Flipper Devices Inc. and contributors, GPL-3.0. The app is built against it with ufbt.

### NimBLE-Arduino, by h2zero
- https://github.com/h2zero/NimBLE-Arduino (Apache-2.0)
- Downloaded by PlatformIO when building the ESP32 bridge firmware (`bridge/`); not included in this repository.
