// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"

/* Declaration order is the display order. */
typedef enum {
    EditLayout,
    EditLine1,
    EditLine2,
    EditQr,
    EditSize,
    EditBold,
    EditBorder,
    EditIcon,
    EditEcc,
    EditPreview,
    EditFieldCount,
} EditField;

static const char* const ecc_names[LabelEccCount] = {"Low", "Medium", "Quartile"};
static const char* const off_on[2] = {"Off", "On"};
static const char* const size_names[LABEL_SCALE_MAX + 1] =
    {"Auto", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8"};

/* Which fields a layout actually uses; the rest are hidden. */
static bool field_shown(LabelLayout layout, EditField f) {
    bool has_text = layout != LabelLayoutQr;
    bool has_qr = layout == LabelLayoutQr || layout == LabelLayoutQrText;
    switch(f) {
    case EditLine1:
    case EditLine2:
    case EditSize:
    case EditBold:
        return has_text;
    case EditQr:
    case EditEcc:
        return has_qr;
    case EditIcon:
        return layout == LabelLayoutIconText;
    case EditLayout:
    case EditBorder:
    case EditPreview:
    default:
        return true;
    }
}

static void add_choice(
    App* app,
    EditField id,
    const char* label,
    const char* const* choices,
    uint8_t count,
    uint8_t index) {
    app->edit_ids[app->edit_count++] = id;
    option_list_add(
        app->edit_list,
        &(OptionItem){
            .label = label,
            .kind = OptionItemChoice,
            .choices = choices,
            .count = count,
            .index = index,
        });
}

static void add_text(App* app, EditField id, const char* label, const char* text) {
    app->edit_ids[app->edit_count++] = id;
    option_list_add(
        app->edit_list, &(OptionItem){.label = label, .kind = OptionItemText, .text = text});
}

/* (Re)build the visible rows for the current layout. Selection goes back to the top row. */
static void build_list(App* app) {
    Label* l = &app->label;
    LabelLayout layout = l->layout;

    option_list_reset(app->edit_list);
    app->edit_count = 0;

    for(EditField f = 0; f < EditFieldCount; f++) {
        if(!field_shown(layout, f)) continue;
        switch(f) {
        case EditLayout:
            add_choice(app, f, "Layout", label_layout_names, LabelLayoutCount, l->layout);
            break;
        case EditLine1:
            add_text(app, f, "Line 1", l->line1);
            break;
        case EditLine2:
            add_text(app, f, "Line 2", l->line2);
            break;
        case EditQr:
            add_text(app, f, "QR text", l->qr);
            break;
        case EditSize:
            add_choice(app, f, "Size", size_names, LABEL_SCALE_MAX + 1, l->scale);
            break;
        case EditBold:
            add_choice(app, f, "Bold", off_on, 2, l->bold);
            break;
        case EditBorder:
            add_choice(app, f, "Border", off_on, 2, l->border);
            break;
        case EditIcon:
            add_choice(app, f, "Icon", label_icon_names, LABEL_ICON_COUNT, l->icon);
            break;
        case EditEcc:
            add_choice(app, f, "QR ECC", ecc_names, LabelEccCount, l->qr_ecc);
            break;
        case EditPreview:
            app->edit_ids[app->edit_count++] = f;
            option_list_add(
                app->edit_list,
                &(OptionItem){.label = "Preview & print", .kind = OptionItemAction});
            break;
        default:
            break;
        }
    }
}

static void on_change(void* context, uint8_t row, uint8_t v) {
    App* app = context;
    Label* l = &app->label;
    switch(app->edit_ids[row]) {
    case EditLayout:
        l->layout = v;
        build_list(app); /* different layout, different fields */
        break;
    case EditSize:
        l->scale = v;
        break;
    case EditBold:
        l->bold = v;
        break;
    case EditBorder:
        l->border = v;
        break;
    case EditIcon:
        l->icon = v;
        break;
    case EditEcc:
        l->qr_ecc = v;
        break;
    default:
        break;
    }
}

static void on_enter(void* context, uint8_t row) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, app->edit_ids[row]);
}

void fichero_scene_edit_on_enter(void* context) {
    App* app = context;

    option_list_set_callbacks(app->edit_list, on_change, on_enter, app);
    build_list(app);

    /* back from a text field: select the row we came from (stored as a field, not a row) */
    uint32_t last = scene_manager_get_scene_state(app->scene_manager, FicheroSceneEdit);
    for(uint8_t row = 0; row < app->edit_count; row++)
        if(app->edit_ids[row] == last) option_list_set_selected(app->edit_list, row);

    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewEdit);
}

bool fichero_scene_edit_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, FicheroSceneEdit, event.event);
    switch(event.event) {
    case EditLine1:
        app->text_target = TextTargetLine1;
        scene_manager_next_scene(app->scene_manager, FicheroSceneTextEdit);
        break;
    case EditLine2:
        app->text_target = TextTargetLine2;
        scene_manager_next_scene(app->scene_manager, FicheroSceneTextEdit);
        break;
    case EditQr:
        app->text_target = TextTargetQr;
        scene_manager_next_scene(app->scene_manager, FicheroSceneTextEdit);
        break;
    case EditPreview:
        scene_manager_next_scene(app->scene_manager, FicheroScenePreview);
        break;
    }
    return true;
}

void fichero_scene_edit_on_exit(void* context) {
    App* app = context;
    option_list_reset(app->edit_list);
    settings_save(&app->settings, &app->label);
}
