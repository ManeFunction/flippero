// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"

#define VIEW_W     128
#define VIEW_H     55 /* above the hint line */
#define MAX_PREVIEW_W 124

typedef struct {
    const Bitmap* bmp;
    bool render_ok;
} PreviewModel;

/* Draws the part of the label that actually prints (the first width - LABEL_LEAD_PX dots; the
 * rest falls beyond the label's end), scaled down to fit and framed, centered on the screen. */
static void preview_draw(Canvas* canvas, void* model) {
    const PreviewModel* m = model;
    const Bitmap* b = m->bmp;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    int src_w = b->w - LABEL_LEAD_PX;
    int d = (src_w + MAX_PREVIEW_W - 1) / MAX_PREVIEW_W; /* integer down-scale factor */
    if(d < 2) d = 2;
    int pw = src_w / d, ph = b->h / d;
    int frame_w = pw + 2, frame_h = ph + 2;
    int fx = (VIEW_W - frame_w) / 2;
    int fy = (VIEW_H - frame_h) / 2;

    canvas_draw_frame(canvas, fx, fy, frame_w, frame_h);
    for(int y = 0; y < ph; y++) {
        for(int x = 0; x < pw; x++) {
            bool black = false;
            for(int j = 0; j < d && !black; j++)
                for(int i = 0; i < d; i++)
                    if(bitmap_get(b, x * d + i, y * d + j)) {
                        black = true;
                        break;
                    }
            if(black) canvas_draw_dot(canvas, fx + 1 + x, fy + 1 + y);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, VIEW_H - 1, VIEW_W - 1, VIEW_H - 1); /* 1 px above the area the frame is centered in */
    canvas_draw_str_aligned(
        canvas,
        64,
        63,
        AlignCenter,
        AlignBottom,
        m->render_ok ? "OK: print" : "QR too long - edit label");
}

static bool preview_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FicheroEventPrint);
        return true;
    }
    return false; /* Back goes to the previous scene */
}

View* fichero_preview_view_alloc(App* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(PreviewModel));
    view_set_context(view, app);
    view_set_draw_callback(view, preview_draw);
    view_set_input_callback(view, preview_input);
    return view;
}

void fichero_scene_preview_on_enter(void* context) {
    App* app = context;
    app_render_label(app);
    with_view_model(
        app->preview,
        PreviewModel * m,
        {
            m->bmp = app->bitmap;
            m->render_ok = app->render_ok;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewPreview);
}

bool fichero_scene_preview_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == FicheroEventPrint) {
        if(!app->render_ok) {
            notification_message(app->notifications, &sequence_error);
            return true;
        }
        app->job_kind = JobKindPrint;
        scene_manager_next_scene(app->scene_manager, FicheroSceneJob);
        return true;
    }
    return false;
}

void fichero_scene_preview_on_exit(void* context) {
    UNUSED(context);
}
