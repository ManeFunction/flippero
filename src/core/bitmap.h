// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 1-bit label canvas, row-major, MSB first, 1 = black.
 *
 * Coordinates are *logical* (landscape): x runs along the label length,
 * y runs across the 96 px printhead. The transposition into the printer's
 * raster orientation happens in fichero_bitmap_to_raster(). */

#define BITMAP_MAX_W 400 /* 50 mm at 8 px/mm */
#define BITMAP_H     96  /* printhead width in dots */

typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t stride; /* bytes per row, derived from w */
    uint8_t* data;   /* BITMAP_MAX_W/8 * BITMAP_H bytes, allocated once */
} Bitmap;

Bitmap* bitmap_alloc(void);
void bitmap_free(Bitmap* bmp);

/* Change the logical width (clamped to BITMAP_MAX_W) and clear. */
void bitmap_reshape(Bitmap* bmp, uint16_t w);

void bitmap_clear(Bitmap* bmp);
void bitmap_set(Bitmap* bmp, int x, int y, bool on);
bool bitmap_get(const Bitmap* bmp, int x, int y);
void bitmap_fill_rect(Bitmap* bmp, int x, int y, int w, int h, bool on);
void bitmap_frame(Bitmap* bmp, int x, int y, int w, int h, int thickness);
