// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bitmap.h"

/* Fichero D11s / AiYin command set. Pure byte builders; no I/O.
 * Reference: https://github.com/0xMH/fichero-printer (docs/PROTOCOL.md) */

#define FICHERO_HEAD_PX    96
#define FICHERO_HEAD_BYTES 12
#define FICHERO_PX_PER_MM  8 /* 203 dpi */

#define FICHERO_WAKE_LEN 12
#define FICHERO_CMD_MAX  FICHERO_WAKE_LEN /* biggest fixed-size command */

typedef enum {
    FicheroPaperGap = 0,
    FicheroPaperBlackMark = 1,
    FicheroPaperContinuous = 2,
} FicheroPaper;

/* Status byte returned by 10 FF 40 (last byte of the reply). Bit meanings follow the
 * reference web app (web/src/lib/fichero/client.ts). */
#define FICHERO_ST_PRINTING   (1 << 0)
#define FICHERO_ST_COVER_OPEN (1 << 1)
#define FICHERO_ST_NO_PAPER   (1 << 2)
#define FICHERO_ST_LOW_BATT   (1 << 3)
#define FICHERO_ST_OVERHEATED ((1 << 4) | (1 << 6))
#define FICHERO_ST_CHARGING   (1 << 5)

size_t fichero_cmd_density(uint8_t* out, uint8_t level); /* 0..2 */
size_t fichero_cmd_paper(uint8_t* out, uint8_t paper);
size_t fichero_cmd_wake(uint8_t* out);
size_t fichero_cmd_enable(uint8_t* out);
size_t fichero_cmd_feed(uint8_t* out);
size_t fichero_cmd_stop(uint8_t* out);
size_t fichero_cmd_status(uint8_t* out);
size_t fichero_cmd_battery(uint8_t* out);
size_t fichero_cmd_model(uint8_t* out);

/* 1D 76 30 00 <xL xH> <yL yH>: raster header for `rows` rows of 12 bytes. */
#define FICHERO_RASTER_HEADER_LEN 8
size_t fichero_cmd_raster_header(uint8_t* out, uint16_t rows);

/* True if the response starts with "OK". */
bool fichero_resp_is_ok(const uint8_t* data, size_t len);
/* The stop command is acknowledged with 0xAA or "OK". */
bool fichero_resp_is_done(const uint8_t* data, size_t len);

/* Convert the landscape logical bitmap into the printer raster.
 * Output is bmp->w rows x FICHERO_HEAD_BYTES, MSB first, 1 = black, i.e.
 * bmp->w * 12 bytes. The bitmap is rotated 90 degrees clockwise, which is what
 * the reference web app does for the D11s (printDirection "left" -> rotateCW90);
 * `flip` rotates it 180 degrees from that. */
size_t fichero_bitmap_to_raster(const Bitmap* bmp, bool flip, uint8_t* out);
