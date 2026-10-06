// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gui/view.h>

/* A settings-style list: label on the left, a wide value zone on the right.
 * Replaces the stock VariableItemList, whose value area is a fixed ~42 px with the
 * `<` `>` arrows drawn at x=73 and x=115, so values longer than ~7 characters
 * collide with the arrows. Here the value zone is ~70 px.
 *
 *   Choice: Left/Right (or OK) cycle through `choices`
 *   Text:   shows a live string; OK calls the enter callback
 *   Action: label only; OK calls the enter callback */

#define OPTION_LIST_MAX_ITEMS 12

typedef enum {
    OptionItemChoice,
    OptionItemText,
    OptionItemAction,
} OptionItemKind;

typedef struct {
    const char* label;
    OptionItemKind kind;
    const char* const* choices; /* Choice: static array of `count` strings */
    uint8_t count;
    uint8_t index; /* Choice: current choice */
    const char* text; /* Text: must stay valid while the list is shown */
} OptionItem;

typedef struct OptionList OptionList;

/* A choice was changed to `choice` (index into its choices). */
typedef void (*OptionListChangeCallback)(void* context, uint8_t item, uint8_t choice);
/* OK was pressed on a Text or Action item. */
typedef void (*OptionListEnterCallback)(void* context, uint8_t item);

OptionList* option_list_alloc(void);
void option_list_free(OptionList* list);
View* option_list_get_view(OptionList* list);

void option_list_reset(OptionList* list);
uint8_t option_list_add(OptionList* list, const OptionItem* item);
void option_list_set_callbacks(
    OptionList* list,
    OptionListChangeCallback on_change,
    OptionListEnterCallback on_enter,
    void* context);

void option_list_set_selected(OptionList* list, uint8_t item);
