/*
 * dock_demo.c - three-zone docking demo for nuklear (GLFW + OpenGL3).
 *
 * Left/Center/Right dock zones, draggable splitters, panes that can be
 * dragged out to floating windows and dropped back onto zone edges, and a
 * live layout-state query panel.
 *
 * The whole nuklear context runs on a fixed memory block, so there are no
 * per-frame heap allocations.
 *
 * Modes:
 *   dock_demo            interactive window
 *   dock_demo --script   unattended, plays a timed interaction script and
 *                        exits (used for screen recording; no keyboard)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#include <nuklear.h>
#include "nuklear_glfw_gl3.h"
#include "../common/nk_dock.h"

#define WIN_W 1200
#define WIN_H 800
#define MAX_VERTEX_BUFFER (512 * 1024)
#define MAX_ELEMENT_BUFFER (128 * 1024)
#define FIXED_BYTES (16 * 1024 * 1024)

static unsigned char fixed_memory[FIXED_BYTES];

/* ---- offscreen frame recorder (FBO + glReadPixels, no screen capture) ---- */
struct recorder {
    GLuint fbo;
    GLuint color_tex;
    int enabled;
    int width, height;
    char prefix[256];
    int frame;
};

static void
recorder_init(struct recorder *rec, int w, int h, const char *prefix)
{
    rec->enabled = 1;
    rec->width = w;
    rec->height = h;
    rec->frame = 0;
    strncpy(rec->prefix, prefix, sizeof(rec->prefix) - 1);
    rec->prefix[sizeof(rec->prefix) - 1] = 0;

    glGenFramebuffers(1, &rec->fbo);
    glGenTextures(1, &rec->color_tex);
    glBindTexture(GL_TEXTURE_2D, rec->color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, rec->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, rec->color_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void
recorder_capture(struct recorder *rec)
{
    char path[320];
    FILE *fp;
    int row, col;
    unsigned char *pixels;
    if (!rec->enabled) return;

    pixels = (unsigned char *)malloc((size_t)rec->width *
                                     rec->height * 4);
    if (!pixels) return;
    glBindFramebuffer(GL_FRAMEBUFFER, rec->fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, rec->width, rec->height, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    snprintf(path, sizeof(path), "%s/%05d.ppm", rec->prefix, rec->frame++);
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "P6\n%d %d\n255\n", rec->width, rec->height);
        /* OpenGL origin is bottom-left; flip rows while writing PPM. */
        for (row = rec->height - 1; row >= 0; --row)
            for (col = 0; col < rec->width; ++col) {
                const unsigned char *px =
                    &pixels[(row * rec->width + col) * 4];
                fwrite(px, 1, 3, fp);
            }
        fclose(fp);
    }
    free(pixels);
}

enum pane_id { PANE_EXPLORER, PANE_PROPERTIES, PANE_CONSOLE, PANE_PALETTE,
               PANE_COUNT };

/* tiny self-made swatch texture, generated at runtime, no external assets */
static GLuint swatch_tex;
static unsigned char swatch_pixels[8 * 8 * 4];

static void
make_swatch(void)
{
    int x, y;
    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) {
            unsigned char *px = &swatch_pixels[(y * 8 + x) * 4];
            px[0] = (unsigned char)(40 + x * 24);
            px[1] = (unsigned char)(90 + y * 18);
            px[2] = (unsigned char)(200 - x * 10);
            px[3] = 255;
        }
    glGenTextures(1, &swatch_tex);
    glBindTexture(GL_TEXTURE_2D, swatch_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, swatch_pixels);
}

static void
pane_body(struct nk_context *ctx, int id)
{
    nk_layout_row_dynamic(ctx, 22, 1);
    switch (id) {
    case PANE_EXPLORER:
        nk_label(ctx, "Project files", NK_TEXT_LEFT);
        {
            const char *items[] = {"src/main.c", "src/ui.c", "assets/", "README.md"};
            int i;
            nk_layout_row_dynamic(ctx, 20, 1);
            for (i = 0; i < 4; ++i)
                nk_selectable_label(ctx, items[i], NK_TEXT_LEFT,
                                    &(nk_bool){(i == 0) ? nk_true : nk_false});
        }
        break;
    case PANE_PROPERTIES:
        nk_label(ctx, "Selected object", NK_TEXT_LEFT);
        {
            static int opacity = 80;
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_property_int(ctx, "Opacity:", 0, &opacity, 100, 1, 1);
            nk_layout_row_dynamic(ctx, 40, 1);
            {
                struct nk_image img = nk_image_id((int)swatch_tex);
                nk_image(ctx, img);
            }
        }
        break;
    case PANE_CONSOLE:
        nk_label(ctx, "Build output", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label_colored(ctx, "cc -c ui.c", NK_TEXT_LEFT,
                         nk_rgb(180, 220, 180));
        nk_label_colored(ctx, "linking demo ... ok", NK_TEXT_LEFT,
                         nk_rgb(180, 220, 180));
        break;
    default:
        nk_label(ctx, "Color palette", NK_TEXT_LEFT);
        {
            static const struct nk_color cols[6] = {
                {220,80,80,255},{220,170,60,255},{120,200,90,255},
                {70,160,220,255},{120,110,220,255},{210,110,190,255}
            };
            int i;
            nk_layout_row_dynamic(ctx, 34, 6);
            for (i = 0; i < 6; ++i) {
                if (nk_button_color(ctx, cols[i]))
                    (void)0;
            }
        }
        break;
    }
}

/* ---- unattended interaction script for recording ---- */
struct script_step {
    int frame;
    int x, y;
    int down;          /* 1 = left held, 0 = released */
    const char *title; /* segment title shown while this step is active */
};

static const struct script_step script[] = {
    {  0,   0,   0, 0, "1. Initial three-zone layout"},
    { 60,   0,   0, 0, "2. Drag left splitter to widen zone"},
    { 62, 270, 400, 1, ""},
    { 90, 430, 400, 1, ""},
    { 92, 430, 400, 0, ""},
    {140,   0,   0, 0, "3. Drag Explorer out into a floating window"},
    {142, 140,  52, 1, ""},
    {175, 600, 300, 1, ""},
    {177, 600, 300, 0, ""},
    {230,   0,   0, 0, "4. Drop Explorer onto the right edge to re-dock"},
    {232, 590, 312, 1, ""},
    {268, 1140, 400, 1, ""},
    {270, 1140, 400, 0, ""},
    {320,   0,   0, 0, "5. Move the floating Properties window"},
    {322, 1062, 428, 1, ""},
    {355, 700, 200, 1, ""},
    {357, 700, 200, 0, ""},
    {410,   0,   0, 0, "6. Drag right splitter, then inspect state query"},
    {412, 924, 400, 1, ""},
    {445, 800, 400, 1, ""},
    {447, 800, 400, 0, ""},
    {560,   0,   0, 0, "Done"},
};
#define SCRIPT_COUNT (int)(sizeof(script) / sizeof(script[0]))

int
main(int argc, char **argv)
{
    struct nk_glfw glfw = {0};
    GLFWwindow *win;
    struct nk_context *ctx;
    struct nk_dock dock;
    struct nk_colorf bg;
    int script_mode = 0;
    int hold_mode = 0;
    const char *record_dir = 0;
    int argi;
    for (argi = 1; argi < argc; ++argi) {
        if (strcmp(argv[argi], "--script") == 0) script_mode = 1;
        else if (strcmp(argv[argi], "--hold") == 0) hold_mode = 1;
        else if (strcmp(argv[argi], "--record") == 0 &&
                 argi + 1 < argc)
            record_dir = argv[++argi];
    }
    int width = 0, height = 0;
    int frame = 0;
    int step_idx = 0;
    const char *segment_title = script[0].title;
    int mouse_down = 0, mouse_x = 0, mouse_y = 0;
    const double frame_dt = 1.0 / 60.0;
    double next_time = 0.0;
    double script_clock = 0.0;
    struct recorder rec;
    memset(&rec, 0, sizeof(rec));

    glfwSetErrorCallback(0);
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    win = glfwCreateWindow(WIN_W, WIN_H, "nuklear dock demo", NULL, NULL);
    {
        int mx, my;
        const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode) {
            mx = (mode->width - WIN_W) / 2;
            my = (mode->height - WIN_H) / 2;
            glfwSetWindowPos(win, mx, my);
        }
    }
    glfwShowWindow(win);
    glfwMakeContextCurrent(win);
    glfwGetFramebufferSize(win, &width, &height);

    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "glewInit failed\n");
        return 1;
    }

    ctx = nk_glfw3_init_fixed(&glfw, win, NK_GLFW3_INSTALL_CALLBACKS,
                              fixed_memory, sizeof(fixed_memory));
    {
        struct nk_font_atlas *atlas;
        nk_glfw3_font_stash_begin(&glfw, &atlas);
        nk_glfw3_font_stash_end(&glfw);
    }
    make_swatch();

    if (record_dir)
        recorder_init(&rec, WIN_W, WIN_H, record_dir);

    bg.r = 0.10f; bg.g = 0.18f; bg.b = 0.24f; bg.a = 1.0f;

    nk_dock_init(&dock);
    nk_dock_add(&dock, "Explorer", NK_DOCK_LEFT,
                nk_rect(60, 80, 240, 320));
    nk_dock_add(&dock, "Properties", NK_DOCK_RIGHT,
                nk_rect(900, 80, 240, 320));
    nk_dock_add(&dock, "Console", NK_DOCK_CENTER,
                nk_rect(360, 480, 460, 200));
    nk_dock_add(&dock, "Palette", NK_DOCK_CENTER,
                nk_rect(420, 160, 360, 240));

    while (!glfwWindowShouldClose(win)) {
        int fbw, fbh;

        glfwPollEvents();

        if (script_mode) {
            double now = glfwGetTime();
            if (next_time == 0.0) next_time = now;
            /* fixed 60 Hz pacing so recorded timing and pauses are stable */
            while (now < next_time) {
                glfwWaitEventsTimeout(next_time - now);
                now = glfwGetTime();
            }
            script_clock += frame_dt;
            next_time += frame_dt;
        }

        if (script_mode) {
            /* The synthetic-input path bypasses nk_glfw3_new_frame, so keep
             * the backend framebuffer metrics current for its projection. */
            glfwGetWindowSize(win, &glfw.width, &glfw.height);
            glfwGetFramebufferSize(win, &glfw.display_width,
                                   &glfw.display_height);
            glfw.fb_scale.x = (float)glfw.display_width /
                              (float)glfw.width;
            glfw.fb_scale.y = (float)glfw.display_height /
                              (float)glfw.height;
            /* drive synthetic input through the public nk_input API */
            while (step_idx < SCRIPT_COUNT &&
                   (double)script[step_idx].frame / 60.0 <= script_clock) {
                const struct script_step *st = &script[step_idx];
                if (st->title[0]) segment_title = st->title;
                if (st->x || st->y) {
                    mouse_x = st->x; mouse_y = st->y;
                    mouse_down = st->down;
                }
                step_idx++;
            }
            nk_input_begin(ctx);
            nk_input_motion(ctx, mouse_x, mouse_y);
            nk_input_button(ctx, NK_BUTTON_LEFT, mouse_x, mouse_y,
                            mouse_down ? nk_true : nk_false);
            nk_input_end(ctx);
            if (step_idx >= SCRIPT_COUNT && !hold_mode)
                glfwSetWindowShouldClose(win, nk_true);
        } else {
            nk_glfw3_new_frame(&glfw);
        }

        glfwGetFramebufferSize(win, &fbw, &fbh);
        {
            struct nk_rect area = nk_rect(8, 36,
                                          (float)fbw - 16,
                                          (float)fbh - 44);
            int i;

            nk_dock_begin_frame(&dock, ctx, area);
            for (i = 0; i < dock.count; ++i) {
                if (nk_dock_begin_pane(&dock, ctx, i))
                    pane_body(ctx, i);
                nk_dock_end_pane(&dock, ctx, i);
            }
            nk_dock_draw_drop_zones(&dock, ctx);

            /* splitter hover cursor feedback */
            if (dock.hover_splitter >= 0 || dock.drag_splitter >= 0)
                nk_style_set_cursor(ctx, NK_CURSOR_RESIZE_HORIZONTAL);

            /* segment title + live state query, fixed buffer, no alloc */
            if (nk_begin(ctx, segment_title, nk_rect(8, 4, 560, 30),
                         NK_WINDOW_BORDER | NK_WINDOW_NO_INPUT |
                         NK_WINDOW_NO_SCROLLBAR)) {
                nk_layout_row_dynamic(ctx, 26, 1);
                nk_label(ctx, segment_title, NK_TEXT_LEFT);
            }
            nk_end(ctx);

            if (nk_begin(ctx, "Dock Query", nk_rect((float)fbw - 308, 36,
                                                    300, 168),
                         NK_WINDOW_BORDER | NK_WINDOW_TITLE |
                         NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                         NK_WINDOW_NO_SCROLLBAR)) {
                static char report[1024];
                int n = nk_dock_query(&dock, report, (int)sizeof(report));
                nk_layout_row_dynamic(ctx, 150, 1);
                nk_edit_string_zero_terminated(ctx,
                    NK_EDIT_MULTILINE | NK_EDIT_READ_ONLY,
                    report, (int)sizeof(report), nk_filter_default);
                (void)n;
            }
            nk_end(ctx);
        }

        glViewport(0, 0, fbw, fbh);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        if (rec.enabled)
            glBindFramebuffer(GL_FRAMEBUFFER, rec.fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        nk_glfw3_render(&glfw, NK_ANTI_ALIASING_ON,
                        MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        if (rec.enabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            recorder_capture(&rec);
        }
        glfwSwapBuffers(win);
        frame++;
    }

    nk_glfw3_shutdown(&glfw);
    glfwTerminate();
    return 0;
}
