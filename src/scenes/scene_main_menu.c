// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"

typedef enum {
    MenuPrint,
    MenuEdit,
    MenuSettings,
    MenuPrinterInfo,
    MenuWiring,
    MenuAppInfo,
} MenuItem;

static void menu_cb(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void fichero_scene_main_menu_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Flippero");
    submenu_add_item(app->submenu, "Preview & print", MenuPrint, menu_cb, app);
    submenu_add_item(app->submenu, "Edit label", MenuEdit, menu_cb, app);
    submenu_add_item(app->submenu, "Printer settings", MenuSettings, menu_cb, app);
    submenu_add_item(app->submenu, "Printer info", MenuPrinterInfo, menu_cb, app);
    submenu_add_item(app->submenu, "Wiring help", MenuWiring, menu_cb, app);
    submenu_add_item(app->submenu, "App info", MenuAppInfo, menu_cb, app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FicheroSceneMainMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewSubmenu);
}

bool fichero_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, FicheroSceneMainMenu, event.event);
    switch(event.event) {
    case MenuPrint:
        scene_manager_next_scene(app->scene_manager, FicheroScenePreview);
        break;
    case MenuEdit:
        scene_manager_next_scene(app->scene_manager, FicheroSceneEdit);
        break;
    case MenuSettings:
        scene_manager_next_scene(app->scene_manager, FicheroSceneSettings);
        break;
    case MenuPrinterInfo:
        app->job_kind = JobKindInfo;
        scene_manager_next_scene(app->scene_manager, FicheroSceneJob);
        break;
    case MenuWiring:
        scene_manager_next_scene(app->scene_manager, FicheroSceneWiring);
        break;
    case MenuAppInfo:
        scene_manager_next_scene(app->scene_manager, FicheroSceneInfo);
        break;
    }
    return true;
}

void fichero_scene_main_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
