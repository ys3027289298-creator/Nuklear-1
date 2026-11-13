/*
 * nk_dock.c - three-zone docking layout helper for nuklear.
 * Public domain. No heap allocation: all state is caller-provided.
 */
#include "nk_dock.h"

#include <assert.h>
#include <stdio.h>

#define NK_DOCK_ASSERT(expr) assert(expr)
#define NK_DOCK_INVALID (-1)

static float
dock_header_height(const struct nk_context *ctx)
{
    return ctx->style.window.header.padding.y * 2.0f +
           ctx->style.font->height;
}

NK_API const char *
nk_dock_zone_name(enum nk_dock_zone zone)
{
    switch (zone) {
    case NK_DOCK_LEFT:   return "Left";
    case NK_DOCK_CENTER: return "Center";
    case NK_DOCK_RIGHT:  return "Right";
    default:             return "Floating";
    }
}

NK_API void
nk_dock_init(struct nk_dock *dock)
{
    int i;
    if (!dock) return;
    for (i = 0; i < NK_DOCK_MAX_PANES; ++i) {
        dock->panes[i].name = 0;
        dock->panes[i].zone = NK_DOCK_NONE;
        dock->panes[i].open = nk_false;
        dock->panes[i].floating = nk_rect(0, 0, 0, 0);
        dock->panes[i].rect = nk_rect(0, 0, 0, 0);
    }
    dock->count = 0;
    dock->area = nk_rect(0, 0, 0, 0);
    dock->zone[NK_DOCK_LEFT] = nk_rect(0, 0, 0, 0);
    dock->zone[NK_DOCK_CENTER] = nk_rect(0, 0, 0, 0);
    dock->zone[NK_DOCK_RIGHT] = nk_rect(0, 0, 0, 0);
    dock->left_frac = 0.22f;
    dock->right_frac = 0.22f;
    dock->drag_pane = NK_DOCK_INVALID;
    dock->hover_splitter = NK_DOCK_INVALID;
    dock->drag_splitter = NK_DOCK_INVALID;
    dock->drop_hint = NK_DOCK_NONE;
    for (i = 0; i < NK_DOCK_MAX_PANES; ++i)
        dock->zone_changed[i] = 0;
}

NK_API int
nk_dock_add(struct nk_dock *dock, const char *name,
            enum nk_dock_zone zone, struct nk_rect floating)
{
    int idx;
    if (!dock || !name || dock->count >= NK_DOCK_MAX_PANES) return NK_DOCK_INVALID;
    idx = dock->count++;
    dock->panes[idx].name = name;
    dock->panes[idx].zone = zone;
    dock->panes[idx].open = nk_true;
    dock->panes[idx].floating = floating;
    dock->panes[idx].rect = floating;
    return idx;
}

NK_API struct nk_rect
nk_dock_splitter_rect(const struct nk_dock *dock, int which)
{
    float cx;
    if (!dock) return nk_rect(0, 0, 0, 0);
    if (which == 0)
        cx = dock->area.x + dock->area.w * dock->left_frac;
    else
        cx = dock->area.x + dock->area.w * (1.0f - dock->right_frac);
    return nk_rect(cx - NK_DOCK_SPLITTER_W * 0.5f, dock->area.y,
                   NK_DOCK_SPLITTER_W, dock->area.h);
}

static int
point_in_rect(struct nk_vec2 p, struct nk_rect r)
{
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

/* Edge-triggered: true only on the frame the left button goes down.
 * A release also increments `clicked`, so the down state must be checked. */
static nk_bool
left_pressed(const struct nk_context *ctx)
{
    const struct nk_mouse_button *b =
        &ctx->input.mouse.buttons[NK_BUTTON_LEFT];
    return (b->down && b->clicked) ? nk_true : nk_false;
}

static void
layout_zones(struct nk_dock *dock)
{
    struct nk_rect a = dock->area;
    float lw = a.w * dock->left_frac;
    float rw = a.w * dock->right_frac;
    dock->zone[NK_DOCK_LEFT] =
        nk_rect(a.x, a.y, lw - NK_DOCK_GAP, a.h);
    dock->zone[NK_DOCK_CENTER] =
        nk_rect(a.x + lw, a.y, a.w - lw - rw, a.h);
    dock->zone[NK_DOCK_RIGHT] =
        nk_rect(a.x + a.w - rw + NK_DOCK_GAP, a.y,
                rw - NK_DOCK_GAP, a.h);
}

static void
update_splitters(struct nk_dock *dock, const struct nk_context *ctx)
{
    struct nk_vec2 m = ctx->input.mouse.pos;
    struct nk_rect s0 = nk_dock_splitter_rect(dock, 0);
    struct nk_rect s1 = nk_dock_splitter_rect(dock, 1);

    if (dock->drag_splitter == NK_DOCK_INVALID) {
        dock->hover_splitter = NK_DOCK_INVALID;
        if (point_in_rect(m, s0)) dock->hover_splitter = 0;
        else if (point_in_rect(m, s1)) dock->hover_splitter = 1;
        if (dock->hover_splitter != NK_DOCK_INVALID &&
            left_pressed(ctx))
            dock->drag_splitter = dock->hover_splitter;
    }

    if (dock->drag_splitter != NK_DOCK_INVALID) {
        float rel = (m.x - dock->area.x) / dock->area.w;
        if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
            if (dock->drag_splitter == 0) {
                if (rel < NK_DOCK_MIN_ZONE_W / dock->area.w)
                    rel = NK_DOCK_MIN_ZONE_W / dock->area.w;
                if (rel > 1.0f - dock->right_frac -
                         NK_DOCK_MIN_ZONE_W / dock->area.w)
                    rel = 1.0f - dock->right_frac -
                          NK_DOCK_MIN_ZONE_W / dock->area.w;
                dock->left_frac = rel;
            } else {
                if (rel < dock->left_frac +
                         NK_DOCK_MIN_ZONE_W / dock->area.w)
                    rel = dock->left_frac +
                          NK_DOCK_MIN_ZONE_W / dock->area.w;
                if (rel > 1.0f - NK_DOCK_MIN_ZONE_W / dock->area.w)
                    rel = 1.0f - NK_DOCK_MIN_ZONE_W / dock->area.w;
                dock->right_frac = 1.0f - rel;
            }
            layout_zones(dock);
        } else {
            dock->drag_splitter = NK_DOCK_INVALID;
        }
    }
}

static enum nk_dock_zone
zone_at_point(const struct nk_dock *dock, struct nk_vec2 p)
{
    if (!point_in_rect(p, dock->area)) return NK_DOCK_NONE;
    /* Docking only triggers on the edge strips; the wide middle band is
     * free space where the pane stays (or becomes) floating. */
    if (p.x <= dock->area.x + NK_DOCK_EDGE_SIZE) return NK_DOCK_LEFT;
    if (p.x >= dock->area.x + dock->area.w - NK_DOCK_EDGE_SIZE)
        return NK_DOCK_RIGHT;
    return NK_DOCK_NONE;
}

/* Handles header drags for every pane: undocking, moving a floating pane,
 * and re-docking onto an edge strip on release. All panes are moved through
 * this single path so floating and docked behaviour stay consistent. */
static void
update_pane_drag(struct nk_dock *dock, const struct nk_context *ctx)
{
    struct nk_vec2 m = ctx->input.mouse.pos;
    int i;

    if (dock->drag_pane == NK_DOCK_INVALID) {
        if (dock->drag_splitter != NK_DOCK_INVALID) return;
        if (!left_pressed(ctx)) return;
        for (i = 0; i < dock->count; ++i) {
            struct nk_dock_pane *p = &dock->panes[i];
            struct nk_rect header;
            if (!p->open) continue;
            header = p->rect;
            header.h = dock_header_height(ctx);
            if (point_in_rect(m, header)) {
                dock->drag_pane = i;
                if (p->zone != NK_DOCK_NONE) {
                    /* Undock: keep the remembered floating size instead of
                     * inheriting the full-height docked rect. */
                    p->zone = NK_DOCK_NONE;
                    dock->zone_changed[i] = 1;
                    p->floating.x = m.x - (p->floating.w * 0.5f);
                    p->floating.y = m.y - header.h * 0.5f;
                    p->rect = p->floating;
                }
                break;
            }
        }
        return;
    }

    {
        struct nk_dock_pane *p = &dock->panes[dock->drag_pane];
        if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
            float hh = dock_header_height(ctx);
            /* Follow the cursor; only position changes, the remembered
             * floating width/height are preserved. */
            p->rect.x = m.x - (p->rect.w * 0.5f);
            p->rect.y = m.y - hh * 0.5f;
            p->floating.x = p->rect.x;
            p->floating.y = p->rect.y;
            dock->drop_hint = zone_at_point(dock, m);
        } else {
            enum nk_dock_zone target = zone_at_point(dock, m);
            if (target != NK_DOCK_NONE) {
                p->zone = target;
                dock->zone_changed[dock->drag_pane] = 1;
            }
            else {
                p->floating.x = p->rect.x;
                p->floating.y = p->rect.y;
            }
            dock->drag_pane = NK_DOCK_INVALID;
            dock->drop_hint = NK_DOCK_NONE;
        }
    }
}

/* Even vertical stack of open panes inside each dock zone. Floating panes
 * (zone == NK_DOCK_NONE) keep their own rect and are never touched here. */
static void
layout_docked_panes(struct nk_dock *dock)
{
    static const enum nk_dock_zone zones[3] = {
        NK_DOCK_LEFT, NK_DOCK_CENTER, NK_DOCK_RIGHT
    };
    int z, i;
    for (z = 0; z < 3; ++z) {
        struct nk_rect zr = dock->area;
        int members[NK_DOCK_MAX_PANES];
        int n = 0;
        float y, slot;
        for (i = 0; i < dock->count; ++i)
            if (dock->panes[i].open &&
                dock->panes[i].zone != NK_DOCK_NONE &&
                dock->panes[i].zone == zones[z] &&
                dock->drag_pane != i)
                members[n++] = i;
        if (!n) continue;
        zr = dock->zone[zones[z]];
        slot = (zr.h - NK_DOCK_GAP * (float)(n - 1)) / (float)n;
        y = zr.y;
        for (i = 0; i < n; ++i) {
            dock->panes[members[i]].rect =
                nk_rect(zr.x, y, zr.w, slot);
            y += slot + NK_DOCK_GAP;
        }
    }
}

NK_API void
nk_dock_begin_frame(struct nk_dock *dock, struct nk_context *ctx,
                    struct nk_rect area)
{
    NK_DOCK_ASSERT(dock && ctx);
    dock->area = area;
    layout_zones(dock);
    update_splitters(dock, ctx);
    layout_docked_panes(dock);
    update_pane_drag(dock, ctx);
}

NK_API nk_bool
nk_dock_begin_pane(struct nk_dock *dock, struct nk_context *ctx, int idx)
{
    struct nk_dock_pane *p;
    nk_flags flags;
    NK_DOCK_ASSERT(dock && idx >= 0 && idx < dock->count);
    p = &dock->panes[idx];
    if (!p->open) return nk_false;

    flags = NK_WINDOW_TITLE | NK_WINDOW_BORDER |
            NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE |
            NK_WINDOW_SCROLL_AUTO_HIDE;
    if (p->zone == NK_DOCK_NONE)
        flags |= NK_WINDOW_SCALABLE;

    /* Nuklear keeps a persistent window per name and reuses its old bounds.
     * For floating panes, and on the frame any pane changes zone, force the
     * module-owned rect so an undocked pane does not inherit the full-height
     * docked bounds and a dragged floating window follows the cursor. */
    if (dock->zone_changed[idx] ||
        (p->zone == NK_DOCK_NONE && dock->drag_pane == idx)) {
        struct nk_window *win = nk_window_find(ctx, p->name);
        if (win) win->bounds = p->rect;
    }
    if (dock->zone_changed[idx])
        dock->zone_changed[idx] = 0;

    return nk_begin(ctx, p->name, p->rect, flags);
}

NK_API void
nk_dock_end_pane(struct nk_dock *dock, struct nk_context *ctx, int idx)
{
    struct nk_dock_pane *p;
    NK_DOCK_ASSERT(dock && idx >= 0 && idx < dock->count);
    p = &dock->panes[idx];
    if (!p->open) return;
    if (nk_window_is_closed(ctx, p->name)) p->open = nk_false;
    if (p->zone == NK_DOCK_NONE) {
        struct nk_rect b = nk_window_get_bounds(ctx);
        /* While a header drag is active the module owns the position; at
         * other times adopt nuklear's bounds, but always keep size in sync. */
        if (dock->drag_pane != idx) {
            p->rect.x = b.x;
            p->rect.y = b.y;
            p->floating.x = b.x;
            p->floating.y = b.y;
        }
        p->rect.w = b.w;
        p->rect.h = b.h;
        p->floating.w = b.w;
        p->floating.h = b.h;
    }
    nk_end(ctx);
}

NK_API void
nk_dock_draw_splitters(struct nk_dock *dock, struct nk_context *ctx)
{
    /* Splitters are painted by the shared overlay so a single background
     * window handles both drop zones and splitter feedback. */
    nk_dock_draw_drop_zones(dock, ctx);
}

NK_API void
nk_dock_draw_drop_zones(struct nk_dock *dock, struct nk_context *ctx)
{
    struct nk_command_buffer *canvas;
    int show_drop = (dock->drag_pane != NK_DOCK_INVALID);
    int show_split = (dock->hover_splitter != NK_DOCK_INVALID ||
                      dock->drag_splitter != NK_DOCK_INVALID);
    if (!show_drop && !show_split) return;
    if (nk_begin(ctx, "##nk_dock_overlay", dock->area,
                 NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT |
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NOT_INTERACTIVE)) {
        canvas = nk_window_get_canvas(ctx);
        if (show_drop) {
            static const enum nk_dock_zone zones[3] = {
                NK_DOCK_LEFT, NK_DOCK_CENTER, NK_DOCK_RIGHT
            };
            int i;
            for (i = 0; i < 3; ++i) {
                struct nk_rect r = dock->zone[zones[i]];
                struct nk_color c = (zones[i] == dock->drop_hint)
                    ? nk_rgba(80, 160, 255, 110)
                    : nk_rgba(255, 255, 255, 28);
                nk_fill_rect(canvas, r, 6.0f, c);
            }
        }
        if (show_split) {
            int i;
            for (i = 0; i < 2; ++i) {
                struct nk_rect r = nk_dock_splitter_rect(dock, i);
                struct nk_color c = (dock->drag_splitter == i ||
                                     dock->hover_splitter == i)
                    ? nk_rgba(90, 170, 255, 200)
                    : nk_rgba(255, 255, 255, 60);
                nk_fill_rect(canvas, r, 2.0f, c);
            }
        }
    }
    nk_end(ctx);
}

NK_API int
nk_dock_query(const struct nk_dock *dock, char *out, int out_size)
{
    int i, used = 0;
    int n;
    NK_DOCK_ASSERT(dock && out && out_size > 0);
    #define NK_DOCK_PUT(...) do { \
        n = snprintf(out + used, (size_t)(out_size - used), __VA_ARGS__); \
        if (n < 0 || n >= out_size - used) { out[out_size - 1] = 0; return used; } \
        used += n; \
    } while (0)

    NK_DOCK_PUT("layout: left=%.0f%% right=%.0f%% center=%.0f%%\n",
                dock->left_frac * 100.0f, dock->right_frac * 100.0f,
                (1.0f - dock->left_frac - dock->right_frac) * 100.0f);
    for (i = 0; i < dock->count; ++i) {
        const struct nk_dock_pane *p = &dock->panes[i];
        NK_DOCK_PUT("[%s] %s open=%d rect=(%.0f,%.0f %.0fx%.0f) float=(%.0f,%.0f %.0fx%.0f)\n",
                    nk_dock_zone_name(p->zone), p->name, (int)p->open,
                    p->rect.x, p->rect.y, p->rect.w, p->rect.h,
                    p->floating.x, p->floating.y,
                    p->floating.w, p->floating.h);
    }
    #undef NK_DOCK_PUT
    return used;
}
