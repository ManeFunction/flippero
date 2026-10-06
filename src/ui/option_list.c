// SPDX-License-Identifier: GPL-3.0-or-later
#include "option_list.h"

#include <furi.h>
#include <gui/elements.h>

#define ROW_H      12
#define VISIBLE    5 /* 5 * 12 = 60 px */
#define LABEL_X    4
#define ARROW_L_X  45
#define ARROW_R_X  117
#define VALUE_MID  81 /* centre of the value zone, between the arrows */
#define TEXT_X     47
#define TEXT_MAX_W 66
#define ROW_W      123

struct OptionList {
    View* view;
    OptionListChangeCallback on_change;
    OptionListEnterCallback on_enter;
    void* context;
};

typedef struct {
    OptionItem items[OPTION_LIST_MAX_ITEMS];
    uint8_t count;
    uint8_t selected;
    uint8_t scroll;
} OptionListModel;

/* Draw `text` at x, shortened with ".." so it fits `max_w` pixels. */
static void draw_fitted(Canvas* canvas, uint8_t x, uint8_t y, const char* text, uint8_t max_w) {
    char buf[40];
    size_t n = strlen(text);
    if(n > sizeof(buf) - 3) n = sizeof(buf) - 3;
    memcpy(buf, text, n);
    buf[n] = 0;

    if(canvas_string_width(canvas, buf) > max_w) {
        while(n > 1 && canvas_string_width(canvas, buf) + 8 > max_w) buf[--n] = 0;
        buf[n] = '.';
        buf[n + 1] = '.';
        buf[n + 2] = 0;
    }
    canvas_draw_str(canvas, x, y, buf);
}

static void option_list_draw(Canvas* canvas, void* _model) {
    const OptionListModel* m = _model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t row = 0; row < VISIBLE; row++) {
        uint8_t i = m->scroll + row;
        if(i >= m->count) break;
        const OptionItem* it = &m->items[i];
        uint8_t y = row * ROW_H;
        uint8_t base = y + ROW_H - 3; /* text baseline */
        bool sel = i == m->selected;

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, y, ROW_W, ROW_H - 1);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, LABEL_X, base, it->label);

        switch(it->kind) {
        case OptionItemChoice:
            if(it->count > 1) {
                canvas_draw_str(canvas, ARROW_L_X, base, "<");
                canvas_draw_str(canvas, ARROW_R_X, base, ">");
            }
            canvas_draw_str_aligned(
                canvas, VALUE_MID, base, AlignCenter, AlignBottom, it->choices[it->index]);
            break;
        case OptionItemText:
            draw_fitted(canvas, TEXT_X, base, it->text[0] ? it->text : "(empty)", TEXT_MAX_W);
            break;
        case OptionItemAction:
            canvas_draw_str(canvas, ARROW_R_X, base, ">");
            break;
        }
    }

    canvas_set_color(canvas, ColorBlack);
    elements_scrollbar(canvas, m->selected, m->count);
}

static void select_item(OptionListModel* m, int delta) {
    if(!m->count) return;
    int s = (int)m->selected + delta;
    if(s < 0) s = m->count - 1;
    if(s >= m->count) s = 0;
    m->selected = s;
    if(m->selected < m->scroll) m->scroll = m->selected;
    if(m->selected >= m->scroll + VISIBLE) m->scroll = m->selected - VISIBLE + 1;
}

static bool option_list_input(InputEvent* event, void* context) {
    OptionList* list = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;
    bool changed = false, entered = false;
    uint8_t item = 0, choice = 0;

    with_view_model(
        list->view,
        OptionListModel * m,
        {
            OptionItem* it = m->count ? &m->items[m->selected] : NULL;
            switch(event->key) {
            case InputKeyUp:
                select_item(m, -1);
                break;
            case InputKeyDown:
                select_item(m, 1);
                break;
            case InputKeyLeft:
            case InputKeyRight:
                if(it && it->kind == OptionItemChoice && it->count > 1) {
                    int d = event->key == InputKeyRight ? 1 : -1;
                    int idx = ((int)it->index + d + it->count) % it->count;
                    it->index = idx;
                    changed = true;
                    item = m->selected;
                    choice = idx;
                }
                break;
            case InputKeyOk:
                if(!it) break;
                if(event->type != InputTypeShort) break;
                if(it->kind == OptionItemChoice) {
                    if(it->count > 1) {
                        it->index = (it->index + 1) % it->count;
                        changed = true;
                        item = m->selected;
                        choice = it->index;
                    }
                } else {
                    entered = true;
                    item = m->selected;
                }
                break;
            default:
                consumed = false;
                break;
            }
        },
        consumed);

    /* Callbacks run outside the model lock: they may rebuild the list. */
    if(changed && list->on_change) list->on_change(list->context, item, choice);
    if(entered && list->on_enter) list->on_enter(list->context, item);
    return consumed;
}

OptionList* option_list_alloc(void) {
    OptionList* list = malloc(sizeof(OptionList));
    memset(list, 0, sizeof(OptionList));
    list->view = view_alloc();
    view_set_context(list->view, list);
    view_allocate_model(list->view, ViewModelTypeLocking, sizeof(OptionListModel));
    view_set_draw_callback(list->view, option_list_draw);
    view_set_input_callback(list->view, option_list_input);
    option_list_reset(list);
    return list;
}

void option_list_free(OptionList* list) {
    view_free(list->view);
    free(list);
}

View* option_list_get_view(OptionList* list) {
    return list->view;
}

void option_list_reset(OptionList* list) {
    with_view_model(
        list->view,
        OptionListModel * m,
        {
            m->count = 0;
            m->selected = 0;
            m->scroll = 0;
        },
        true);
}

uint8_t option_list_add(OptionList* list, const OptionItem* item) {
    uint8_t idx = 0;
    with_view_model(
        list->view,
        OptionListModel * m,
        {
            if(m->count < OPTION_LIST_MAX_ITEMS) m->items[m->count++] = *item;
            idx = m->count - 1;
        },
        true);
    return idx;
}

void option_list_set_callbacks(
    OptionList* list,
    OptionListChangeCallback on_change,
    OptionListEnterCallback on_enter,
    void* context) {
    list->on_change = on_change;
    list->on_enter = on_enter;
    list->context = context;
}

void option_list_set_selected(OptionList* list, uint8_t item) {
    with_view_model(
        list->view,
        OptionListModel * m,
        {
            if(item < m->count) {
                m->selected = item;
                m->scroll = item >= VISIBLE ? item - VISIBLE + 1 : 0;
            }
        },
        true);
}
