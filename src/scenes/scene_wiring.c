// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"

static const char* wiring_text =
    "\e#Flipper -> ESP32\e*\n"
    "13 (TX)  -> RX pin\n"
    "14 (RX)  -> TX pin\n"
    "9  (3V3) -> 3V3\n"
    "8  (GND) -> GND\n"
    "\n"
    "Serial: 115200 8N1.\n"
    "Flash the bridge\n"
    "firmware from the\n"
    "repo's bridge/ dir.\n"
    "Turn the printer on;\n"
    "it must not be paired\n"
    "to a phone.";

static const char* direct_text =
    "\e#BLE (direct)\e*\n"
    "No wiring needed.\n"
    "\n"
    "Needs the Flipper\n"
    "Blue++ firmware (full\n"
    "BLE stack).\n"
    "\n"
    "Turn the printer on and\n"
    "close any phone app\n"
    "connected to it. Your\n"
    "phone link drops while\n"
    "printing.\n"
    "\n"
    "To use an ESP32 bridge\n"
    "instead, set Link to\n"
    "ESP32 in Printer\n"
    "settings.";

void fichero_scene_wiring_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        app->settings.link == LinkKindBle ? direct_text : wiring_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewWidget);
}

bool fichero_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fichero_scene_wiring_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
