// SPDX-License-Identifier: GPL-3.0-or-later
#include "bitmap.h"

#include <stdlib.h>
#include <string.h>

Bitmap* bitmap_alloc(void) {
    Bitmap* bmp = malloc(sizeof(Bitmap));
    bmp->data = malloc((BITMAP_MAX_W / 8) * BITMAP_H);
    bmp->h = BITMAP_H;
    bitmap_reshape(bmp, 240);
    return bmp;
}

void bitmap_free(Bitmap* bmp) {
    if(!bmp) return;
    free(bmp->data);
    free(bmp);
}

void bitmap_reshape(Bitmap* bmp, uint16_t w) {
    if(w > BITMAP_MAX_W) w = BITMAP_MAX_W;
    if(w < 8) w = 8;
    bmp->w = w;
    bmp->stride = (w + 7) / 8;
    bitmap_clear(bmp);
}

void bitmap_clear(Bitmap* bmp) {
    memset(bmp->data, 0, (size_t)bmp->stride * bmp->h);
}

void bitmap_set(Bitmap* bmp, int x, int y, bool on) {
    if(x < 0 || y < 0 || x >= bmp->w || y >= bmp->h) return;
    uint8_t* p = &bmp->data[(size_t)y * bmp->stride + (x >> 3)];
    uint8_t mask = 0x80 >> (x & 7);
    if(on)
        *p |= mask;
    else
        *p &= ~mask;
}

bool bitmap_get(const Bitmap* bmp, int x, int y) {
    if(x < 0 || y < 0 || x >= bmp->w || y >= bmp->h) return false;
    return bmp->data[(size_t)y * bmp->stride + (x >> 3)] & (0x80 >> (x & 7));
}

void bitmap_fill_rect(Bitmap* bmp, int x, int y, int w, int h, bool on) {
    for(int j = y; j < y + h; j++)
        for(int i = x; i < x + w; i++)
            bitmap_set(bmp, i, j, on);
}

void bitmap_frame(Bitmap* bmp, int x, int y, int w, int h, int t) {
    bitmap_fill_rect(bmp, x, y, w, t, true);
    bitmap_fill_rect(bmp, x, y + h - t, w, t, true);
    bitmap_fill_rect(bmp, x, y, t, h, true);
    bitmap_fill_rect(bmp, x + w - t, y, t, h, true);
}
