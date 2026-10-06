// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../core/bitmap.h"
#include "../../core/font5x7.h"
#include "../../core/label.h"
#include "../../core/fichero_proto.h"
#include "../../core/uart_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond) \
    do { \
        if(!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            fails++; \
        } \
    } while(0)

static void check_bytes(const uint8_t* got, size_t n, const uint8_t* want, size_t m, int line) {
    if(n != m || memcmp(got, want, n)) {
        printf("FAIL line %d: byte mismatch (got %zu bytes, want %zu)\n", line, n, m);
        fails++;
    }
}
#define CHECK_BYTES(got, n, ...) \
    do { \
        const uint8_t want[] = {__VA_ARGS__}; \
        check_bytes(got, n, want, sizeof(want), __LINE__); \
    } while(0)

static void dump(const Bitmap* b, const char* title) {
    printf("\n== %s (%dx%d) ==\n", title, b->w, b->h);
    for(int y = 0; y < b->h; y++) {
        for(int x = 0; x < b->w; x++) putchar(bitmap_get(b, x, y) ? '#' : '.');
        putchar('\n');
    }
}

static void test_protocol(void) {
    uint8_t buf[FICHERO_CMD_MAX];
    size_t n;
    n = fichero_cmd_density(buf, 1);
    CHECK_BYTES(buf, n, 0x10, 0xFF, 0x10, 0x00, 0x01);
    n = fichero_cmd_density(buf, 9); /* clamped */
    CHECK_BYTES(buf, n, 0x10, 0xFF, 0x10, 0x00, 0x02);
    n = fichero_cmd_paper(buf, FicheroPaperGap);
    CHECK_BYTES(buf, n, 0x10, 0xFF, 0x84, 0x00);
    n = fichero_cmd_enable(buf);
    CHECK_BYTES(buf, n, 0x10, 0xFF, 0xFE, 0x01);
    n = fichero_cmd_stop(buf);
    CHECK_BYTES(buf, n, 0x10, 0xFF, 0xFE, 0x45);
    n = fichero_cmd_feed(buf);
    CHECK_BYTES(buf, n, 0x1D, 0x0C);
    n = fichero_cmd_wake(buf);
    CHECK(n == 12 && buf[0] == 0 && buf[11] == 0);
    n = fichero_cmd_raster_header(buf, 240);
    CHECK_BYTES(buf, n, 0x1D, 0x76, 0x30, 0x00, 0x0C, 0x00, 240, 0x00);
    n = fichero_cmd_raster_header(buf, 300);
    CHECK_BYTES(buf, n, 0x1D, 0x76, 0x30, 0x00, 0x0C, 0x00, 0x2C, 0x01);

    CHECK(fichero_resp_is_ok((const uint8_t*)"OK", 2));
    CHECK(!fichero_resp_is_ok((const uint8_t*)"O", 1));
    CHECK(fichero_resp_is_done((const uint8_t[]){0xAA}, 1));
}

static void test_raster(void) {
    Bitmap* b = bitmap_alloc();
    bitmap_reshape(b, 240);
    uint8_t* raster = malloc(240 * 12);

    /* Clockwise rotation (the web app's orientation): logical top-left (0,0) -> row 0, col 95 */
    bitmap_set(b, 0, 0, true);
    size_t n = fichero_bitmap_to_raster(b, false, raster);
    CHECK(n == 240 * 12);
    CHECK(raster[0 * 12 + 11] == 0x01);
    /* logical (239, 95) -> last row, col 0 */
    bitmap_clear(b);
    bitmap_set(b, 239, 95, true);
    fichero_bitmap_to_raster(b, false, raster);
    CHECK(raster[239 * 12 + 0] == 0x80);
    /* flipped (180 degrees from that): (0,0) -> last row, col 0 */
    bitmap_clear(b);
    bitmap_set(b, 0, 0, true);
    fichero_bitmap_to_raster(b, true, raster);
    CHECK(raster[239 * 12 + 0] == 0x80);

    /* exactly one bit set anywhere */
    int ones = 0;
    for(size_t i = 0; i < n; i++)
        for(int k = 0; k < 8; k++) ones += (raster[i] >> k) & 1;
    CHECK(ones == 1);

    free(raster);
    bitmap_free(b);
}

static void test_frames(void) {
    uint8_t buf[UART_FRAME_MAX_LEN];
    uint8_t payload[200];
    for(int i = 0; i < 200; i++) payload[i] = (uint8_t)(i * 7 + 3);

    UartFrameParser p;
    uart_frame_parser_reset(&p);

    /* roundtrip, max size, with leading garbage */
    size_t n = uart_frame_encode(buf, FRAME_WRITE, payload, 200);
    CHECK(n == 205);
    int got = 0;
    uint8_t junk[] = {0x00, 0xA5, 0xFF, 0x13};
    for(size_t i = 0; i < sizeof(junk); i++) got += uart_frame_parser_feed(&p, junk[i]);
    uart_frame_parser_reset(&p);
    for(size_t i = 0; i < n; i++) {
        if(uart_frame_parser_feed(&p, buf[i])) {
            got++;
            CHECK(p.frame.type == FRAME_WRITE && p.frame.len == 200);
            CHECK(!memcmp(p.frame.payload, payload, 200));
        }
    }
    CHECK(got == 1);

    /* empty payload */
    n = uart_frame_encode(buf, FRAME_HELLO, NULL, 0);
    CHECK(n == 5);
    got = 0;
    for(size_t i = 0; i < n; i++) got += uart_frame_parser_feed(&p, buf[i]);
    CHECK(got == 1 && p.frame.type == FRAME_HELLO && p.frame.len == 0);

    /* corrupted CRC is rejected, next good frame still parses */
    n = uart_frame_encode(buf, FRAME_NOTIFY, (const uint8_t*)"OK", 2);
    buf[5] ^= 0x40;
    got = 0;
    for(size_t i = 0; i < n; i++) got += uart_frame_parser_feed(&p, buf[i]);
    CHECK(got == 0);
    n = uart_frame_encode(buf, FRAME_NOTIFY, (const uint8_t*)"OK", 2);
    for(size_t i = 0; i < n; i++) got += uart_frame_parser_feed(&p, buf[i]);
    CHECK(got == 1 && p.frame.len == 2 && !memcmp(p.frame.payload, "OK", 2));

    /* oversize length is rejected */
    uint8_t bad[] = {UART_FRAME_SOF, FRAME_WRITE, 0xFF, 0x7F};
    got = 0;
    for(size_t i = 0; i < sizeof(bad); i++) got += uart_frame_parser_feed(&p, bad[i]);
    CHECK(got == 0 && p.stage == ParseSof);

    /* known-answer CRC-8/SMBUS("123456789") = 0xF4 */
    CHECK(uart_frame_crc8((const uint8_t*)"123456789", 9) == 0xF4);
}

/* Rightmost set pixel column, or -1. */
static int rightmost(const Bitmap* b) {
    for(int x = b->w - 1; x >= 0; x--)
        for(int y = 0; y < b->h; y++)
            if(bitmap_get(b, x, y)) return x;
    return -1;
}

/* Tallest run of rows containing black pixels at x >= from (text height at the right of a QR). */
static int ink_height(const Bitmap* b, int from) {
    int y0 = b->h, y1 = -1;
    for(int y = 0; y < b->h; y++)
        for(int x = from; x < b->w; x++)
            if(bitmap_get(b, x, y)) {
                if(y < y0) y0 = y;
                if(y > y1) y1 = y;
            }
    return y1 < 0 ? 0 : y1 - y0 + 1;
}

/* The printer starts ~2 mm into the label, so nothing may be drawn in the last LABEL_LEAD_PX dots. */
static void test_dead_zone(void) {
    Bitmap* b = bitmap_alloc();
    static const uint16_t widths[] = {160, 200, 240, 320, 400};
    for(size_t wi = 0; wi < sizeof(widths) / sizeof(widths[0]); wi++) {
        for(int layout = 0; layout < LabelLayoutCount; layout++) {
            for(int border = 0; border < 2; border++) {
                bitmap_reshape(b, widths[wi]);
                Label l;
                label_default(&l);
                l.layout = layout;
                l.border = border;
                strcpy(l.line1, "flugilo_art");
                strcpy(l.line2, "a long second line");
                strcpy(l.qr, "https://instagram.com/flugilo_art");
                label_render(&l, b);
                int r = rightmost(b);
                if(r >= widths[wi] - LABEL_LEAD_PX) {
                    printf("FAIL: width %u layout %d border %d draws at x=%d (limit %d)\n",
                           widths[wi], layout, border, r, widths[wi] - LABEL_LEAD_PX - 1);
                    fails++;
                }
            }
        }
    }

    /* the label from the bug report: QR + "flugilo_art" at the default 30 mm must keep 2x text */
    bitmap_reshape(b, 240);
    Label l;
    label_default(&l);
    l.layout = LabelLayoutQrText;
    strcpy(l.qr, "https://instagram.com/flugilo_art");
    strcpy(l.line1, "flugilo_art");
    l.line2[0] = 0;
    CHECK(label_render(&l, b));
    int text_h = ink_height(b, 96);
    printf("QR+text label: text ink height %d px (2x = 16, 1x = 8), rightmost x=%d (limit %d)\n",
           text_h, rightmost(b), 240 - LABEL_LEAD_PX - 1);
    CHECK(text_h >= 14);

    /* shifted all the way to the start of the printable area: the QR touches column 0 */
    l.border = 0;
    CHECK(label_render(&l, b));
    int left = b->w;
    for(int x = 0; x < b->w && left == b->w; x++)
        for(int y = 0; y < b->h; y++)
            if(bitmap_get(b, x, y)) {
                left = x;
                break;
            }
    printf("QR+text label: leftmost ink at x=%d\n", left);
    CHECK(left == 0);
    bitmap_free(b);
}

static void test_labels(bool verbose) {
    Bitmap* b = bitmap_alloc();
    Label l;
    label_default(&l);

    bitmap_reshape(b, 240);
    CHECK(label_render(&l, b));
    if(verbose) dump(b, "text auto");

    l.border = 1;
    l.bold = 1;
    strcpy(l.line1, "Gym bag");
    l.line2[0] = 0;
    CHECK(label_render(&l, b));
    if(verbose) dump(b, "bold border one line");

    label_default(&l);
    l.layout = LabelLayoutQr;
    strcpy(l.qr, "https://flipper.net");
    CHECK(label_render(&l, b));
    if(verbose) dump(b, "qr");

    /* A typical URL (33 chars -> version 3, 29 modules) must be drawn at 3 px per module,
     * i.e. nearly the full label height. */
    {
        Label q;
        label_default(&q);
        q.layout = LabelLayoutQr;
        strcpy(q.qr, "https://instagram.com/flugilo_art");
        CHECK(label_render(&q, b));
        int y0 = BITMAP_H, y1 = -1;
        for(int y = 0; y < b->h; y++)
            for(int x = 0; x < b->w; x++)
                if(bitmap_get(b, x, y)) {
                    if(y < y0) y0 = y;
                    if(y > y1) y1 = y;
                }
        printf("33-char URL QR height: %d px of %d\n", y1 - y0 + 1, BITMAP_H);
        CHECK(y1 - y0 + 1 >= 84);
        if(verbose) dump(b, "qr 33-char url");
    }

    l.layout = LabelLayoutQrText;
    strcpy(l.line1, "Scan me");
    strcpy(l.line2, "flipper.net");
    CHECK(label_render(&l, b));
    if(verbose) dump(b, "qr+text");

    /* too long: must fail gracefully */
    memset(l.qr, 'x', LABEL_QR_MAX);
    l.qr[LABEL_QR_MAX] = 0;
    l.qr_ecc = LabelEccQuartile;
    bool ok = label_render(&l, b);
    printf("64-char QR @ ECC Q: %s\n", ok ? "fits" : "rejected");

    for(int i = 0; i < LABEL_ICON_COUNT; i++) {
        label_default(&l);
        l.layout = LabelLayoutIconText;
        l.icon = i;
        strcpy(l.line1, label_icon_names[i]);
        l.line2[0] = 0;
        CHECK(label_render(&l, b));
        if(verbose) dump(b, label_icon_names[i]);
    }

    /* font sheet */
    label_default(&l);
    bitmap_reshape(b, 400);
    bitmap_clear(b);
    font_draw_text(b, 2, 0, " !\"#$%&'()*+,-./0123456789:;<=>?", 1, false);
    font_draw_text(b, 2, 10, "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", 1, false);
    font_draw_text(b, 2, 20, "`abcdefghijklmnopqrstuvwxyz{|}~", 1, false);
    if(verbose) {
        printf("\n== font sheet ==\n");
        for(int y = 0; y < 30; y++) {
            for(int x = 0; x < 200; x++) putchar(bitmap_get(b, x, y) ? '#' : '.');
            putchar('\n');
        }
    }

    /* minimum + maximum widths don't crash */
    bitmap_reshape(b, 8);
    label_default(&l);
    label_render(&l, b);
    bitmap_reshape(b, 5000); /* clamped */
    CHECK(b->w == BITMAP_MAX_W);
    label_render(&l, b);

    bitmap_free(b);
}

int main(int argc, char** argv) {
    bool verbose = argc > 1 && !strcmp(argv[1], "-v");
    test_protocol();
    test_raster();
    test_frames();
    test_dead_zone();
    test_labels(verbose);
    if(fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
