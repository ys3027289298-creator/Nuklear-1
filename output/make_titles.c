/* Renders the four segment title cards as PPM images using the same
 * software (rawfb) backend as the demo, so the recording needs no
 * external artwork and no drawtext-capable ffmpeg build.
 *
 * Build:
 *   cc -std=c89 -Wall -Wextra -pedantic -Wno-unused-function \
 *      -I. output/make_titles.c -lm -o output/make_titles
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

#define W 480
#define H 360

static void
write_ppm(const char *path, const unsigned int *fb)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (i = 0; i < W * H; ++i) {
        unsigned char px[3];
        unsigned int c = fb[i];
        px[0] = (unsigned char)(c & 0xFF);
        px[1] = (unsigned char)((c >> 8) & 0xFF);
        px[2] = (unsigned char)((c >> 16) & 0xFF);
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static void
render_card(struct rawfb_context *rawfb, const char *title,
    const char *subtitle, const char *path)
{
    if (nk_begin(&rawfb->ctx, "card", nk_rect(0, 0, W, H), 0)) {
        nk_layout_row_dynamic(&rawfb->ctx, 60, 1);
        nk_label(&rawfb->ctx, "", NK_TEXT_CENTERED);
        nk_layout_row_dynamic(&rawfb->ctx, 40, 1);
        nk_label(&rawfb->ctx, title, NK_TEXT_CENTERED);
        nk_layout_row_dynamic(&rawfb->ctx, 30, 1);
        nk_label(&rawfb->ctx, subtitle, NK_TEXT_CENTERED);
    }
    nk_end(&rawfb->ctx);
    nk_rawfb_render(rawfb, nk_rgb(28, 30, 36), 1);
    write_ppm(path, (const unsigned int*)rawfb->fb.pixels);
    nk_clear(&rawfb->ctx);
}

int
main(void)
{
    struct rawfb_context *rawfb;
    struct rawfb_pl pl;
    unsigned int *fb;
    unsigned char *scratch;

    fb = (unsigned int*)calloc((size_t)W * H, sizeof(unsigned int));
    scratch = (unsigned char*)calloc(512 * 512, 1);
    if (!fb || !scratch) return 1;

    pl.bytesPerPixel = 4;
    pl.rshift = 0;  pl.gshift = 8;  pl.bshift = 16; pl.ashift = 24;
    pl.rloss = 0;   pl.gloss = 0;   pl.bloss = 0;   pl.aloss = 0;

    rawfb = nk_rawfb_init(fb, scratch, W, H, W * 4, pl);
    if (!rawfb) return 1;

    render_card(rawfb, "1. HSV picker", "hue / saturation / value",
        "output/frames/title_1.ppm");
    render_card(rawfb, "2. Matrix drag", "saturation and value",
        "output/frames/title_2.ppm");
    render_card(rawfb, "3. Hue bar drag", "continuous hue selection",
        "output/frames/title_3.ppm");
    render_card(rawfb, "4. Hex input", "type and commit 20A0FFFF",
        "output/frames/title_4.ppm");

    nk_rawfb_shutdown(rawfb);
    free(scratch);
    free(fb);
    return 0;
}
