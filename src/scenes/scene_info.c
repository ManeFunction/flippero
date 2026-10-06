// SPDX-License-Identifier: GPL-3.0-or-later
#include "../flippero.h"
#include "scenes.h"
#include "flippero_icons.h"

#include "../third_party/qrcodegen/qrcodegen.h"

#include <gui/elements.h>

/* Project page, shown as a QR code. */
#define APP_URL "https://github.com/ManeFunction/flippero"

#define QR_MAX_VERSION 3 /* more than the URL needs; the encoder picks the smallest that fits */

typedef struct {
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
    bool valid;
} InfoModel;

/* ---- drawing, laid out like the About page of Clock'o'Dial ------------------------------ */

/* The Canvas API has no letter-spacing, so draw one glyph at a time. */
static void draw_tracked_str(Canvas* canvas, int32_t x, int32_t y, int32_t gap, const char* str) {
    char glyph[2] = {0, 0};
    for(const char* p = str; *p; p++) {
        glyph[0] = *p;
        canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, glyph);
        x += canvas_glyph_width(canvas, (uint16_t)(unsigned char)*p) + gap;
    }
}

static int32_t tracked_str_width(Canvas* canvas, int32_t gap, const char* str) {
    int32_t width = 0;
    for(const char* p = str; *p; p++) {
        if(p != str) width += gap;
        width += canvas_glyph_width(canvas, (uint16_t)(unsigned char)*p);
    }
    return width;
}

static void info_draw(Canvas* canvas, void* _model) {
    const InfoModel* m = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* header, packed against the top edge to leave room for the content */
    canvas_set_font(canvas, FontPrimary);
    const int32_t title_h = canvas_get_font_params(canvas, FontPrimary)->height;
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "DEVELOPED BY");
    canvas_set_font(canvas, FontSecondary);
    const int32_t sub_h = canvas_get_font_params(canvas, FontSecondary)->height;
    canvas_draw_str_aligned(canvas, 64, title_h + 1, AlignCenter, AlignTop, "Ilia Petrov-Komotskii");
    const int32_t top = title_h + 1 + sub_h;

    /* text block on the right of the QR code */
    const int32_t line_h = sub_h;
    const int32_t gap_1_2 = 2; /* extra space between "Thank you," and "please support" */
    const int32_t gap_2_3 = 5; /* extra space between "please support" and "on GitHub" */
    const int32_t letter_gap = 1; /* "on GitHub" only */
    const int32_t text_h = line_h * 3 + gap_1_2 + gap_2_3;
    const int32_t line3_w = tracked_str_width(canvas, letter_gap, "on GitHub") + 1 + 3 +
                            icon_get_width(&I_star) + 2 + icon_get_width(&I_heart);
    int32_t text_w = canvas_string_width(canvas, "please support");
    if(line3_w > text_w) text_w = line3_w;

    /* QR code at one screen pixel per module; the white around it is the quiet zone */
    const int32_t qr_size = m->valid ? qrcodegen_getSize(m->qr) : 0;
    const int32_t side_gap = 6;
    int32_t left = (128 - (qr_size + side_gap + text_w)) / 2;
    if(left < 1) left = 1;
    const int32_t qr_y = top + (64 - top - qr_size) / 2;
    for(int32_t y = 0; y < qr_size; y++)
        for(int32_t x = 0; x < qr_size; x++)
            if(qrcodegen_getModule(m->qr, x, y)) canvas_draw_dot(canvas, left + x, qr_y + y);

    const int32_t text_x = left + qr_size + side_gap;
    int32_t y = top + (64 - top - text_h) / 2; /* the whole block is centered next to the QR code */
    canvas_draw_str_aligned(canvas, text_x, y - 1, AlignLeft, AlignTop, "Thank you,");
    y += line_h + gap_1_2;
    canvas_draw_str_aligned(canvas, text_x, y - 1, AlignLeft, AlignTop, "please support");
    y += line_h + gap_2_3;

    /* no bold bitmap font here: draw the last line twice, a pixel apart, to thicken it */
    const int32_t y3 = y + 1;
    draw_tracked_str(canvas, text_x, y3, letter_gap, "on GitHub");
    draw_tracked_str(canvas, text_x + 1, y3, letter_gap, "on GitHub");
    const int32_t icons_x = text_x + tracked_str_width(canvas, letter_gap, "on GitHub") + 1 + 3;
    const int32_t icons_cy = y3 + line_h / 2 - 1;
    canvas_draw_icon(canvas, icons_x, icons_cy - icon_get_height(&I_star) / 2, &I_star);
    canvas_draw_icon(
        canvas,
        icons_x + icon_get_width(&I_star) + 2,
        icons_cy - icon_get_height(&I_heart) / 2,
        &I_heart);
}

View* fichero_info_view_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(InfoModel));
    view_set_draw_callback(view, info_draw);
    return view;
}

/* ---- scene ------------------------------------------------------------------------------ */

void fichero_scene_info_on_enter(void* context) {
    App* app = context;

    /* Upper-case text uses the QR "alphanumeric" mode, which is denser than "byte" mode: the URL
     * then fits version 2 (25x25 modules) instead of version 3 (29x29). Scheme and host are
     * case-insensitive, and GitHub resolves repository paths case-insensitively too. */
    char text[sizeof(APP_URL)];
    for(size_t i = 0; i < sizeof(APP_URL); i++) {
        char c = APP_URL[i];
        text[i] = (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
    }

    uint8_t* tmp = malloc(qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION));
    with_view_model(
        app->info,
        InfoModel * m,
        {
            m->valid = qrcodegen_encodeText(
                text,
                tmp,
                m->qr,
                qrcodegen_Ecc_LOW,
                1,
                QR_MAX_VERSION,
                qrcodegen_Mask_AUTO,
                false);
        },
        true);
    free(tmp);

    view_dispatcher_switch_to_view(app->view_dispatcher, FicheroViewInfo);
}

bool fichero_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fichero_scene_info_on_exit(void* context) {
    UNUSED(context);
}
