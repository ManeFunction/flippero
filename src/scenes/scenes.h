// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) FicheroScene##id,
typedef enum {
#include "scene_config.h"
    FicheroSceneNum,
} FicheroScene;
#undef ADD_SCENE

extern const SceneManagerHandlers fichero_scene_handlers;

#define ADD_SCENE(prefix, name, id) \
    void prefix##_scene_##name##_on_enter(void* context);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    void prefix##_scene_##name##_on_exit(void* context);
#include "scene_config.h"
#undef ADD_SCENE
