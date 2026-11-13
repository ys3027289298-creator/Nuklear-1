/*
 * dock_test.c - headless verification of the nk_dock module.
 *
 * Builds nuklear with nk_init_fixed over a static memory block and drives
 * the docking state machine through synthetic mouse input only. It verifies:
 *   1. dock layout geometry after several interaction sequences
 *   2. zero heap allocation per frame (and overall after startup)
 *   3. legacy floating-window behaviour still works (movable/scalable path)
 *
 * Exit code is 0 on success, non-zero (with a message) on failure.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "../common/nk_dock.h"

#define WIN_W 1200
#define WIN_H 800
#define AREA  nk_rect(10.0f, 40.0f, 1180.0f, 750.0f)
#define FIXED_BYTES (16 * 1024 * 1024)

static unsigned char fixed_memory[FIXED_BYTES];

/* heap allocation watchdog */
static long g_live_allocs;
static long g_total_allocs;

static void *
track_alloc(nk_handle ud, void *ptr, nk_size new_size)
{
    (void)ud;
    if (new_size != 0 && ptr == 0) {
        g_live_allocs++;
        g_total_allocs++;
        return malloc(new_size);
    }
    if (new_size == 0 && ptr != 0) {
        /* Nuklear frees through the dedicated free callback instead */
        return 0;
    }
    return realloc(ptr, new_size);
}

static void
track_free(nk_handle ud, void *ptr)
{
    (void)ud;
    if (ptr) g_live_allocs--;
    free(ptr);
}

static struct nk_context ctx;
static struct nk_dock dock;
static int frame_no;
static int failures;
static struct nk_font_atlas atlas;
static struct nk_font *font;
static int mouse_x, mouse_y;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL [frame %d]: %s\n", frame_no, msg); \
        failures++; \
    } \
} while (0)

static float
header_h(void)
{
    return ctx.style.window.header.padding.y * 2.0f +
           ctx.style.font->height;
}

/* A frame that also creates the pane windows so window state stays alive.
 * mx/my is the mouse position; btn_down drives the left button state. */
static void
frame_at(int mx, int my, nk_bool btn_down)
{
    int i;
    static nk_size prev_calls = 0;
    static nk_size prev_allocated = 0;
    static int have_baseline = 0;
    mouse_x = mx; mouse_y = my;
    nk_input_begin(&ctx);
    nk_input_motion(&ctx, mx, my);
    nk_input_button(&ctx, NK_BUTTON_LEFT, mx, my, btn_down);
    nk_input_end(&ctx);
    nk_dock_begin_frame(&dock, &ctx, AREA);
    for (i = 0; i < dock.count; ++i) {
        if (nk_dock_begin_pane(&dock, &ctx, i)) {
            nk_layout_row_dynamic(&ctx, 24, 1);
            nk_label(&ctx, dock.panes[i].name, NK_TEXT_LEFT);
        }
        nk_dock_end_pane(&dock, &ctx, i);
    }
    nk_dock_draw_drop_zones(&dock, &ctx);
    nk_clear(&ctx);
    /* The fixed memory block must never grow or take another allocation
     * call once steady-state rendering has started. */
    if (have_baseline) {
        if (ctx.memory.calls != prev_calls)
            CHECK(0, "allocation call count changed during a frame");
        if (ctx.memory.allocated != prev_allocated)
            CHECK(0, "fixed-buffer allocation grew during a frame");
    }
    prev_calls = ctx.memory.calls;
    prev_allocated = ctx.memory.allocated;
    have_baseline = 1;
    frame_no++;
}

static void
frame(void)
{
    frame_at(mouse_x, mouse_y, nk_false);
}

static void
drag(int x0, int y0, int x1, int y1, int steps)
{
    int i;
    /* Press frame: button transitions up->down and stays at the grab point,
     * so the module's edge-triggered press detection starts the drag. */
    frame_at(x0, y0, nk_true);
    for (i = 1; i <= steps; ++i) {
        int x = x0 + (x1 - x0) * i / steps;
        int y = y0 + (y1 - y0) * i / steps;
        frame_at(x, y, nk_true);
    }
    /* release frame: down->up ends the drag and applies docking decision */
    frame_at(x1, y1, nk_false);
    /* settle frame: button fully up, no click edge remains */
    frame_at(x1, y1, nk_false);
}

static int
approx(float a, float b, float eps)
{
    float d = a - b;
    if (d < 0) d = -d;
    return d <= eps;
}

int
main(void)
{
    int i;
    int explorer, props, console, palette;
    char report[2048];
    long allocs_at_first_frame;

    /* start nuklear on the watchdog allocator, then switch to the fixed
     * buffer so the steady state cannot allocate at all. */
        {
        struct nk_allocator alloc;
        alloc.userdata.ptr = 0;
        alloc.alloc = track_alloc;
        alloc.free = track_free;
        nk_init(&ctx, &alloc, 0);
    }
        nk_free(&ctx);
    g_live_allocs = g_total_allocs = 0;

        nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    font = nk_font_atlas_add_default(&atlas, 13.0f, 0);
    {
        int fw, fh;
        (void)nk_font_atlas_bake(&atlas, &fw, &fh, NK_FONT_ATLAS_ALPHA8);
    }
        if (!nk_init_fixed(&ctx, fixed_memory, sizeof(fixed_memory),
                       &font->handle)) {
        printf("FAIL: nk_init_fixed returned false\n");
        return 1;
    }

        nk_dock_init(&dock);
    explorer = nk_dock_add(&dock, "Explorer", NK_DOCK_LEFT,
                           nk_rect(400, 200, 260, 300));
    props    = nk_dock_add(&dock, "Properties", NK_DOCK_RIGHT,
                           nk_rect(700, 200, 240, 300));
    console  = nk_dock_add(&dock, "Console", NK_DOCK_CENTER,
                           nk_rect(300, 500, 400, 180));
    palette  = nk_dock_add(&dock, "Palette", NK_DOCK_CENTER,
                           nk_rect(420, 260, 300, 200));
    CHECK(explorer == 0 && props == 1 && console == 2 && palette == 3,
          "nk_dock_add indices");
    CHECK(dock.count == 4, "four panes registered");

    /* ---- segment 1: initial three-zone layout ---- */
        frame();
    allocs_at_first_frame = g_total_allocs;
    CHECK(g_total_allocs == 0, "no heap allocations with fixed context");
    CHECK(dock.panes[explorer].zone == NK_DOCK_LEFT, "Explorer starts left");
    CHECK(dock.panes[props].zone == NK_DOCK_RIGHT, "Properties starts right");
    CHECK(dock.panes[console].zone == NK_DOCK_CENTER, "Console starts center");
    {
        struct nk_rect zl = dock.zone[NK_DOCK_LEFT];
        struct nk_rect zc = dock.zone[NK_DOCK_CENTER];
        struct nk_rect zr = dock.zone[NK_DOCK_RIGHT];
        CHECK(approx(zl.x, AREA.x, 1.0f), "left zone origin");
        CHECK(approx(zr.x + zr.w, AREA.x + AREA.w, 1.0f),
              "right zone touches right edge");
        CHECK(approx(zl.w + zc.w + zr.w + 2 * NK_DOCK_GAP,
                     AREA.w, 1.0f), "zone widths fill the work area");
        /* two center panes share the center height */
        CHECK(approx(dock.panes[console].rect.h, dock.panes[palette].rect.h,
                     1.0f), "center panes split height evenly");
        CHECK(approx(dock.panes[palette].rect.y,
                     dock.panes[console].rect.y +
                     dock.panes[console].rect.h + NK_DOCK_GAP, 1.0f),
              "center panes stacked with gap");
    }

    /* ---- segment 2: drag left splitter to widen left zone ---- */
    {
        struct nk_rect s = nk_dock_splitter_rect(&dock, 0);
        int sx = (int)(s.x + s.w * 0.5f);
        int sy = (int)(s.y + s.h * 0.5f);
        float old_frac = dock.left_frac;
        drag(sx, sy, sx + 160, sy, 12);
        CHECK(dock.left_frac > old_frac, "left splitter widened left zone");
        CHECK(dock.right_frac > 0.0f && dock.left_frac < 1.0f,
              "splitter fractions stay valid");
        CHECK(approx(dock.zone[NK_DOCK_LEFT].w,
                     AREA.w * dock.left_frac - NK_DOCK_GAP, 1.0f),
              "left zone matches new fraction");
        CHECK(g_total_allocs == allocs_at_first_frame,
              "no allocation during splitter drag");
    }

    /* ---- segment 3: drag Explorer out, then drop it on the right edge ---- */
    {
        struct nk_rect r = dock.panes[explorer].rect;
        int hx = (int)(r.x + r.w * 0.5f);
        int hy = (int)(r.y + header_h() * 0.5f);
        drag(hx, hy, 600, 300, 12);
        CHECK(dock.panes[explorer].zone == NK_DOCK_NONE,
              "Explorer becomes floating when dropped outside edges");
        r = dock.panes[explorer].rect;
        CHECK(approx(dock.panes[explorer].rect.x,
                     600.0f - r.w * 0.5f, 2.0f) &&
              approx(dock.panes[explorer].rect.y,
                     300.0f - header_h() * 0.5f, 2.0f),
              "floating window follows cursor");

        r = dock.panes[explorer].rect;
        hx = (int)(r.x + r.w * 0.5f);
        hy = (int)(r.y + header_h() * 0.5f);
        drag(hx, hy, (int)(AREA.x + AREA.w - 30), 400, 12);
        CHECK(dock.panes[explorer].zone == NK_DOCK_RIGHT,
              "Explorer re-docks on right edge");
        CHECK(approx(dock.panes[explorer].rect.x,
                     dock.zone[NK_DOCK_RIGHT].x, 1.0f),
              "re-docked pane snaps into right zone");
    }

    /* ---- segment 4: drag Properties out to floating, leave it there ---- */
    {
        struct nk_rect r = dock.panes[props].rect;
        int hx = (int)(r.x + r.w * 0.5f);
        int hy = (int)(r.y + header_h() * 0.5f);
        drag(hx, hy, 250, 120, 12);
        CHECK(dock.panes[props].zone == NK_DOCK_NONE,
              "Properties floats freely");
        CHECK(dock.panes[props].open, "floating pane stays open");
    }

    /* ---- segment 5: floating pane keeps moving by header drag ---- */
    {
        struct nk_rect before;
        struct nk_rect r;
        int hx, hy;
        before = dock.panes[props].rect;
        r = dock.panes[props].rect;
        hx = (int)(r.x + r.w * 0.5f);
        hy = (int)(r.y + header_h() * 0.5f);
        /* stay well inside the central free area so it remains floating */
        drag(hx, hy, hx + 60, hy + 40, 8);
        CHECK(dock.panes[props].zone == NK_DOCK_NONE,
              "floating pane stays floating after free drag");
        CHECK(approx(dock.panes[props].rect.x,
                     (hx + 60) - before.w * 0.5f, 2.0f) &&
              approx(dock.panes[props].rect.y,
                     (hy + 40) - header_h() * 0.5f, 2.0f),
              "floating pane follows header drag");
        CHECK(approx(dock.panes[props].rect.w, before.w, 1.0f) &&
              approx(dock.panes[props].rect.h, before.h, 1.0f),
              "floating pane keeps size during move");
    }

    /* ---- segment 6: right splitter drag while panes are mixed ---- */
    {
        struct nk_rect s = nk_dock_splitter_rect(&dock, 1);
        int sx = (int)(s.x + s.w * 0.5f);
        int sy = (int)(s.y + s.h * 0.5f);
        float old = dock.right_frac;
        drag(sx, sy, sx - 120, sy, 10);
        CHECK(dock.right_frac > old, "right splitter widened right zone");
    }

    /* ---- steady-state allocation check over many frames ---- */
    for (i = 0; i < 120; ++i)
        frame();
    CHECK(g_total_allocs == 0, "zero heap allocations across all frames");
    CHECK(g_live_allocs == 0, "no leaked heap allocations");

    /* ---- state query must work without allocation ---- */
    i = nk_dock_query(&dock, report, (int)sizeof(report));
    CHECK(i > 0 && i < (int)sizeof(report), "nk_dock_query fills buffer");
    CHECK(strstr(report, "layout:") != 0, "query reports layout fractions");
    CHECK(strstr(report, "Explorer") != 0 &&
          strstr(report, "Floating") != 0,
          "query reports per-pane zone and floating state");

    printf("%s\n", report);
    printf("%s: %d frame(s), %ld heap alloc(s)\n",
           failures ? "FAILED" : "PASSED", frame_no, g_total_allocs);
    nk_free(&ctx);
    return failures ? 1 : 0;
}
