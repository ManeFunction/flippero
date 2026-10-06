// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bitmap.h"

/* Embedded 5x7 ASCII font (0x20..0x7E) with a descender row, so each glyph
 * cell is 5 columns x 8 rows. The Flipper canvas cannot be read back, so the
 * built-in Flipper fonts cannot render into the print buffer; this font is
 * integer-scaled instead. */

#define FONT_CELL_W 5
#define FONT_CELL_H 8

/* Column bitmap of one glyph: FONT_CELL_W bytes, bit 0 = top row (non-ASCII -> "?"). */
const uint8_t* font5x7_glyph(char c);

/* Pixel width of a string at `scale`; the gap between glyphs is one font column (= scale px). */
int font_text_width(const char* s, int scale, bool bold);
/* Same with an explicit gap in pixels between glyphs (to squeeze text into a tight space). */
int font_text_width_gap(const char* s, int scale, int gap, bool bold);
int font_text_height(int scale);

/* Draw with the top-left of the first glyph cell at (x, y). */
void font_draw_text(Bitmap* bmp, int x, int y, const char* s, int scale, bool bold);
void font_draw_text_gap(Bitmap* bmp, int x, int y, const char* s, int scale, int gap, bool bold);
