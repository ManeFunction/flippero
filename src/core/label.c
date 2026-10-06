// SPDX-License-Identifier: GPL-3.0-or-later
#include "label.h"
#include "font5x7.h"

#include "../third_party/qrcodegen/qrcodegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QR_MAX_VERSION 6
/* No reserved quiet zone inside the content area: the 4 px label margin plus the ~1 mm of
 * paper the printhead doesn't cover (it prints 12 of the label's 14 mm) already surround the
 * code. Reserving 2 modules on top of that cut a typical URL's code from 3 px to 2 px per
 * module, i.e. about 65% of the label height instead of 90%. */
#define QR_QUIET       0
#define QR_MIN_SCALE   2 /* px per module; below this a 203 dpi head is not scannable */

const char* const label_layout_names[LabelLayoutCount] = {
    "Text",
    "QR",
    "QR + text",
    "Icon + text",
};

const char* const label_icon_names[LABEL_ICON_COUNT] = {
    "Warning",
    "Arrow",
    "Check",
    "Cross",
    "Heart",
    "Star",
};

/* 16x16 icons, '#' = black. */
static const char* const icon_art[LABEL_ICON_COUNT][16] = {
    {
        // Warning
        "................",
        ".......##.......",
        ".......##.......",
        "......####......",
        "......####......",
        ".....##..##.....",
        ".....##..##.....",
        "....###..###....",
        "....###..###....",
        "...####..####...",
        "..#####..#####..",
        "..############..",
        ".######..######.",
        ".##############.",
        "################",
        "................",
    },
    {
        // Arrow
        "................",
        ".........#......",
        ".........##.....",
        ".........###....",
        "..............#.",
        "################",
        "################",
        "################",
        "################",
        "..............#.",
        ".........###....",
        ".........##.....",
        ".........#......",
        "................",
        "................",
        "................",
    },
    {
        // Check
        "................",
        "..............##",
        ".............###",
        "............###.",
        "...........###..",
        "..........###...",
        ".##......###....",
        ".###....###.....",
        "..###..###......",
        "...######.......",
        "....####........",
        ".....##.........",
        "................",
        "................",
        "................",
        "................",
    },
    {
        // Cross
        "................",
        ".###........###.",
        ".####......####.",
        "..####....####..",
        "...####..####...",
        "....########....",
        ".....######.....",
        "......####......",
        ".....######.....",
        "....########....",
        "...####..####...",
        "..####....####..",
        ".####......####.",
        ".###........###.",
        "................",
        "................",
    },
    {
        // Heart
        "................",
        "..####....####..",
        ".######..######.",
        "################",
        "################",
        "################",
        "################",
        ".##############.",
        "..############..",
        "...##########...",
        "....########....",
        ".....######.....",
        "......####......",
        ".......##.......",
        "................",
        "................",
    },
    {
        // Star
        ".......##.......",
        ".......##.......",
        "......####......",
        "......####......",
        "################",
        ".##############.",
        "..############..",
        "...##########...",
        "...##########...",
        "..############..",
        "..####....####..",
        ".####......####.",
        ".###........###.",
        "................",
        "................",
        "................",
    },
};

void label_default(Label* label) {
    memset(label, 0, sizeof(Label));
    label->layout = LabelLayoutText;
    label->scale = LABEL_SCALE_AUTO;
    label->border = 0;
    label->qr_ecc = LabelEccLow;
    strcpy(label->line1, "Hello");
    strcpy(label->line2, "Flipper");
    strcpy(label->qr, "https://flipper.net");
}

typedef struct {
    int x0, y0, x1, y1; /* inclusive-exclusive content area */
} Area;

static int area_w(const Area* a) {
    return a->x1 - a->x0;
}
static int area_h(const Area* a) {
    return a->y1 - a->y0;
}

static int widest_line(const char* const* lines, int n, int scale, int gap, bool bold) {
    int widest = 0;
    for(int i = 0; i < n; i++) {
        int w = font_text_width_gap(lines[i], scale, gap, bold);
        if(w > widest) widest = w;
    }
    return widest;
}

/* Draw up to two lines of text into `a`, shrinking from `max_scale` until it fits. At each size
 * the normal letter spacing is tried first, then half of it, before dropping to a smaller size
 * (a smaller size is far more noticeable than slightly tighter spacing). Centered horizontally
 * when `center` is set, left-aligned otherwise. */
static void draw_text_block(
    Bitmap* bmp,
    const Area* a,
    const char* l1,
    const char* l2,
    int max_scale,
    bool bold,
    bool center) {
    const char* ls[2];
    int n = 0;
    if(l1[0]) ls[n++] = l1;
    if(l2[0]) ls[n++] = l2;
    if(n == 0) return;

    int scale = max_scale;
    int gap = 1;
    for(; scale >= 1; scale--) {
        int height = n * font_text_height(scale) + (n - 1) * scale * 2;
        if(scale > 1 && height > area_h(a)) continue;

        int normal = scale;
        int tight = scale / 2 < 1 ? 1 : scale / 2;
        if(widest_line(ls, n, scale, normal, bold) <= area_w(a)) {
            gap = normal;
            break;
        }
        if(tight < normal && widest_line(ls, n, scale, tight, bold) <= area_w(a)) {
            gap = tight;
            break;
        }
        if(scale == 1) {
            gap = 1; /* nothing smaller to try; draw it anyway */
            break;
        }
    }

    int height = n * font_text_height(scale) + (n - 1) * scale * 2;
    int y = a->y0 + (area_h(a) - height) / 2;
    for(int i = 0; i < n; i++) {
        /* Text that doesn't fit even at the smallest size is cut: anything past the area would
         * land beyond the end of the label and not print, while the preview would still show it. */
        char buf[LABEL_LINE_MAX + 1];
        snprintf(buf, sizeof(buf), "%s", ls[i]);
        size_t len = strlen(buf);
        while(len > 0 && font_text_width_gap(buf, scale, gap, bold) > area_w(a)) buf[--len] = 0;

        int w = font_text_width_gap(buf, scale, gap, bold);
        int x = center ? a->x0 + (area_w(a) - w) / 2 : a->x0;
        font_draw_text_gap(bmp, x, y, buf, scale, gap, bold);
        y += font_text_height(scale) + scale * 2;
    }
}

static void draw_error(Bitmap* bmp, const Area* a, const char* msg) {
    draw_text_block(bmp, a, msg, "", 2, false, true);
}

/* Draws the QR for `text` in a square no larger than the area height, at the
 * left edge or centered. Returns the square's side in px, or 0 on failure. */
static int draw_qr(Bitmap* bmp, const Area* a, const char* text, LabelEcc ecc, bool center) {
    static const enum qrcodegen_Ecc eccs[LabelEccCount] = {
        qrcodegen_Ecc_LOW,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_Ecc_QUARTILE,
    };
    uint8_t* qr = malloc(qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION));
    uint8_t* tmp = malloc(qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION));
    int used = 0;

    if(qr && tmp && text[0] &&
       qrcodegen_encodeText(
           text, tmp, qr, eccs[ecc], 1, QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) {
        int n = qrcodegen_getSize(qr);
        int scale = area_h(a) / (n + 2 * QR_QUIET);
        if(scale >= QR_MIN_SCALE) {
            int side = (n + 2 * QR_QUIET) * scale;
            int ox = (center ? a->x0 + (area_w(a) - side) / 2 : a->x0) + QR_QUIET * scale;
            int oy = a->y0 + (area_h(a) - side) / 2 + QR_QUIET * scale;
            for(int y = 0; y < n; y++)
                for(int x = 0; x < n; x++)
                    if(qrcodegen_getModule(qr, x, y))
                        bitmap_fill_rect(bmp, ox + x * scale, oy + y * scale, scale, scale, true);
            used = side;
        }
    }
    free(qr);
    free(tmp);
    return used;
}

static void draw_icon(Bitmap* bmp, const Area* a, int icon, int* used_w) {
    int scale = area_h(a) / 16;
    if(scale < 1) scale = 1;
    int oy = a->y0 + (area_h(a) - 16 * scale) / 2;
    for(int y = 0; y < 16; y++)
        for(int x = 0; x < 16; x++)
            if(icon_art[icon][y][x] == '#')
                bitmap_fill_rect(bmp, a->x0 + x * scale, oy + y * scale, scale, scale, true);
    *used_w = 16 * scale;
}

bool label_render(const Label* label, Bitmap* bmp) {
    bitmap_clear(bmp);

    int margin = 4;
    int usable_w = bmp->w - LABEL_LEAD_PX; /* the part that actually lands on the label */
    if(label->border) {
        /* frame from the very start of the printable area (x >= 0) to 2 dots before its end */
        bitmap_frame(bmp, 0, 2, usable_w - 2 - LABEL_SHIFT_PX, bmp->h - 4, 2);
        margin = 8;
    }
    /* a little more room at the end than at the start: the leading edge has its own offset */
    Area area = {
        margin - LABEL_SHIFT_PX,
        margin,
        usable_w - (label->border ? margin : 6) - LABEL_SHIFT_PX,
        bmp->h - margin};
    int max_scale = label->scale == LABEL_SCALE_AUTO ? LABEL_SCALE_MAX : label->scale;
    bool ok = true;

    switch(label->layout) {
    case LabelLayoutText:
        draw_text_block(bmp, &area, label->line1, label->line2, max_scale, label->bold, true);
        break;

    case LabelLayoutQr:
        if(draw_qr(bmp, &area, label->qr, label->qr_ecc, true) == 0) {
            ok = false;
            draw_error(bmp, &area, "QR TOO LONG");
        }
        break;

    case LabelLayoutQrText: {
        int w = draw_qr(bmp, &area, label->qr, label->qr_ecc, false);
        if(w == 0) {
            ok = false;
            draw_error(bmp, &area, "QR TOO LONG");
            break;
        }
        Area text = {area.x0 + w + 5, area.y0, area.x1, area.y1};
        draw_text_block(bmp, &text, label->line1, label->line2, max_scale, label->bold, false);
        break;
    }

    case LabelLayoutIconText: {
        int w = 0;
        draw_icon(bmp, &area, label->icon % LABEL_ICON_COUNT, &w);
        Area text = {area.x0 + w + 8, area.y0, area.x1, area.y1};
        draw_text_block(bmp, &text, label->line1, label->line2, max_scale, label->bold, false);
        break;
    }
    }
    return ok;
}
