// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings.h"
#include "print_job.h"
#include "core/bitmap.h"
#include "core/label.h"
#include "ui/option_list.h"
#include "ui/keyboard.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>

typedef enum {
    FicheroViewSubmenu,
    FicheroViewEdit,
    FicheroViewSettings,
    FicheroViewKeyboard,
    FicheroViewPreview,
    FicheroViewPopup,
    FicheroViewWidget,
    FicheroViewInfo,
} FicheroView;

typedef enum {
    FicheroEventTextDone = 1,
    FicheroEventPrint,
    FicheroEventJob = 0x10000, /* | status << 8 | arg */
} FicheroEvent;

typedef enum {
    TextTargetLine1,
    TextTargetLine2,
    TextTargetQr,
} TextTarget;

#define TEXT_BUF_SIZE (LABEL_QR_MAX + 1)

typedef struct App {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    OptionList* edit_list;
    OptionList* settings_list;
    Keyboard* keyboard;
    View* preview;
    View* info;
    Popup* popup;
    Widget* widget;

    Settings settings;
    Label label;
    Bitmap* bitmap;
    bool render_ok;

    uint8_t edit_ids[OPTION_LIST_MAX_ITEMS]; /* Edit screen: row -> field, which rows are shown depends on the layout */
    uint8_t edit_count;

    char text_buf[TEXT_BUF_SIZE];
    TextTarget text_target;

    JobKind job_kind;
    PrintJob* job;
} App;

/* Re-render app->label into app->bitmap at the configured length. */
void app_render_label(App* app);
