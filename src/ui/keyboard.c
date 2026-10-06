// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard.h"

#include "../core/font5x7.h"

#include <furi.h>
#include <gui/elements.h>

#define KB_ROWS 5
#define KB_COLS 11
#define KEY_H   8
#define KEYS_Y  23 /* top of the first key row */

#define CODE_BACKSPACE 0x08
#define CODE_SHIFT     0x0E

typedef enum {
    KeyChar,
    KeyBackspace,
    KeyShift,
    KeySym,
    KeySpace,
    KeyLeft,
    KeyRight,
    KeySave,
} KeyKind;

typedef struct {
    uint8_t kind;
    char ch;
    uint8_t col;
    uint8_t span;
} Key;

typedef enum {
    PageLower,
    PageUpper,
    PageSymbols,
} Page;

/* Row 0 (digits + backspace) and the bottom row are the same on every page. */
static const char row_digits[] = "1234567890\x08";
static const char* const letter_rows[3][3] = {
    {"qwertyuiop@", "asdfghjkl./", "\x0Ezxcvbnm-_:"}, /* lower */
    {"QWERTYUIOP@", "ASDFGHJKL./", "\x0EZXCVBNM-_:"}, /* upper */
    {"!\"#$%&'()*+", ",;<=>?[]{}|", "\\^`~-_:@./#"}, /* symbols */
};

struct Keyboard {
    View* view;
    KeyboardCallback callback;
    void* context;
};

typedef struct {
    char header[24];
    char text[KEYBOARD_TEXT_MAX + 1];
    uint8_t cap; /* max characters */
    uint8_t cursor;
    uint8_t page; /* Page */
    uint8_t letters_page; /* page to return to from symbols */
    uint8_t row;
    uint8_t idx;
    uint8_t sel_col; /* column kept while moving up/down */
} KeyboardModel;

/* ---- layout --------------------------------------------------------------- */

static uint8_t build_row(uint8_t page, uint8_t row, Key out[KB_COLS]) {
    uint8_t n = 0;
    if(row == 4) {
        out[n++] = (Key){KeySym, 0, 0, 2};
        out[n++] = (Key){KeySpace, ' ', 2, 5};
        out[n++] = (Key){KeyLeft, 0, 7, 1};
        out[n++] = (Key){KeyRight, 0, 8, 1};
        out[n++] = (Key){KeySave, 0, 9, 2};
        return n;
    }
    const char* chars = row == 0 ? row_digits : letter_rows[page][row - 1];
    for(uint8_t c = 0; c < KB_COLS && chars[c]; c++) {
        char ch = chars[c];
        KeyKind kind = KeyChar;
        if(ch == CODE_BACKSPACE) kind = KeyBackspace;
        if(ch == CODE_SHIFT) kind = KeyShift;
        out[n++] = (Key){kind, ch, c, 1};
    }
    return n;
}

static uint8_t key_at_col(const Key* keys, uint8_t n, uint8_t col) {
    for(uint8_t i = 0; i < n; i++)
        if(col >= keys[i].col && col < keys[i].col + keys[i].span) return i;
    return n - 1;
}

/* ---- drawing -------------------------------------------------------------- */

static uint8_t col_x(uint8_t col) {
    return (uint8_t)((col * 128) / KB_COLS);
}

static void draw_text_field(Canvas* canvas, const KeyboardModel* m) {
    const uint8_t field_w = 116;
    char tmp[KEYBOARD_TEXT_MAX + 1];

    /* scroll so the cursor stays visible */
    uint8_t start = 0;
    for(; start < m->cursor; start++) {
        memcpy(tmp, m->text + start, m->cursor - start);
        tmp[m->cursor - start] = 0;
        if(canvas_string_width(canvas, tmp) <= field_w - 4) break;
    }
    uint8_t cursor_x = 0;
    {
        memcpy(tmp, m->text + start, m->cursor - start);
        tmp[m->cursor - start] = 0;
        cursor_x = canvas_string_width(canvas, tmp);
    }

    size_t n = strlen(m->text + start);
    memcpy(tmp, m->text + start, n);
    tmp[n] = 0;
    while(n > 0 && canvas_string_width(canvas, tmp) > field_w) tmp[--n] = 0;

    elements_slightly_rounded_frame(canvas, 1, 9, 126, 13);
    canvas_draw_str(canvas, 5, 19, tmp);
    canvas_draw_line(canvas, 5 + cursor_x, 11, 5 + cursor_x, 19); /* cursor */
}

static void draw_key(Canvas* canvas, const Key* k, uint8_t row, bool sel, uint8_t page) {
    uint8_t x0 = col_x(k->col);
    uint8_t x1 = col_x(k->col + k->span);
    uint8_t w = x1 - x0;
    uint8_t y = KEYS_Y + row * KEY_H;
    int cx = x0 + w / 2;
    int my = y + 4;

    if(sel) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, x0, y, w - 1, KEY_H);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    switch(k->kind) {
    case KeyChar: {
        /* Drawn with the embedded 5x7 font: the Flipper fonts have brackets and punctuation
         * taller than a key row, which overlapped neighbouring rows. */
        const uint8_t* g = font5x7_glyph(k->ch);
        for(uint8_t c = 0; c < FONT_CELL_W; c++)
            for(uint8_t r = 0; r < FONT_CELL_H; r++)
                if(g[c] & (1 << r)) canvas_draw_dot(canvas, cx - FONT_CELL_W / 2 + c, y + r);
        break;
    }
    case KeyBackspace: /* <- arrow */
        canvas_draw_line(canvas, cx - 3, my, cx + 3, my);
        canvas_draw_line(canvas, cx - 3, my, cx - 1, my - 2);
        canvas_draw_line(canvas, cx - 3, my, cx - 1, my + 2);
        break;
    case KeyShift: /* up arrow; solid stem when in UPPER */
        canvas_draw_line(canvas, cx, my - 2, cx, my + 2);
        canvas_draw_line(canvas, cx - 2, my, cx, my - 2);
        canvas_draw_line(canvas, cx + 2, my, cx, my - 2);
        if(page == PageUpper) canvas_draw_line(canvas, cx - 2, my + 3, cx + 2, my + 3);
        break;
    case KeyLeft:
        canvas_draw_line(canvas, cx - 2, my, cx + 2, my);
        canvas_draw_line(canvas, cx - 2, my, cx, my - 2);
        canvas_draw_line(canvas, cx - 2, my, cx, my + 2);
        break;
    case KeyRight:
        canvas_draw_line(canvas, cx - 2, my, cx + 2, my);
        canvas_draw_line(canvas, cx + 2, my, cx, my - 2);
        canvas_draw_line(canvas, cx + 2, my, cx, my + 2);
        break;
    case KeySym:
        canvas_draw_str_aligned(
            canvas, cx, y + KEY_H - 1, AlignCenter, AlignBottom, page == PageSymbols ? "abc" : "#+=");
        break;
    case KeySpace:
        canvas_draw_line(canvas, cx - 10, my + 1, cx - 10, my + 2);
        canvas_draw_line(canvas, cx - 10, my + 2, cx + 10, my + 2);
        canvas_draw_line(canvas, cx + 10, my + 1, cx + 10, my + 2);
        break;
    case KeySave:
        canvas_draw_str_aligned(canvas, cx, y + KEY_H - 1, AlignCenter, AlignBottom, "OK");
        break;
    }
    canvas_set_color(canvas, ColorBlack);
}

static void keyboard_draw(Canvas* canvas, void* _model) {
    const KeyboardModel* m = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 7, m->header);

    draw_text_field(canvas, m);

    canvas_set_font(canvas, FontSecondary); /* FontKeyboard glyphs are taller than a key row */
    for(uint8_t row = 0; row < KB_ROWS; row++) {
        Key keys[KB_COLS];
        uint8_t n = build_row(m->page, row, keys);
        for(uint8_t i = 0; i < n; i++)
            draw_key(canvas, &keys[i], row, row == m->row && i == m->idx, m->page);
    }
}

/* ---- input ---------------------------------------------------------------- */

static void insert_char(KeyboardModel* m, char ch) {
    size_t len = strlen(m->text);
    if(len >= m->cap) return;
    memmove(m->text + m->cursor + 1, m->text + m->cursor, len - m->cursor + 1);
    m->text[m->cursor++] = ch;
}

static void backspace(KeyboardModel* m) {
    if(m->cursor == 0) return;
    size_t len = strlen(m->text);
    memmove(m->text + m->cursor - 1, m->text + m->cursor, len - m->cursor + 1);
    m->cursor--;
}

static void move_selection(KeyboardModel* m, InputKey key) {
    Key keys[KB_COLS];
    uint8_t n = build_row(m->page, m->row, keys);
    if(key == InputKeyLeft || key == InputKeyRight) {
        m->idx = (key == InputKeyRight) ? (m->idx + 1) % n : (m->idx + n - 1) % n;
        m->sel_col = keys[m->idx].col;
        return;
    }
    m->row = (key == InputKeyDown) ? (m->row + 1) % KB_ROWS : (m->row + KB_ROWS - 1) % KB_ROWS;
    n = build_row(m->page, m->row, keys);
    m->idx = key_at_col(keys, n, m->sel_col);
}

/* Returns true if the OK key was "save". */
static bool press_key(KeyboardModel* m) {
    Key keys[KB_COLS];
    build_row(m->page, m->row, keys);
    const Key* k = &keys[m->idx];
    switch(k->kind) {
    case KeyChar:
    case KeySpace:
        insert_char(m, k->ch);
        break;
    case KeyBackspace:
        backspace(m);
        break;
    case KeyShift:
        m->page = m->page == PageUpper ? PageLower : PageUpper;
        m->letters_page = m->page;
        break;
    case KeySym:
        m->page = m->page == PageSymbols ? m->letters_page : PageSymbols;
        break;
    case KeyLeft:
        if(m->cursor > 0) m->cursor--;
        break;
    case KeyRight:
        if(m->cursor < strlen(m->text)) m->cursor++;
        break;
    case KeySave:
        return true;
    }
    /* the new page may have fewer keys in this row (e.g. no shift on the symbols page) */
    uint8_t n = build_row(m->page, m->row, keys);
    if(m->idx >= n) m->idx = n - 1;
    return false;
}

static bool keyboard_input(InputEvent* event, void* context) {
    Keyboard* kb = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true, save = false;
    with_view_model(
        kb->view,
        KeyboardModel * m,
        {
            switch(event->key) {
            case InputKeyUp:
            case InputKeyDown:
            case InputKeyLeft:
            case InputKeyRight:
                move_selection(m, event->key);
                break;
            case InputKeyOk:
                if(event->type == InputTypeShort) save = press_key(m);
                break;
            default:
                consumed = false; /* Back cancels */
                break;
            }
        },
        consumed);

    if(save && kb->callback) kb->callback(kb->context);
    return consumed;
}

/* ---- public --------------------------------------------------------------- */

Keyboard* keyboard_alloc(void) {
    Keyboard* kb = malloc(sizeof(Keyboard));
    memset(kb, 0, sizeof(Keyboard));
    kb->view = view_alloc();
    view_set_context(kb->view, kb);
    view_allocate_model(kb->view, ViewModelTypeLocking, sizeof(KeyboardModel));
    view_set_draw_callback(kb->view, keyboard_draw);
    view_set_input_callback(kb->view, keyboard_input);
    return kb;
}

void keyboard_free(Keyboard* kb) {
    view_free(kb->view);
    free(kb);
}

View* keyboard_get_view(Keyboard* kb) {
    return kb->view;
}

void keyboard_setup(Keyboard* kb, const char* header, const char* text, size_t capacity) {
    with_view_model(
        kb->view,
        KeyboardModel * m,
        {
            memset(m, 0, sizeof(KeyboardModel));
            snprintf(m->header, sizeof(m->header), "%s", header);
            snprintf(m->text, sizeof(m->text), "%s", text);
            m->cap = capacity > 0 && capacity - 1 < KEYBOARD_TEXT_MAX ? capacity - 1 : KEYBOARD_TEXT_MAX;
            m->text[m->cap] = 0;
            m->cursor = strlen(m->text);
            m->page = PageLower;
            m->letters_page = PageLower;
            m->row = 1; /* start on the letters */
            m->idx = 0;
            m->sel_col = 0;
        },
        true);
}

void keyboard_set_callback(Keyboard* kb, KeyboardCallback cb, void* context) {
    kb->callback = cb;
    kb->context = context;
}

void keyboard_get_text(Keyboard* kb, char* out, size_t out_size) {
    with_view_model(
        kb->view, KeyboardModel * m, { snprintf(out, out_size, "%s", m->text); }, false);
}
