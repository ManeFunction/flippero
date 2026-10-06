// SPDX-License-Identifier: GPL-3.0-or-later
#include "flippero.h"
#include "scenes/scenes.h"

View* fichero_preview_view_alloc(App* app);
View* fichero_info_view_alloc(void);

void app_render_label(App* app) {
    bitmap_reshape(app->bitmap, settings_length_px(&app->settings));
    app->render_ok = label_render(&app->label, app->bitmap);
}

static bool custom_event_cb(void* context, uint32_t event) {
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool back_event_cb(void* context) {
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    settings_load(&app->settings, &app->label);
    app->bitmap = bitmap_alloc();

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&fichero_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_event_cb);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FicheroViewSubmenu, submenu_get_view(app->submenu));
    app->edit_list = option_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FicheroViewEdit, option_list_get_view(app->edit_list));
    app->settings_list = option_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FicheroViewSettings, option_list_get_view(app->settings_list));
    app->keyboard = keyboard_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FicheroViewKeyboard, keyboard_get_view(app->keyboard));
    app->preview = fichero_preview_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, FicheroViewPreview, app->preview);
    app->info = fichero_info_view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FicheroViewInfo, app->info);
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FicheroViewPopup, popup_get_view(app->popup));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FicheroViewWidget, widget_get_view(app->widget));

    return app;
}

static void app_free(App* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewEdit);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewKeyboard);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewPreview);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, FicheroViewInfo);

    submenu_free(app->submenu);
    option_list_free(app->edit_list);
    option_list_free(app->settings_list);
    keyboard_free(app->keyboard);
    view_free(app->preview);
    view_free(app->info);
    popup_free(app->popup);
    widget_free(app->widget);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    bitmap_free(app->bitmap);
    free(app);
}

int32_t flippero_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FicheroSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    settings_save(&app->settings, &app->label);
    app_free(app);
    return 0;
}
