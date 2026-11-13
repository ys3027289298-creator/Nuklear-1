/* Offscreen, software-rendered demo of the hex-enabled color picker.
 *
 * It drives the real nuklear widget through the rawfb backend with a
 * plain malloc'd framebuffer (no window system, no GL), simulates mouse
 * and keyboard input, and writes one PPM image per frame into a folder.
 * ffmpeg turns the PPM sequence into the segmented screen recording.
 *
 * Build (from repo root):
 *   cc -std=c89 -Wall -Wextra -pedantic -Wno-unused-function \
 *      -I. output/color_picker_offscreen.c -lm -o output/color_picker_offscreen
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"
#define NK_RAWFB_IMPLEMENTATION
#include "../demo/rawfb/nuklear_rawfb.h"

#define WIN_W 480
#define WIN_H 360

static void
write_ppm(const char *path, const unsigned int *fb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; ++i) {
        unsigned char px[3];
        unsigned int c = fb[i];
        px[0] = (unsigned char)((c >> 0) & 0xFF);  /* R, rshift = 0 */
        px[1] = (unsigned char)((c >> 8) & 0xFF);  /* G, gshift = 8 */
        px[2] = (unsigned char)((c >> 16) & 0xFF); /* B, bshift = 16 */
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static void
mouse_move(struct nk_context *ctx, int x, int y)
{
    nk_input_motion(ctx, x, y);
}

static void
mouse_button(struct nk_context *ctx, int x, int y, nk_bool down)
{
    nk_input_motion(ctx, x, y);
    nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
}

static void
type_text(struct nk_context *ctx, const char *text)
{
    for (; *text; ++text)
        nk_input_char(ctx, *text);
}

int
main(void)
{
    struct rawfb_context *rawfb;
    struct rawfb_pl pl;
    unsigned char *scratch;
    unsigned int *fb;
    struct nk_colorf color;
    char hex[16];
    int hex_len = 0;
    nk_bool hex_active = nk_false;
    struct nk_text_edit hex_edit;
    int frame = 0;
    int i;
    const char *outdir = "output/frames";

    fb = (unsigned int*)calloc((size_t)WIN_W * WIN_H, sizeof(unsigned int));
    scratch = (unsigned char*)calloc(512 * 512, 1);
    if (!fb || !scratch) return 1;

    /* 32-bit BGRA-ish integer layout used by the rawfb setpixel path */
    pl.bytesPerPixel = 4;
    pl.rshift = 0;  pl.gshift = 8;  pl.bshift = 16; pl.ashift = 24;
    pl.rloss = 0;   pl.gloss = 0;   pl.bloss = 0;   pl.aloss = 0;

    rawfb = nk_rawfb_init(fb, scratch, WIN_W, WIN_H, WIN_W * 4, pl);
    if (!rawfb) return 1;

    color.r = 0.85f; color.g = 0.2f; color.b = 0.25f; color.a = 1.0f;
    hex[0] = '\0';
    memset(&hex_edit, 0, sizeof(hex_edit));

    /* ---- segment 1: idle, shows HSV matrix + hue bar + hex field ---- */
    for (i = 0; i < 30; ++i) {
        char path[128];
        nk_input_begin(&rawfb->ctx);
        mouse_move(&rawfb->ctx, 10, 10);
        nk_input_end(&rawfb->ctx);
        if (nk_begin(&rawfb->ctx, "Color picker",
            nk_rect(20, 20, 440, 320),
            NK_WINDOW_BORDER|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(&rawfb->ctx, 24, 1);
            nk_label(&rawfb->ctx, "HSV + hex color picker", NK_TEXT_LEFT);
            nk_layout_row_dynamic(&rawfb->ctx, 220, 1);
            nk_color_pick_hex(&rawfb->ctx, &color, NK_RGBA, hex,
                (int)sizeof(hex), &hex_len, &hex_active, &hex_edit);
        }
        nk_end(&rawfb->ctx);
        nk_rawfb_render(rawfb, nk_rgb(45, 45, 48), 1);
        nk_clear(&rawfb->ctx);
        sprintf(path, "%s/frame_%04d.ppm", outdir, frame++);
        write_ppm(path, fb, WIN_W, WIN_H);
    }

    /* ---- segment 2: drag inside the saturation/value matrix ---- */
    for (i = 0; i < 45; ++i) {
        char path[128];
        int x = 45 + i * 7;
        int y = 95 + (i % 7) * 18;
        nk_input_begin(&rawfb->ctx);
        nk_input_motion(&rawfb->ctx, x, y);
        if (i == 0)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, nk_true);
        else if (i == 44)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, nk_false);
        nk_input_end(&rawfb->ctx);
        if (nk_begin(&rawfb->ctx, "Color picker",
            nk_rect(20, 20, 440, 320),
            NK_WINDOW_BORDER|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(&rawfb->ctx, 24, 1);
            nk_label(&rawfb->ctx, "dragging saturation/value", NK_TEXT_LEFT);
            nk_layout_row_dynamic(&rawfb->ctx, 220, 1);
            nk_color_pick_hex(&rawfb->ctx, &color, NK_RGBA, hex,
                (int)sizeof(hex), &hex_len, &hex_active, &hex_edit);
        }
        nk_end(&rawfb->ctx);
        nk_rawfb_render(rawfb, nk_rgb(45, 45, 48), 1);
        nk_clear(&rawfb->ctx);
        sprintf(path, "%s/frame_%04d.ppm", outdir, frame++);
        write_ppm(path, fb, WIN_W, WIN_H);
    }

    /* ---- segment 3: drag the hue bar ---- */
    for (i = 0; i < 45; ++i) {
        char path[128];
        int x = 412;
        int y = 90 + i * 4;
        nk_input_begin(&rawfb->ctx);
        nk_input_motion(&rawfb->ctx, x, y);
        if (i == 0)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, nk_true);
        else if (i == 44)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, nk_false);
        nk_input_end(&rawfb->ctx);
        if (nk_begin(&rawfb->ctx, "Color picker",
            nk_rect(20, 20, 440, 320),
            NK_WINDOW_BORDER|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(&rawfb->ctx, 24, 1);
            nk_label(&rawfb->ctx, "dragging hue", NK_TEXT_LEFT);
            nk_layout_row_dynamic(&rawfb->ctx, 220, 1);
            nk_color_pick_hex(&rawfb->ctx, &color, NK_RGBA, hex,
                (int)sizeof(hex), &hex_len, &hex_active, &hex_edit);
        }
        nk_end(&rawfb->ctx);
        nk_rawfb_render(rawfb, nk_rgb(45, 45, 48), 1);
        nk_clear(&rawfb->ctx);
        sprintf(path, "%s/frame_%04d.ppm", outdir, frame++);
        write_ppm(path, fb, WIN_W, WIN_H);
    }

    /* ---- segment 4: click the hex field and type a value ---- */
    for (i = 0; i < 60; ++i) {
        char path[128];
        nk_input_begin(&rawfb->ctx);
        /* keep the pointer parked on the hex field on every frame */
        nk_input_motion(&rawfb->ctx, 60, 298);
        if (i == 0)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, 60, 298, nk_true);
        else if (i == 1)
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, 60, 298, nk_false);
        else if (i >= 2 && i < 18 && (i % 2 == 0)) {
            /* 8 backspace presses (down/up pairs) clear the old value */
            nk_input_key(&rawfb->ctx, NK_KEY_BACKSPACE,
                nk_true);
        }
        else if (i >= 2 && i < 18) {
            nk_input_key(&rawfb->ctx, NK_KEY_BACKSPACE, nk_false);
        }
        else if (i >= 20 && i < 28) {
            static const char *typed = "20A0FFFF";
            int k = i - 20;
            if (k < 8) nk_input_char(&rawfb->ctx, typed[k]);
        }
        else if (i == 30)
            nk_input_key(&rawfb->ctx, NK_KEY_ENTER, nk_true);
        else if (i == 31)
            nk_input_key(&rawfb->ctx, NK_KEY_ENTER, nk_false);
        nk_input_end(&rawfb->ctx);
        if (nk_begin(&rawfb->ctx, "Color picker",
            nk_rect(20, 20, 440, 320),
            NK_WINDOW_BORDER|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(&rawfb->ctx, 24, 1);
            nk_label(&rawfb->ctx, "typing hex value 20A0FFFF", NK_TEXT_LEFT);
            nk_layout_row_dynamic(&rawfb->ctx, 220, 1);
            nk_color_pick_hex(&rawfb->ctx, &color, NK_RGBA, hex,
                (int)sizeof(hex), &hex_len, &hex_active, &hex_edit);
        }
        nk_end(&rawfb->ctx);
        nk_rawfb_render(rawfb, nk_rgb(45, 45, 48), 1);
        nk_clear(&rawfb->ctx);
        sprintf(path, "%s/frame_%04d.ppm", outdir, frame++);
        write_ppm(path, fb, WIN_W, WIN_H);
    }

    printf("wrote %d frames to %s; final hex='%s' rgb=(%.2f,%.2f,%.2f)\n",
        frame, outdir, hex, color.r, color.g, color.b);

    nk_rawfb_shutdown(rawfb);
    free(scratch);
    free(fb);
    return 0;
}
