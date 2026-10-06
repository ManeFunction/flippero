// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"

static void result_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FicheroEventTextDone);
}

static char* target_buffer(App* app, size_t* cap, const char** header) {
    switch(app->text_target) {
    case TextTargetLine1:
        *cap = sizeof(app->label.line1);
        *header = "Line 1";
        return app->label.line1;
    case TextTargetLine2:
        *cap = sizeof(app->label.line2);
        *header = "Line 2 (optional)";
        return app->label.line2;
    case TextTargetQr:
    default:
        *cap = sizeof(app->label.qr);
        *header = "QR content";
        return app->label.qr;
    }
}

void fichero_scene_text_edit_on_enter(void* context) {
    App* app = context;
    size_t cap;
    const char* header;
    char* target = target_buffer(app, &cap, &header);

    keyboard_setup(app->keyboard, header, target, cap);
    keyboard_set_callback(app->keyboard, result_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewKeyboard);
}

bool fichero_scene_text_edit_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == FicheroEventTextDone) {
        size_t cap;
        const char* header;
        char* target = target_buffer(app, &cap, &header);
        keyboard_get_text(app->keyboard, app->text_buf, sizeof(app->text_buf));
        snprintf(target, cap, "%s", app->text_buf);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void fichero_scene_text_edit_on_exit(void* context) {
    UNUSED(context);
}
