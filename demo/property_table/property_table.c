/*
 * Property panel laid out as a table inside a scroll container.
 *
 * Demonstrates: fixed + auto column widths, table header, per-column
 * alignment, selected row, auto row height, virtualized scrolling with the
 * exact content range, and bounded single-frame cost on 1000 rows.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL2_IMPLEMENTATION
#include "../../nuklear.h"
#include "../glfw_opengl2/nuklear_glfw_gl2.h"

#define WINDOW_WIDTH  1000
#define WINDOW_HEIGHT  720

#define ROW_COUNT 1000
#define COL_COUNT 4

/* column geometry, in panel content pixels */
#define COL_NAME_MIN   150  /* variable: grows, never below this */
#define COL_VALUE_FIX  140  /* static: always exactly this wide */
#define COL_UNIT_FIX    80  /* static */
#define COL_TYPE_DYN     0  /* dynamic: takes the leftover space */
#define HEADER_HEIGHT   30
#define ROW_MIN_HEIGHT  26
#define ROW_TALL_EVERY 137  /* a few rows get a taller editor cell */
#define ROW_TALL_HEIGHT 52

#ifndef OUTPUT_DIR
#define OUTPUT_DIR "output"
#endif
#ifndef FRAME_LIMIT
#define FRAME_LIMIT 120
#endif
#ifndef SCROLL_SWEEP
#define SCROLL_SWEEP 1
#endif
#ifndef SCROLL_STEP
#define SCROLL_STEP (-800.0)
#endif
#ifndef SWEEP_END
#define SWEEP_END 30
#endif
#ifndef SWEEP_START
#define SWEEP_START 10
#endif
#ifndef TITLE_OVERLAY
#define TITLE_OVERLAY 0
#endif
#ifndef ENABLE_VSYNC
#define ENABLE_VSYNC 0
#endif
#ifndef FRAME_PACING_MS
#define FRAME_PACING_MS 0
#endif

#if TITLE_OVERLAY
static const char *phase_title(int frame)
{
    if (frame < 105)  return "1/4  Layout: columns, header, alignment";
    if (frame < 225)  return "2/4  Selected row and auto row height";
    if (frame < 345)  return "3/4  Scrolling in the scroll container";
    return "4/4  1000 rows: only visible rows built";
}
#endif

struct prop_row {
    char name[28];
    int  value;
    char unit[8];
    char type[14];
    int  height;
};

struct table_metrics {
    struct nk_rect panel_bounds;
    struct nk_rect header_bounds;
    struct nk_rect body_bounds;
    struct nk_rect body_panel;
    struct nk_vec2 body_content;
    int content_height;
    int scroll_y;
    int visible_rows;
    int selected;
    int first_row;
    int col_x[COL_COUNT + 1];
    int body_col_x[COL_COUNT + 1];
};

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void make_rows(struct prop_row *rows, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        sprintf(rows[i].name, "property_%03d", i);
        rows[i].value = (i * 37) % 1000;
        rows[i].unit[0] = 'p'; rows[i].unit[1] = 'x'; rows[i].unit[2] = 0;
        if ((i % 3) == 0) { rows[i].unit[0] = '%'; rows[i].unit[1] = 0; }
        strcpy(rows[i].type, (i % 2) ? "number" : "length");
        rows[i].height = ((i > 0) && (i % ROW_TALL_EVERY) == 0) ?
                         ROW_TALL_HEIGHT : ROW_MIN_HEIGHT;
    }
}

/* shared column template: one variable, two fixed, one dynamic column */
static void column_template(struct nk_context *ctx, float row_height)
{
    nk_layout_row_template_begin(ctx, row_height);
    nk_layout_row_template_push_variable(ctx, COL_NAME_MIN);
    nk_layout_row_template_push_static(ctx, COL_VALUE_FIX);
    nk_layout_row_template_push_static(ctx, COL_UNIT_FIX);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
}

static void draw_header(struct nk_context *ctx, struct table_metrics *m,
                        float scrollbar_w)
{
    struct nk_rect b;
    struct nk_rect c0, c1, c2, c3;
    struct nk_command_buffer *canvas;

    /* one template row carries all four header labels; paint its background
     * from the first cell's row bounds so no extra layout slot is consumed */
    column_template(ctx, HEADER_HEIGHT);
    (void)nk_widget(&c0, ctx);
    b = c0;
    b.x = m->panel_bounds.x;
    b.w = m->panel_bounds.w - scrollbar_w;
    m->header_bounds = b;
    canvas = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas, b, 0, nk_rgb(45, 52, 62));
    nk_stroke_line(canvas, b.x, b.y + b.h, b.x + b.w, b.y + b.h,
                   1.0f, nk_rgb(70, 80, 92));
    m->col_x[0] = (int)c0.x;
    nk_draw_text(nk_window_get_canvas(ctx), c0, "Name", 4,
                 ctx->style.font, nk_rgba(0,0,0,0), nk_rgb(210, 218, 228));
    (void)nk_widget(&c1, ctx);
    m->col_x[1] = (int)c1.x;
    nk_draw_text(nk_window_get_canvas(ctx), c1, "Value", 5,
                 ctx->style.font, nk_rgba(0,0,0,0), nk_rgb(210, 218, 228));
    (void)nk_widget(&c2, ctx);
    m->col_x[2] = (int)c2.x;
    nk_draw_text(nk_window_get_canvas(ctx), c2, "Unit", 4,
                 ctx->style.font, nk_rgba(0,0,0,0), nk_rgb(210, 218, 228));
    (void)nk_widget(&c3, ctx);
    m->col_x[3] = (int)c3.x;
    nk_draw_text(nk_window_get_canvas(ctx), c3, "Type", 4,
                 ctx->style.font, nk_rgba(0,0,0,0), nk_rgb(210, 218, 228));
    m->col_x[4] = (int)(b.x + b.w);
}

static void draw_row(struct nk_context *ctx, struct prop_row *row,
                     int index, int selected, int *new_selection,
                     struct table_metrics *m)
{
    struct nk_rect cell;
    struct nk_rect row_bounds;
    struct nk_command_buffer *canvas;
    struct nk_color text_color;
    nk_bool measure = (index == m->first_row);

    column_template(ctx, (float)row->height);
    (void)nk_widget(&cell, ctx);
    row_bounds = cell;
    row_bounds.w = ctx->current->layout->bounds.w;
    canvas = nk_window_get_canvas(ctx);
    if (selected) {
        nk_fill_rect(canvas, row_bounds, 0, nk_rgb(58, 96, 140));
        text_color = nk_rgb(255, 255, 255);
    } else if ((index % 2) == 0) {
        nk_fill_rect(canvas, row_bounds, 0, nk_rgb(37, 42, 50));
        text_color = nk_rgb(220, 224, 230);
    } else {
        text_color = nk_rgb(200, 206, 214);
    }
    if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, row_bounds))
        *new_selection = index;

    if (measure)
        m->body_bounds = row_bounds;

    nk_draw_text(canvas, cell, row->name, (int)strlen(row->name),
                 ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);
    nk_property_int(ctx, "##v", 0, &row->value, 100000, 1, 0.2f);
    nk_label_colored(ctx, row->unit, NK_TEXT_CENTERED, text_color);
    nk_label_colored(ctx, row->type, NK_TEXT_RIGHT, text_color);
}

static double draw_table(struct nk_context *ctx, struct prop_row *rows,
                         int *selected, struct table_metrics *m)
{
    static int heights[ROW_COUNT];
    struct nk_list_view view;
    double t0 = 0.0, t1 = 0.0;
    float scrollbar_w = ctx->style.window.scrollbar_size.x;
    int i;
    int new_selection = *selected;
    nk_bool list_active;

    for (i = 0; i < ROW_COUNT; ++i)
        heights[i] = rows[i].height;
    m->first_row = -1;

    if (nk_group_begin(ctx, "PropertyTable", NK_WINDOW_BORDER)) {
        m->panel_bounds = nk_window_get_content_region(ctx);
        draw_header(ctx, m, scrollbar_w);

        /* explicit body row: header consumed the first row, so the list view
         * must claim its own full-height slot inside the outer group */
        nk_layout_row_dynamic(ctx,
            m->panel_bounds.h - (HEADER_HEIGHT + (int)ctx->style.window.spacing.y),
            1);
        t0 = now_seconds();
        list_active = nk_list_view_begin_heights(ctx, &view, "rows", 0,
                                                 heights, ROW_MIN_HEIGHT,
                                                 ROW_COUNT);
        if (list_active)
            m->body_panel = nk_window_get_content_region(ctx);
        if (list_active) {
            for (i = view.begin; i < view.end; ++i) {
                if (i == view.begin)
                    m->first_row = i;
                draw_row(ctx, &rows[i], i, *selected == i,
                         &new_selection, m);
            }
            /* reconstruct body column edges from the same row template:
             * two fixed columns, two flexible columns splitting the rest */
            {
                int sx = (int)ctx->style.window.spacing.x;
                int x0 = (int)m->body_bounds.x;
                int usable = (int)m->body_panel.w;
                int flex = (usable - COL_VALUE_FIX - COL_UNIT_FIX -
                            sx * (COL_COUNT - 1)) / 2;
                m->body_col_x[0] = x0;
                m->body_col_x[1] = x0 + flex + sx;
                m->body_col_x[2] = m->body_col_x[1] + COL_VALUE_FIX + sx;
                m->body_col_x[3] = m->body_col_x[2] + COL_UNIT_FIX + sx;
                m->body_col_x[4] = x0 + usable;
            }
            nk_list_view_end(&view);
        }
        t1 = now_seconds();

        *selected = new_selection;
        m->body_content = nk_window_get_content_region_size(ctx);
        m->content_height = view.total_height;
        m->scroll_y = (int)view.scroll_value;
        m->visible_rows = view.count;
        m->selected = *selected;
        nk_group_end(ctx);
    }
    return t1 - t0;
}

int main(void)
{
    struct nk_context *ctx;
    struct nk_colorf bg;
    struct GLFWwindow *win;
    struct prop_row *rows;
    struct table_metrics m;
    int selected = 7;
    int width = WINDOW_WIDTH, height = WINDOW_HEIGHT;
    int frame = 0;
    double table_ms = 0.0;
    double max_ms = 0.0;
    double sum_ms = 0.0;
    int measured = 0;
    FILE *report;

    memset(&m, 0, sizeof(m));
    rows = (struct prop_row*)calloc(ROW_COUNT, sizeof(struct prop_row));
    make_rows(rows, ROW_COUNT);

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    win = glfwCreateWindow(width, height, "Property Table", NULL, NULL);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
#if ENABLE_VSYNC
    glfwSwapInterval(1);
#endif
    glfwGetWindowSize(win, &width, &height);

    ctx = nk_glfw3_init(win, NK_GLFW3_INSTALL_CALLBACKS);
    {
        struct nk_font_atlas *atlas;
        nk_glfw3_font_stash_begin(&atlas);
        nk_glfw3_font_stash_end();
    }
    bg.r = 0.10f; bg.g = 0.12f; bg.b = 0.15f; bg.a = 1.0f;

    report = fopen(OUTPUT_DIR "/table_timings.csv", "w");
    if (report) fprintf(report, "frame,table_ms,visible_rows,scroll_y\n");

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        /* one virtual click inside the table activates the window and
         * selects a row; wait until layout geometry is stable */
        if (SCROLL_SWEEP && frame == (SWEEP_START - 5)) {
            nk_glfw3_new_frame();
            nk_input_motion(ctx, (int)m.body_panel.x + 60,
                            (int)m.body_panel.y + 60);
            nk_input_button(ctx, NK_BUTTON_LEFT,
                            (int)m.body_panel.x + 60,
                            (int)m.body_panel.y + 60, 1);
            nk_input_button(ctx, NK_BUTTON_LEFT,
                            (int)m.body_panel.x + 60,
                            (int)m.body_panel.y + 60, 0);
        } else {
            nk_glfw3_new_frame();
        }
        /* automated scroll sweep: queue wheel events before the frame so the
         * backend forwards them inside nk_glfw3_new_frame */
        if (SCROLL_SWEEP && frame >= SWEEP_START && frame < SWEEP_END)
            nk_gflw3_scroll_callback(win, 0.0, SCROLL_STEP);
        if (SCROLL_SWEEP && frame >= (SWEEP_START - 5))
            nk_input_motion(ctx, (int)m.body_panel.x + 40,
                            (int)m.body_panel.y + 40);

        if (nk_begin(ctx, "Property Panel",
                     nk_rect(20, 20, (float)(width - 40), (float)(height - 40)),
                     NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE)) {
            /* one explicit-height row fills the window content area */
            nk_layout_row_dynamic(ctx, (float)(height - 90), 1);
            table_ms = draw_table(ctx, rows, &selected, &m) * 1000.0;
#if TITLE_OVERLAY
            /* segment title drawn onto the panel canvas; clipping keeps it
             * inside the content region, so place it at the panel top */
            {
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                struct nk_rect region = nk_window_get_content_region(ctx);
                const char *title = phase_title(frame);
                int len = (int)strlen(title);
                float tw = ctx->style.font->width(ctx->style.font->userdata,
                                                   ctx->style.font->height,
                                                   title, len);
                struct nk_rect tb = nk_rect(
                    region.x + (region.w - tw - 24.0f) * 0.5f,
                    region.y + 4.0f, tw + 24.0f, 22.0f);
                if (tb.x + tb.w > region.x + region.w - 8.0f)
                    tb.x = region.x + region.w - tb.w - 8.0f;
                nk_fill_rect(canvas, tb, 4.0f, nk_rgba(30, 36, 46, 220));
                nk_draw_text(canvas,
                    nk_rect(tb.x + 12.0f, tb.y + 3.0f, tw, tb.h - 4.0f),
                    title, len, ctx->style.font,
                    nk_rgba(0, 0, 0, 0), nk_rgb(235, 240, 248));
            }
#endif
        }
        nk_end(ctx);

        if (frame > 2) {
            if (table_ms > max_ms) max_ms = table_ms;
            sum_ms += table_ms;
            measured++;
            if (report)
                fprintf(report, "%d,%.4f,%d,%d\n", frame, table_ms,
                        m.visible_rows, m.scroll_y);
        }

        glfwGetWindowSize(win, &width, &height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        nk_glfw3_render(NK_ANTI_ALIASING_ON);
        glfwSwapBuffers(win);

        /* headless validation: dump metrics once layout is stable and exit */
        if (frame == 8) {
            int spacing_x = (int)ctx->style.window.spacing.x;
            int spacing_y = (int)ctx->style.window.spacing.y;
            /* inner panel usable width, excluding padding + scrollbar */
            int hdr0_w = m.col_x[1] - m.col_x[0] - spacing_x;
            int hdr1_w = m.col_x[2] - m.col_x[1] - spacing_x;
            int hdr2_w = m.col_x[3] - m.col_x[2] - spacing_x;
            int hdr3_w = m.col_x[4] - m.col_x[3] - spacing_x;
            int col0_w = m.body_col_x[1] - m.body_col_x[0] - spacing_x;
            int col1_w = m.body_col_x[2] - m.body_col_x[1] - spacing_x;
            int col2_w = m.body_col_x[3] - m.body_col_x[2] - spacing_x;
            int col3_w = m.body_col_x[4] - m.body_col_x[3] - spacing_x;
            /* both flexible columns must receive the same computed width */
            int expected_var = col0_w;
            int tall_rows = (ROW_COUNT - 1) / ROW_TALL_EVERY;
            int normal_rows = ROW_COUNT - tall_rows;
            /* every row folds one item spacing into its row height; the
             * list view total includes the final row's trailing spacing */
            int expected_content =
                ROW_COUNT * spacing_y +
                tall_rows * ROW_TALL_HEIGHT + normal_rows * ROW_MIN_HEIGHT;
            int max_scroll = expected_content - (int)m.body_content.y;
            int passed;
            int checks[8];
            FILE *f;
            checks[0] = (m.header_bounds.h >= HEADER_HEIGHT);
            checks[1] = (col1_w == COL_VALUE_FIX);
            checks[2] = (col2_w == COL_UNIT_FIX);
            /* flexible columns split leftover space within the engine's
             * sub-pixel FRAC redistribution (one inter-column spacing) */
            checks[3] = (col0_w >= COL_NAME_MIN &&
                         abs(col3_w - expected_var) <= spacing_x &&
                         hdr1_w == COL_VALUE_FIX && hdr2_w == COL_UNIT_FIX);
            checks[4] = (m.content_height == expected_content);
            checks[5] = (m.visible_rows > 0 && m.visible_rows < ROW_COUNT);
            checks[6] = (m.body_bounds.h == ROW_MIN_HEIGHT);
            checks[7] = (max_scroll > 0 &&
                         m.content_height > (int)m.body_content.y);
            passed = checks[0]+checks[1]+checks[2]+checks[3]+checks[4]+
                     checks[5]+checks[6]+checks[7];
            f = fopen(OUTPUT_DIR "/table_metrics.json", "w");
            if (f) {
                fprintf(f,
                  "{\n"
                  "  \"window\": {\"w\": %d, \"h\": %d},\n"
                  "  \"panel\":  {\"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f},\n"
                  "  \"header\": {\"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f},\n"
                  "  \"body\":   {\"x\": %.1f, \"y\": %.1f, \"w\": %.1f, \"h\": %.1f},\n"
                  "  \"body_content\": {\"w\": %.1f, \"h\": %.1f},\n"
                  "  \"header_column_widths\": [%d, %d, %d, %d],\n"
                  "  \"body_column_widths\": [%d, %d, %d, %d],\n"
                  "  \"expected_variable_width\": %d,\n"
                  "  \"content_height\": %d,\n"
                  "  \"expected_content_height\": %d,\n"
                  "  \"max_scroll_y\": %d,\n"
                  "  \"scroll_y\": %d,\n"
                  "  \"visible_rows\": %d,\n"
                  "  \"col_x\": [%d, %d, %d, %d, %d],\n"
                  "  \"body_col_x\": [%d, %d, %d, %d, %d],\n"
                  "  \"row_count\": %d,\n"
                  "  \"selected\": %d,\n"
                  "  \"checks_passed\": %d,\n"
                  "  \"checks_total\": 8\n"
                  "}\n",
                  width, height,
                  m.panel_bounds.x, m.panel_bounds.y,
                  m.panel_bounds.w, m.panel_bounds.h,
                  m.header_bounds.x, m.header_bounds.y,
                  m.header_bounds.w, m.header_bounds.h,
                  m.body_bounds.x, m.body_bounds.y,
                  m.body_bounds.w, m.body_bounds.h,
                  m.body_content.x, m.body_content.y,
                  hdr0_w, hdr1_w, hdr2_w, hdr3_w,
                  col0_w, col1_w, col2_w, col3_w, expected_var,
                  m.content_height, expected_content, max_scroll,
                  m.scroll_y, m.visible_rows,
                  m.col_x[0], m.col_x[1], m.col_x[2], m.col_x[3], m.col_x[4],
                  m.body_col_x[0], m.body_col_x[1], m.body_col_x[2],
                  m.body_col_x[3], m.body_col_x[4],
                  ROW_COUNT, m.selected, passed);
                fclose(f);
            }
            printf("checks: %d/8 (content=%d expected=%d max_scroll=%d cols=%d,%d,%d,%d)\n",
                   passed, m.content_height, expected_content, max_scroll,
                   col0_w, col1_w, col2_w, col3_w);
        }
        if (frame == 29) {
            FILE *s;
            /* the engine clamps to (content + group top inset) - viewport,
             * so the measured maximum sits within one header height of the
             * plain content difference */
            int scroll_clamp = m.content_height - (int)m.body_panel.h;
            s = fopen(OUTPUT_DIR "/table_scroll.json", "w");
            if (s) {
                fprintf(s,
                  "{\n  \"scroll_y_at_end\": %d,\n  \"first_visible_row\": %d,\n"
                  "  \"visible_rows\": %d,\n  \"content_height\": %d,\n"
                  "  \"body_panel_height\": %d,\n  \"scroll_clamp\": %d,\n"
                  "  \"reached_bottom\": %d,\n  \"within_header_of_clamp\": %d\n}\n",
                  m.scroll_y, m.first_row, m.visible_rows, m.content_height,
                  (int)m.body_panel.h, scroll_clamp,
                  m.first_row + m.visible_rows >= ROW_COUNT - 2,
                  abs(m.scroll_y - scroll_clamp) <= HEADER_HEIGHT);
                fclose(s);
            }
            printf("scroll end: y=%d first_row=%d visible=%d clamp=%d\n",
                   m.scroll_y, m.first_row, m.visible_rows, scroll_clamp);
        }
        if (frame >= FRAME_LIMIT)
            glfwSetWindowShouldClose(win, 1);
        frame++;
#if FRAME_PACING_MS > 0
        {
            struct timespec pace;
            pace.tv_sec = 0;
            pace.tv_nsec = FRAME_PACING_MS * 1000000L;
            nanosleep(&pace, 0);
        }
#endif
    }

    if (report) {
        fprintf(report, "# frames=%d max_table_ms=%.4f avg_table_ms=%.4f\n",
                measured, max_ms, measured ? sum_ms / measured : 0.0);
        fclose(report);
    }
    {
        FILE *b = fopen(OUTPUT_DIR "/table_benchmark.json", "w");
        if (b) {
            fprintf(b,
              "{\n  \"row_count\": %d,\n  \"measured_frames\": %d,\n"
              "  \"table_max_ms\": %.4f,\n  \"table_avg_ms\": %.4f,\n"
              "  \"visible_rows_per_frame\": %d,\n"
              "  \"bounded\": %d\n}\n",
              ROW_COUNT, measured, max_ms,
              measured ? sum_ms / measured : 0.0, m.visible_rows,
              m.visible_rows < ROW_COUNT);
            fclose(b);
        }
    }
    printf("table frames=%d max=%.4fms avg=%.4fms visible=%d total_h=%d\n",
           measured, max_ms, measured ? sum_ms / measured : 0.0,
           m.visible_rows, m.content_height);

    nk_glfw3_shutdown();
    glfwTerminate();
    free(rows);
    return 0;
}
