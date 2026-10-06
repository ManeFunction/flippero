// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "core/fichero_proto.h"
#include "scenes.h"

typedef enum {
    SetDensity,
    SetPaper,
    SetLength,
    SetCopies,
    SetRotation,
    SetLink, /* keep last: only present in direct-BLE builds */
} SetItem;

static const char* const density_names[3] = {"Light", "Medium", "Dark"};
static const char* const paper_names[3] = {"Gap", "Black mark", "Continuous"};
/* must match settings_length_mm */
static const char* const length_names[SETTINGS_LENGTH_COUNT] =
    {"20 mm", "25 mm", "30 mm", "40 mm", "50 mm"};
static const char* const copies_names[9] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
static const char* const rotation_names[2] = {"Normal", "Flipped"};
/* indexed by LinkKind */
static const char* const link_names[LinkKindCount] = {"ESP32", "BLE"};

static void on_change(void* context, uint8_t item, uint8_t v) {
    Settings* s = &((App*)context)->settings;
    switch(item) {
    case SetDensity:
        s->density = v;
        break;
    case SetPaper:
        s->paper = v;
        break;
    case SetLength:
        s->length_idx = v;
        break;
    case SetCopies:
        s->copies = v + 1;
        break;
    case SetRotation:
        s->flip = v;
        break;
    case SetLink:
        s->link = v;
        break;
    }
}

static void add_choice(
    App* app,
    const char* label,
    const char* const* choices,
    uint8_t count,
    uint8_t index) {
    option_list_add(
        app->settings_list,
        &(OptionItem){
            .label = label,
            .kind = OptionItemChoice,
            .choices = choices,
            .count = count,
            .index = index,
        });
}

void fichero_scene_settings_on_enter(void* context) {
    App* app = context;
    Settings* s = &app->settings;

    option_list_reset(app->settings_list);
    option_list_set_callbacks(app->settings_list, on_change, NULL, app);

    add_choice(app, "Density", density_names, 3, s->density);
    add_choice(app, "Paper", paper_names, 3, s->paper);
    add_choice(app, "Length", length_names, SETTINGS_LENGTH_COUNT, s->length_idx);
    add_choice(app, "Copies", copies_names, 9, s->copies - 1);
    add_choice(app, "Orient.", rotation_names, 2, s->flip);
    if(link_kind_available(LinkKindBle)) add_choice(app, "Link", link_names, LinkKindCount, s->link);

    option_list_set_selected(
        app->settings_list,
        scene_manager_get_scene_state(app->scene_manager, FicheroSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewSettings);
}

bool fichero_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fichero_scene_settings_on_exit(void* context) {
    App* app = context;
    option_list_reset(app->settings_list);
    settings_save(&app->settings, &app->label);
}
