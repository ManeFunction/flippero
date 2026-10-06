// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/label.h"
#include "link.h"

/* BLE name prefix the printer advertises ("FICHERO_5836_BLE"); no longer user-editable. */
#define PRINTER_NAME_PREFIX "FICHERO"

typedef struct {
    uint8_t density; /* 0..2 */
    uint8_t paper; /* FicheroPaper */
    uint8_t length_idx; /* index into settings_length_mm */
    uint8_t copies; /* 1..9 */
    uint8_t flip; /* rotate raster 180 degrees */
    uint8_t link; /* LinkKind */
} Settings;

#define SETTINGS_LENGTH_COUNT 5
extern const uint8_t settings_length_mm[SETTINGS_LENGTH_COUNT];

void settings_default(Settings* s);
uint16_t settings_length_px(const Settings* s);

/* Persist label + settings in the app data dir. Missing/stale file => defaults. */
void settings_load(Settings* s, Label* label);
void settings_save(const Settings* s, const Label* label);
