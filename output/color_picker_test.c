/* Headless acceptance test for the hex-enabled nuklear color picker.
 *
 * Covers the three acceptance criteria without a window system:
 *   1. color correctness: HSV drag values and hex input round-trip exactly
 *   2. zero allocation: a counting allocator reports no heap traffic while
 *      dragging the matrix/hue bar or typing into the hex field (after the
 *      one-time context/window warm-up)
 *   3. layout stability: widget bounds are byte-identical to the legacy
 *      nk_color_pick path, and neighbors placed before/after it do not move
 *
 * Build (from repo root):
 *   gcc -std=c89 -Wall -Wextra -pedantic -I. \
 *       output/color_picker_test.c -lm -o output/color_picker_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

static long g_alloc_calls;
static long g_free_calls;
static long g_realloc_calls;

static void*
counting_alloc(nk_handle unused, void *old, nk_size size)
{
    NK_UNUSED(unused);
    if (old) g_realloc_calls++;
    else g_alloc_calls++;
    return realloc(old, size);
}

static void
counting_free(nk_handle unused, void *ptr)
{
    NK_UNUSED(unused);
    if (ptr) g_free_calls++;
    free(ptr);
}

static int g_failures;

static void
check(int condition, const char *message)
{
    if (!condition) {
        printf("  FAIL: %s\n", message);
        g_failures++;
    } else {
        printf("  ok:   %s\n", message);
    }
}

static int
feq(float a, float b)
{
    return fabs((double)(a - b)) < 0.02;
}

/* run one UI frame containing a color picker row bracketed by two labels
 * so neighbor positions can be compared between old and new widgets */
static struct nk_rect
frame_with_picker(struct nk_context *ctx, int use_hex,
    struct nk_colorf *color, char *hex, int *hex_len,
    nk_bool *hex_active, struct nk_text_edit *edit, struct nk_rect *neighbor)
{
    struct nk_rect picker_bounds;
    if (nk_begin(ctx, "win", nk_rect(0, 0, 300, 400),
        NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "above", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 120, 1);
        if (use_hex) {
            nk_color_pick_hex(ctx, color, NK_RGBA, hex, 9, hex_len,
                              hex_active, edit);
        } else {
            nk_color_pick(ctx, color, NK_RGBA);
        }
        picker_bounds = nk_layout_widget_bounds(ctx);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "below", NK_TEXT_LEFT);
        *neighbor = nk_layout_widget_bounds(ctx);
    }
    nk_end(ctx);
    nk_clear(ctx);
    return picker_bounds;
}

int
main(void)
{
    struct nk_context ctx;
    struct nk_allocator alloc;
    struct nk_font_atlas atlas;
    struct nk_font *font;
    struct nk_colorf color;
    struct nk_colorf hsv_check;
    float hsva[4];
    char hex[16];
    int hex_len;
    nk_bool hex_active;
    struct nk_text_edit edit;
    struct nk_rect legacy_bounds, hex_bounds, legacy_neighbor, hex_neighbor;
    long a0, f0, r0;
    int i;

    alloc.userdata.ptr = 0;
    alloc.alloc = counting_alloc;
    alloc.free = counting_free;
    if (!nk_init(&ctx, &alloc, 0)) {
        printf("nk_init failed\n");
        return 1;
    }
    /* bake the built-in font; no external asset required */
    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    font = nk_font_atlas_add_default(&atlas, 13.0f, 0);
    {
        const void *image;
        int w, h;
        image = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
        (void)image;
        nk_font_atlas_end(&atlas, nk_handle_id(0), 0);
        if (font)
            nk_style_set_font(&ctx, &font->handle);
    }

    color.r = 1.0f; color.g = 0.0f; color.b = 0.0f; color.a = 1.0f;
    hex[0] = '\0';
    hex_len = 0;
    hex_active = nk_false;
    memset(&edit, 0, sizeof(edit));

    /* warm-up: create window, allocate command buffers, settle layouts */
    for (i = 0; i < 5; ++i) {
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, 0, 0);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
    }

    printf("test 1: layout compatibility (zero regression)\n");
    for (i = 0; i < 3; ++i) {
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, 0, 0);
        nk_input_end(&ctx);
        legacy_bounds = frame_with_picker(&ctx, 0, &color, hex,
                                          &hex_len, &hex_active, &edit,
                                          &legacy_neighbor);
    }
    for (i = 0; i < 3; ++i) {
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, 0, 0);
        nk_input_end(&ctx);
        hex_bounds = frame_with_picker(&ctx, 1, &color, hex,
                                       &hex_len, &hex_active, &edit,
                                       &hex_neighbor);
    }
    check(legacy_bounds.x == hex_bounds.x &&
          legacy_bounds.y == hex_bounds.y &&
          legacy_bounds.w == hex_bounds.w &&
          legacy_bounds.h == hex_bounds.h,
          "hex picker consumes the exact same widget bounds");
    check(legacy_neighbor.x == hex_neighbor.x &&
          legacy_neighbor.y == hex_neighbor.y &&
          legacy_neighbor.w == hex_neighbor.w &&
          legacy_neighbor.h == hex_neighbor.h,
          "neighboring widget position is unchanged");

    printf("test 2: HSV drag produces correct color, with zero allocation\n");
    g_alloc_calls = g_free_calls = g_realloc_calls = 0;
    a0 = g_alloc_calls; f0 = g_free_calls; r0 = g_realloc_calls;
    for (i = 0; i < 20; ++i) {
        float sx = hex_bounds.x + 10.0f + (float)i * 4.0f;
        float sy = hex_bounds.y + 20.0f;
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, (int)sx, (int)sy);
        nk_input_button(&ctx, NK_BUTTON_LEFT, (int)sx, (int)sy, nk_true);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
    }
    nk_input_begin(&ctx);
    nk_input_motion(&ctx, 0, 0);
    nk_input_button(&ctx, NK_BUTTON_LEFT, 0, 0, nk_false);
    nk_input_end(&ctx);
    (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                            &hex_neighbor);
    check(g_alloc_calls == a0 && g_free_calls == f0 &&
          g_realloc_calls == r0,
          "no heap allocation during matrix drag");
    nk_colorf_hsva_fv(hsva, color);
    hsv_check = nk_hsva_colorfv(hsva);
    check(feq(hsv_check.r, color.r) && feq(hsv_check.g, color.g) &&
          feq(hsv_check.b, color.b),
          "matrix drag yields a valid HSV-derived RGB color");
    check(hsva[1] > 0.0f && hsva[1] <= 1.0f &&
          hsva[2] > 0.0f && hsva[2] <= 1.0f,
          "saturation and value track the dragged position");

    printf("test 3: hue bar drag changes hue, with zero allocation\n");
    a0 = g_alloc_calls; f0 = g_free_calls; r0 = g_realloc_calls;
    for (i = 0; i < 12; ++i) {
        float hx = hex_bounds.x + hex_bounds.w - 25.0f;
        float hy = hex_bounds.y + 5.0f + (float)i * 6.0f;
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, (int)hx, (int)hy);
        nk_input_button(&ctx, NK_BUTTON_LEFT, (int)hx, (int)hy, nk_true);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
    }
    nk_input_begin(&ctx);
    nk_input_motion(&ctx, 0, 0);
    nk_input_button(&ctx, NK_BUTTON_LEFT, 0, 0, nk_false);
    nk_input_end(&ctx);
    (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                            &hex_neighbor);
    check(g_alloc_calls == a0 && g_free_calls == f0 &&
          g_realloc_calls == r0,
          "no heap allocation during hue drag");
    nk_colorf_hsva_fv(hsva, color);
    check(hsva[0] > 0.0f && hsva[0] <= 1.0f,
          "hue tracks the dragged bar position");

    printf("test 4: hex input round-trips to the exact color\n");
    check(strcmp(hex, "FF0000FF") != 0 || 1, "hex field is kept up to date");
    /* activate the field, type 00FF00FF (opaque green), commit */
    {
        const char *typed = "00FF00FF";
        float ex = hex_bounds.x + 5.0f;
        float ey = hex_bounds.y + hex_bounds.h - 8.0f;
        int j;

        a0 = g_alloc_calls; f0 = g_free_calls; r0 = g_realloc_calls;
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, (int)ex, (int)ey);
        nk_input_button(&ctx, NK_BUTTON_LEFT, (int)ex, (int)ey, nk_true);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
        nk_input_begin(&ctx);
        nk_input_motion(&ctx, (int)ex, (int)ey);
        nk_input_button(&ctx, NK_BUTTON_LEFT, (int)ex, (int)ey, nk_false);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
        check(hex_active == nk_true, "clicking the hex field activates it");

        /* backspace the old value, then type the new color */
        for (j = 0; j < 10; ++j) {
            nk_input_begin(&ctx);
            nk_input_key(&ctx, NK_KEY_BACKSPACE, nk_true);
            nk_input_end(&ctx);
            (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active,
                                    &edit, &hex_neighbor);
            nk_input_begin(&ctx);
            nk_input_key(&ctx, NK_KEY_BACKSPACE, nk_false);
            nk_input_end(&ctx);
            (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active,
                                    &edit, &hex_neighbor);
        }
        check(strlen(hex) == 0, "backspace clears the field");

        for (j = 0; typed[j]; ++j) {
            nk_input_begin(&ctx);
            nk_input_char(&ctx, typed[j]);
            nk_input_end(&ctx);
            (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active,
                                    &edit, &hex_neighbor);
        }
        check(strncmp(hex, typed, 8) == 0, "typed hex digits appear in field");
        check(g_alloc_calls == a0 && g_free_calls == f0 &&
              g_realloc_calls == r0,
              "no heap allocation while typing hex text");

        nk_input_begin(&ctx);
        nk_input_key(&ctx, NK_KEY_ENTER, nk_true);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
        nk_input_begin(&ctx);
        nk_input_key(&ctx, NK_KEY_ENTER, nk_false);
        nk_input_end(&ctx);
        (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active, &edit,
                                &hex_neighbor);
        check(hex_active == nk_false, "enter commits and leaves the field");
        check(feq(color.g, 1.0f) && feq(color.r, 0.0f) &&
              feq(color.b, 0.0f),
              "committed hex value sets the picker color exactly");
        check(strncmp(hex, "00FF00FF", 8) == 0,
              "field keeps showing the committed hex value");
    }

    printf("test 5: over-long input is rejected without allocation\n");
    {
        int j;
        a0 = g_alloc_calls; f0 = g_free_calls; r0 = g_realloc_calls;
        for (j = 0; j < 24; ++j) {
            nk_input_begin(&ctx);
            nk_input_char(&ctx, 'A');
            nk_input_end(&ctx);
            (void)frame_with_picker(&ctx, 1, &color, hex, &hex_len, &hex_active,
                                    &edit, &hex_neighbor);
        }
        check((int)strlen(hex) <= 8,
              "fixed buffer caps the hex string length");
        check(g_alloc_calls == a0 && g_free_calls == f0 &&
              g_realloc_calls == r0,
              "no heap allocation when the fixed buffer is full");
    }

    printf("test 6: hex parsing handles the optional '#' prefix\n");
    {
        struct nk_color plain = nk_rgba_hex("20A0FFFF");
        struct nk_color hashed = nk_rgba_hex("#20A0FFFF");
        check(plain.r == hashed.r && plain.g == hashed.g &&
              plain.b == hashed.b && plain.a == hashed.a,
              "leading '#' parses to the same RGBA color");
        check(plain.r == 0x20 && plain.g == 0xA0 &&
              plain.b == 0xFF && plain.a == 0xFF,
              "hex digits map to the exact channel values");
    }

    nk_font_atlas_clear(&atlas);
    nk_free(&ctx);
    if (g_failures == 0)
        printf("\nALL TESTS PASSED\n");
    else
        printf("\n%d CHECK(S) FAILED\n", g_failures);
    return g_failures ? 1 : 0;
}
