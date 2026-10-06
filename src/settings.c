// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "core/fichero_proto.h"

#include <furi.h>
#include <storage/storage.h>

#define STATE_PATH  APP_DATA_PATH("state.bin")
#define STATE_MAGIC    0xF1C00004 /* bump when Settings or Label layout changes */
#define STATE_MAGIC_V3 0xF1C00003 /* before the printer name was removed */

const uint8_t settings_length_mm[SETTINGS_LENGTH_COUNT] = {20, 25, 30, 40, 50};

typedef struct {
    uint32_t magic;
    Settings settings;
    Label label;
} SavedState;

/* Previous on-disk layout (had a printer_name field). Still loaded so a saved label survives. */
typedef struct {
    uint8_t density, paper, length_idx, copies, flip, link;
    char printer_name[25];
} SettingsV3;

typedef struct {
    uint32_t magic;
    SettingsV3 settings;
    Label label;
} SavedStateV3;

void settings_default(Settings* s) {
    memset(s, 0, sizeof(Settings));
    s->density = 1;
    s->paper = FicheroPaperGap;
    s->length_idx = 2; /* 30 mm: the stock 14x30 label */
    s->copies = 1;
    /* Direct BLE is the whole point of building against Blue++. */
    s->link = link_kind_available(LinkKindBle) ? LinkKindBle : LinkKindUart;
}

uint16_t settings_length_px(const Settings* s) {
    return settings_length_mm[s->length_idx % SETTINGS_LENGTH_COUNT] * FICHERO_PX_PER_MM;
}

static void sanitize(Settings* s, Label* l) {
    if(s->density > 2) s->density = 1;
    if(s->paper > FicheroPaperContinuous) s->paper = FicheroPaperGap;
    if(s->length_idx >= SETTINGS_LENGTH_COUNT) s->length_idx = 2;
    if(s->copies < 1 || s->copies > 9) s->copies = 1;
    s->flip = s->flip ? 1 : 0;
    if(s->link >= LinkKindCount || !link_kind_available(s->link)) s->link = LinkKindUart;
    if(l->layout >= LabelLayoutCount) l->layout = LabelLayoutText;
    if(l->scale > LABEL_SCALE_MAX) l->scale = LABEL_SCALE_AUTO;
    if(l->icon >= LABEL_ICON_COUNT) l->icon = 0;
    if(l->qr_ecc >= LabelEccCount) l->qr_ecc = LabelEccLow;
    l->bold = l->bold ? 1 : 0;
    l->border = l->border ? 1 : 0;
    l->line1[LABEL_LINE_MAX] = 0;
    l->line2[LABEL_LINE_MAX] = 0;
    l->qr[LABEL_QR_MAX] = 0;
}

void settings_load(Settings* s, Label* label) {
    settings_default(s);
    label_default(label);

    size_t cap = sizeof(SavedState) > sizeof(SavedStateV3) ? sizeof(SavedState) : sizeof(SavedStateV3);
    uint8_t* buf = malloc(cap);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t n = storage_file_read(file, buf, cap);
        uint32_t magic = n >= sizeof(uint32_t) ? *(uint32_t*)buf : 0;

        if(magic == STATE_MAGIC && n == sizeof(SavedState)) {
            const SavedState* st = (const SavedState*)buf;
            *s = st->settings;
            *label = st->label;
            sanitize(s, label);
        } else if(magic == STATE_MAGIC_V3 && n == sizeof(SavedStateV3)) {
            const SavedStateV3* st = (const SavedStateV3*)buf;
            s->density = st->settings.density;
            s->paper = st->settings.paper;
            s->length_idx = st->settings.length_idx;
            s->copies = st->settings.copies;
            s->flip = st->settings.flip;
            s->link = st->settings.link;
            *label = st->label;
            sanitize(s, label);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buf);
}

void settings_save(const Settings* s, const Label* label) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    SavedState* st = malloc(sizeof(SavedState));
    memset(st, 0, sizeof(SavedState));
    st->magic = STATE_MAGIC;
    st->settings = *s;
    st->label = *label;
    if(storage_file_open(file, STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, st, sizeof(SavedState));
        storage_file_close(file);
    }
    free(st);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
