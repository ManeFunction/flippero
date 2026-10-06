// SPDX-License-Identifier: GPL-3.0-or-later
#include "fichero_proto.h"

#include <string.h>

static size_t put(uint8_t* out, const uint8_t* bytes, size_t n) {
    memcpy(out, bytes, n);
    return n;
}

size_t fichero_cmd_density(uint8_t* out, uint8_t level) {
    if(level > 2) level = 2;
    const uint8_t c[] = {0x10, 0xFF, 0x10, 0x00, level};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_paper(uint8_t* out, uint8_t paper) {
    const uint8_t c[] = {0x10, 0xFF, 0x84, paper};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_wake(uint8_t* out) {
    memset(out, 0, FICHERO_WAKE_LEN);
    return FICHERO_WAKE_LEN;
}

size_t fichero_cmd_enable(uint8_t* out) {
    const uint8_t c[] = {0x10, 0xFF, 0xFE, 0x01};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_feed(uint8_t* out) {
    const uint8_t c[] = {0x1D, 0x0C};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_stop(uint8_t* out) {
    const uint8_t c[] = {0x10, 0xFF, 0xFE, 0x45};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_status(uint8_t* out) {
    const uint8_t c[] = {0x10, 0xFF, 0x40};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_battery(uint8_t* out) {
    const uint8_t c[] = {0x10, 0xFF, 0x50, 0xF1};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_model(uint8_t* out) {
    const uint8_t c[] = {0x10, 0xFF, 0x20, 0xF0};
    return put(out, c, sizeof(c));
}

size_t fichero_cmd_raster_header(uint8_t* out, uint16_t rows) {
    const uint8_t c[] = {
        0x1D, 0x76, 0x30, 0x00, FICHERO_HEAD_BYTES, 0x00, (uint8_t)(rows & 0xFF), (uint8_t)(rows >> 8)};
    return put(out, c, sizeof(c));
}

bool fichero_resp_is_ok(const uint8_t* data, size_t len) {
    return len >= 2 && data[0] == 'O' && data[1] == 'K';
}

bool fichero_resp_is_done(const uint8_t* data, size_t len) {
    return (len >= 1 && data[0] == 0xAA) || fichero_resp_is_ok(data, len);
}

size_t fichero_bitmap_to_raster(const Bitmap* bmp, bool flip, uint8_t* out) {
    size_t rows = bmp->w;
    memset(out, 0, rows * FICHERO_HEAD_BYTES);
    for(size_t r = 0; r < rows; r++) {
        for(int c = 0; c < FICHERO_HEAD_PX; c++) {
            /* CW rotation: raster (row r, col c) <- logical (x = r, y = 95-c) */
            int x = flip ? (int)(rows - 1 - r) : (int)r;
            int y = flip ? c : FICHERO_HEAD_PX - 1 - c;
            if(bitmap_get(bmp, x, y)) {
                out[r * FICHERO_HEAD_BYTES + (c >> 3)] |= 0x80 >> (c & 7);
            }
        }
    }
    return rows * FICHERO_HEAD_BYTES;
}
