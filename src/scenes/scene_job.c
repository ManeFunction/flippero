// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"
#include "../core/uart_frame.h"

static void job_cb(JobStatus status, uint8_t arg, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FicheroEventJob | ((uint32_t)status << 8) | arg);
}

static void show(App* app, const char* header, const char* text) {
    popup_set_header(app->popup, header, 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 20, AlignCenter, AlignTop);
}

void fichero_scene_job_on_enter(void* context) {
    App* app = context;
    popup_reset(app->popup);
    show(app, app->job_kind == JobKindPrint ? "Printing" : "Printer info", "Starting...");
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewPopup);

    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    settings_save(&app->settings, &app->label);
    app->job = print_job_start(app->job_kind, &app->settings, app->bitmap, job_cb, app);
}

bool fichero_scene_job_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    const char* header = app->job_kind == JobKindPrint ? "Printing" : "Printer info";

    if(event.type == SceneManagerEventTypeBack) {
        /* Stops the job (and releases the UART) before leaving. */
        print_job_cancel_and_free(app->job);
        app->job = NULL;
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom || !(event.event & FicheroEventJob)) return false;

    JobStatus status = (event.event >> 8) & 0xFF;
    uint8_t arg = event.event & 0xFF;
    char text[48];

    switch(status) {
    case JobStatusStarting:
        show(app, header, "Starting...");
        break;
    case JobStatusConnecting:
        switch((BridgeState)arg) {
        case BridgeStateScanning:
            show(app, header, "Looking for printer...");
            break;
        case BridgeStateConnecting:
            show(app, header, "Connecting...");
            break;
        case BridgeStateConnected:
            show(app, header, "Connected");
            break;
        default:
            show(app, header, "Contacting ESP32...");
            break;
        }
        break;
    case JobStatusChecking:
        show(app, header, "Checking printer...");
        break;
    case JobStatusSending:
        snprintf(text, sizeof(text), "Sending %u%%", arg);
        show(app, header, text);
        break;
    case JobStatusFinishing:
        show(app, header, "Feeding label...");
        break;
    case JobStatusDone:
        notification_message(app->notifications, &sequence_success);
        if(app->job_kind == JobKindInfo && app->job)
            show(app, "Printer", print_job_info(app->job));
        else
            show(app, "Done", "Label printed.\nBack to return.");
        break;
    case JobStatusFailed:
        notification_message(app->notifications, &sequence_error);
        show(app, "Failed", job_failure_text(arg));
        break;
    case JobStatusCancelled:
        break;
    }
    return true;
}

void fichero_scene_job_on_exit(void* context) {
    App* app = context;
    if(app->job) {
        print_job_cancel_and_free(app->job);
        app->job = NULL;
    }
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    popup_reset(app->popup);
}
