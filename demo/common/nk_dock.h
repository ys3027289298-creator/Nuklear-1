/*
 * nk_dock.h - three-zone docking layout helper for nuklear.
 *
 * Public domain. This is an add-on module: it only consumes the public
 * nuklear API and never touches nuklear internals, so existing floating
 * window behaviour is untouched.
 *
 * Design constraints honored by this module:
 *  - Left/Center/Right dock zones with draggable splitters between them.
 *  - Panes (nuklear windows) can be dragged out of a zone into a floating
 *    window and dropped back onto a zone edge to re-dock.
 *  - All state lives in a single fixed-size struct; there is no heap
 *    allocation anywhere, in particular none inside a frame.
 */

#ifndef NK_DOCK_H
#define NK_DOCK_H

#ifndef NK_NUKLEAR_H_
#include <nuklear.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NK_DOCK_MAX_PANES     16
#define NK_DOCK_SPLITTER_W    8.0f
#define NK_DOCK_GAP           4.0f
#define NK_DOCK_EDGE_SIZE     96.0f
#define NK_DOCK_MIN_ZONE_W    120.0f

enum nk_dock_zone {
    NK_DOCK_NONE = 0,   /* floating window, not docked */
    NK_DOCK_LEFT = 1,
    NK_DOCK_CENTER = 2,
    NK_DOCK_RIGHT = 3
};

struct nk_dock_pane {
    const char *name;
    enum nk_dock_zone zone;
    nk_bool open;
    struct nk_rect floating;   /* last floating position/size, restored on undock */
    struct nk_rect rect;       /* computed frame rect while docked */
};

struct nk_dock {
    struct nk_dock_pane panes[NK_DOCK_MAX_PANES];
    int count;

    struct nk_rect area;       /* full docking work area, set per frame */
    struct nk_rect zone[4];    /* index 1..3: LEFT, CENTER, RIGHT */
    float left_frac;           /* fraction of width given to the left zone */
    float right_frac;          /* fraction of width given to the right zone */

    int drag_pane;             /* pane index being manually dragged, -1 if none */
    int hover_splitter;        /* 0 = left splitter, 1 = right splitter, -1 if none */
    int drag_splitter;         /* splitter being dragged, -1 if none */
    enum nk_dock_zone drop_hint; /* zone that would accept the current drop */
    int zone_changed[NK_DOCK_MAX_PANES]; /* 1 on the frame a pane's zone changed */
};

/* Lifecycle */
NK_API void nk_dock_init(struct nk_dock *dock);
NK_API int  nk_dock_add(struct nk_dock *dock, const char *name,
                        enum nk_dock_zone zone, struct nk_rect floating);

/* Frame layout: computes zone rectangles, processes splitter drags and keeps
 * docked panes inside their zones. Call once per frame before the panes. */
NK_API void nk_dock_begin_frame(struct nk_dock *dock, struct nk_context *ctx,
                                struct nk_rect area);

/* Pane helpers wrapping nk_begin/nk_end. Returns the same value as nk_begin
 * so the caller can skip body content when collapsed. */
NK_API nk_bool nk_dock_begin_pane(struct nk_dock *dock, struct nk_context *ctx,
                                  int pane_index);
NK_API void nk_dock_end_pane(struct nk_dock *dock, struct nk_context *ctx,
                             int pane_index);

/* Draws the semi-transparent drop-zone highlights while a pane is dragged. */
NK_API void nk_dock_draw_drop_zones(struct nk_dock *dock, struct nk_context *ctx);

/* Draws the two splitters, highlighting the hovered or dragged one. */
NK_API void nk_dock_draw_splitters(struct nk_dock *dock, struct nk_context *ctx);

/* State query: fills a caller-provided fixed buffer, never allocates.
 * Returns the number of characters written (excluding the terminating NUL). */
NK_API int nk_dock_query(const struct nk_dock *dock, char *out, int out_size);

/* Splitter geometry, mainly useful for tests. */
NK_API struct nk_rect nk_dock_splitter_rect(const struct nk_dock *dock, int which);
NK_API const char *nk_dock_zone_name(enum nk_dock_zone zone);

#ifdef __cplusplus
}
#endif

#endif /* NK_DOCK_H */
