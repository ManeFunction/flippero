// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bitmap.h"

#define LABEL_LINE_MAX 24
#define LABEL_QR_MAX   64

typedef enum {
    LabelLayoutText,
    LabelLayoutQr,
    LabelLayoutQrText,
    LabelLayoutIconText,
    LabelLayoutCount,
} LabelLayout;

typedef enum {
    LabelEccLow,
    LabelEccMedium,
    LabelEccQuartile,
    LabelEccCount,
} LabelEcc;

/* The printer starts printing ~1.6-2 mm after the label's leading edge (measured from a photo
 * of a printed label), so the last LABEL_LEAD_PX dots of a label-length image fall past the
 * label's end and are lost. Layouts keep everything inside [0, width - LABEL_LEAD_PX). */
#define LABEL_LEAD_PX 16

/* Content is shifted this many dots toward the start of the label (0.5 mm). It cannot go
 * further: image column 0 is already the first thing the head prints, ~1.6-2 mm in. */
#define LABEL_SHIFT_PX 4

#define LABEL_SCALE_AUTO 0
#define LABEL_SCALE_MAX  8
#define LABEL_ICON_COUNT 6

typedef struct {
    uint8_t layout; /* LabelLayout */
    uint8_t scale; /* LABEL_SCALE_AUTO or 1..LABEL_SCALE_MAX (upper bound; always shrinks to fit) */
    uint8_t bold;
    uint8_t border;
    uint8_t icon;
    uint8_t qr_ecc; /* LabelEcc */
    char line1[LABEL_LINE_MAX + 1];
    char line2[LABEL_LINE_MAX + 1];
    char qr[LABEL_QR_MAX + 1];
} Label;

extern const char* const label_layout_names[LabelLayoutCount];
extern const char* const label_icon_names[LABEL_ICON_COUNT];

void label_default(Label* label);

/* Render into `bmp` (size taken from bmp->w x BITMAP_H). The bitmap is
 * cleared first. Returns false if the QR payload could not be encoded at a
 * scannable size; an error message is rendered in that case. */
bool label_render(const Label* label, Bitmap* bmp);
