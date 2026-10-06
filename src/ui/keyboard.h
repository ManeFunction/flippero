// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gui/view.h>

/* On-screen keyboard with punctuation, for URLs and e-mail addresses. The stock
 * Flipper text input only has a-z, 0-9 and "_" (no "." or "/"), so this replaces it.
 *
 * Three pages: lower case, UPPER CASE, symbols. Letters-page extras: @ . / - _ :
 * Bottom row: sym/abc toggle, space, cursor left/right, OK (save).
 * Back cancels. */

#define KEYBOARD_TEXT_MAX 64

typedef struct Keyboard Keyboard;
typedef void (*KeyboardCallback)(void* context);

Keyboard* keyboard_alloc(void);
void keyboard_free(Keyboard* kb);
View* keyboard_get_view(Keyboard* kb);

/* Header line, initial text, and the capacity (including the NUL) of the target buffer. */
void keyboard_setup(Keyboard* kb, const char* header, const char* text, size_t capacity);
void keyboard_set_callback(Keyboard* kb, KeyboardCallback cb, void* context);
void keyboard_get_text(Keyboard* kb, char* out, size_t out_size);
