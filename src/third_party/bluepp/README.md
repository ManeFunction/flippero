# libble (vendored)

`ble_central.[ch]`: BLE central / GATT client for the Flipper's STM32WB55 on
top of ST's full BLE stack, from **Flipper Blue++**
(https://github.com/andydixon/flipper-bluepp), commit `61545f5`.

Copied unmodified. Copyright (C) 2026 Andy Dixon, **GPL-3.0-or-later** (see the
SPDX headers and the repository-root LICENSE). Because of this, the direct-BLE
backend, and with it this project, is distributed under GPL-3.0-or-later.

Only compiled when building with `FICHERO_DIRECT_BLE=1` against the Blue++ SDK.
