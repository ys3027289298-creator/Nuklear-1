#include "nuklear.h"
#include "nuklear_internal.h"

/* hexadecimal text field embedded into the color picker bounds.
 * All state lives in caller-owned storage so neither dragging nor
 * typing causes a heap allocation inside the library. */
#define NK_COLOR_HEX_RGB_MAX  7  /* 6 hex digits + NUL */
#define NK_COLOR_HEX_RGBA_MAX 9  /* 8 hex digits + NUL */

NK_LIB nk_bool
nk_color_picker_hex_behavior(nk_flags *state, const struct nk_input *in,
    struct nk_rect hex_bounds, nk_bool *hex_active,
    struct nk_text_edit *edit)
{
    nk_bool clicked;

    NK_ASSERT(state);
    NK_ASSERT(hex_active);
    NK_ASSERT(edit);

    if (!in) return nk_false;
    clicked = nk_button_behavior(state, hex_bounds, in, NK_BUTTON_DEFAULT);
    if (clicked)
        *hex_active = nk_true;
    /* clicking anywhere outside the text field commits and ends editing */
    if (*hex_active && in->mouse.buttons[NK_BUTTON_LEFT].clicked) {
        if (!NK_INBOX(in->mouse.pos.x, in->mouse.pos.y,
                hex_bounds.x, hex_bounds.y, hex_bounds.w, hex_bounds.h)) {
            *hex_active = nk_false;
            edit->active = nk_false;
        }
    }
    if (nk_input_is_key_pressed(in, NK_KEY_ENTER)) {
        *hex_active = nk_false;
        edit->active = nk_false;
    }
    return clicked;
}

/* parse a committed hex string back into the color; a partial or
 * empty string is ignored so an in-progress edit never corrupts it */
NK_LIB void
nk_color_picker_hex_apply(const char *buffer, struct nk_colorf *col,
    enum nk_color_format fmt, int len)
{
    struct nk_color rgb;
    const char *digits;

    NK_ASSERT(buffer);
    NK_ASSERT(col);
    /* tolerate an optional leading '#', matching nk_rgb_hex/nk_rgba_hex */
    digits = buffer;
    if (len > 0 && buffer[0] == '#') {
        digits = buffer + 1;
        len--;
    }
    if (fmt == NK_RGBA) {
        if (len < 8) return;
        rgb = nk_rgba_hex(digits);
        *col = nk_color_cf(rgb);
    } else {
        if (len < 6) return;
        rgb = nk_rgb_hex(digits);
        col->r = rgb.r / 255.0f;
        col->g = rgb.g / 255.0f;
        col->b = rgb.b / 255.0f;
    }
}

/* write the current picker color into the fixed buffer */
NK_LIB int
nk_color_picker_hex_format(char *buffer, struct nk_colorf col,
    enum nk_color_format fmt)
{
    struct nk_color rgb;
    NK_ASSERT(buffer);
    rgb = nk_rgb_cf(col);
    if (fmt == NK_RGBA)
    {
        nk_color_hex_rgba(buffer, rgb);
        return 8;
    } else {
        nk_color_hex_rgb(buffer, rgb);
        return 6;
    }
}

/* ==============================================================
 *
 *                          COLOR PICKER
 *
 * ===============================================================*/
NK_LIB nk_bool
nk_color_picker_behavior(nk_flags *state,
    const struct nk_rect *bounds, const struct nk_rect *matrix,
    const struct nk_rect *hue_bar, const struct nk_rect *alpha_bar,
    struct nk_colorf *color, const struct nk_input *in)
{
    float hsva[4];
    nk_bool value_changed = 0;
    nk_bool hsv_changed = 0;

    NK_ASSERT(state);
    NK_ASSERT(matrix);
    NK_ASSERT(hue_bar);
    NK_ASSERT(color);

    /* color matrix */
    nk_colorf_hsva_fv(hsva, *color);
    if (nk_button_behavior(state, *matrix, in, NK_BUTTON_REPEATER)) {
        hsva[1] = NK_SATURATE((in->mouse.pos.x - matrix->x) / (matrix->w-1));
        hsva[2] = 1.0f - NK_SATURATE((in->mouse.pos.y - matrix->y) / (matrix->h-1));
        value_changed = hsv_changed = 1;
    }
    /* hue bar */
    if (nk_button_behavior(state, *hue_bar, in, NK_BUTTON_REPEATER)) {
        hsva[0] = NK_SATURATE((in->mouse.pos.y - hue_bar->y) / (hue_bar->h-1));
        value_changed = hsv_changed = 1;
    }
    /* alpha bar */
    if (alpha_bar) {
        if (nk_button_behavior(state, *alpha_bar, in, NK_BUTTON_REPEATER)) {
            hsva[3] = 1.0f - NK_SATURATE((in->mouse.pos.y - alpha_bar->y) / (alpha_bar->h-1));
            value_changed = 1;
        }
    }
    nk_widget_state_reset(state);
    if (hsv_changed) {
        *color = nk_hsva_colorfv(hsva);
        *state = NK_WIDGET_STATE_ACTIVE;
    }
    if (value_changed) {
        color->a = hsva[3];
        *state = NK_WIDGET_STATE_ACTIVE;
    }
    /* set color picker widget state */
    if (nk_input_is_mouse_hovering_rect(in, *bounds))
        *state = NK_WIDGET_STATE_HOVERED;
    if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, *bounds))
        *state |= NK_WIDGET_STATE_ENTERED;
    else if (nk_input_is_mouse_prev_hovering_rect(in, *bounds))
        *state |= NK_WIDGET_STATE_LEFT;
    return value_changed;
}
NK_LIB void
nk_draw_color_picker(struct nk_command_buffer *o, const struct nk_rect *matrix,
    const struct nk_rect *hue_bar, const struct nk_rect *alpha_bar,
    struct nk_colorf col)
{
    NK_STORAGE const struct nk_color black = {0,0,0,255};
    NK_STORAGE const struct nk_color white = {255, 255, 255, 255};
    NK_STORAGE const struct nk_color black_trans = {0,0,0,0};

    const float crosshair_size = 7.0f;
    struct nk_color temp;
    float hsva[4];
    float line_y;
    int i;

    NK_ASSERT(o);
    NK_ASSERT(matrix);
    NK_ASSERT(hue_bar);

    /* draw hue bar */
    nk_colorf_hsva_fv(hsva, col);
    for (i = 0; i < 6; ++i) {
        NK_GLOBAL const struct nk_color hue_colors[] = {
            {255, 0, 0, 255}, {255,255,0,255}, {0,255,0,255}, {0, 255,255,255},
            {0,0,255,255}, {255, 0, 255, 255}, {255, 0, 0, 255}
        };
        nk_fill_rect_multi_color(o,
            nk_rect(hue_bar->x, hue_bar->y + (float)i * (hue_bar->h/6.0f) + 0.5f,
                hue_bar->w, (hue_bar->h/6.0f) + 0.5f), hue_colors[i], hue_colors[i],
                hue_colors[i+1], hue_colors[i+1]);
    }
    line_y = (float)(int)(hue_bar->y + hsva[0] * matrix->h + 0.5f);
    nk_stroke_line(o, hue_bar->x-1, line_y, hue_bar->x + hue_bar->w + 2,
        line_y, 1, nk_rgb(255,255,255));

    /* draw alpha bar */
    if (alpha_bar) {
        float alpha = NK_SATURATE(col.a);
        line_y = (float)(int)(alpha_bar->y +  (1.0f - alpha) * matrix->h + 0.5f);

        nk_fill_rect_multi_color(o, *alpha_bar, white, white, black, black);
        nk_stroke_line(o, alpha_bar->x-1, line_y, alpha_bar->x + alpha_bar->w + 2,
            line_y, 1, nk_rgb(255,255,255));
    }

    /* draw color matrix */
    temp = nk_hsv_f(hsva[0], 1.0f, 1.0f);
    nk_fill_rect_multi_color(o, *matrix, white, temp, temp, white);
    nk_fill_rect_multi_color(o, *matrix, black_trans, black_trans, black, black);

    /* draw cross-hair */
    {struct nk_vec2 p; float S = hsva[1]; float V = hsva[2];
    p.x = (float)(int)(matrix->x + S * matrix->w);
    p.y = (float)(int)(matrix->y + (1.0f - V) * matrix->h);
    nk_stroke_line(o, p.x - crosshair_size, p.y, p.x-2, p.y, 1.0f, white);
    nk_stroke_line(o, p.x + crosshair_size + 1, p.y, p.x+3, p.y, 1.0f, white);
    nk_stroke_line(o, p.x, p.y + crosshair_size + 1, p.x, p.y+3, 1.0f, white);
    nk_stroke_line(o, p.x, p.y - crosshair_size, p.x, p.y-2, 1.0f, white);}
}
NK_LIB nk_bool
nk_do_color_picker(nk_flags *state,
    struct nk_command_buffer *out, struct nk_colorf *col,
    enum nk_color_format fmt, struct nk_rect bounds,
    struct nk_vec2 padding, const struct nk_input *in,
    const struct nk_user_font *font)
{
    NK_ASSERT(out);
    NK_ASSERT(col);
    NK_ASSERT(state);
    NK_ASSERT(font);
    if (!out || !col || !state || !font) return 0;

    return nk_do_color_picker_hex(state, out, col, fmt, bounds, padding,
                                  in, font, 0, 0, 0, 0, 0);
}
NK_LIB nk_bool
nk_do_color_picker_hex(nk_flags *state,
    struct nk_command_buffer *out, struct nk_colorf *col,
    enum nk_color_format fmt, struct nk_rect bounds,
    struct nk_vec2 padding, const struct nk_input *in,
    const struct nk_user_font *font,
    char *hex_buffer, int *hex_len, nk_bool *hex_active,
    struct nk_text_edit *hex_edit,
    const struct nk_style_edit *edit_style)
{
    int ret = 0;
    struct nk_rect matrix;
    struct nk_rect hue_bar;
    struct nk_rect alpha_bar;
    struct nk_rect hex_bounds;
    float bar_w;
    float hex_h;

    NK_ASSERT(out);
    NK_ASSERT(col);
    NK_ASSERT(state);
    NK_ASSERT(font);
    if (!out || !col || !state || !font)
        return ret;

    bar_w = font->height;
    bounds.x += padding.x;
    bounds.y += padding.x;
    bounds.w -= 2 * padding.x;
    bounds.h -= 2 * padding.y;

    matrix.x = bounds.x;
    matrix.y = bounds.y;
    if (hex_buffer) {
        float edit_pad_y = edit_style ? edit_style->padding.y : 0.0f;
        float edit_border = edit_style ? edit_style->border : 0.0f;
        /* size the embedded edit from its own style so the text area
         * is tall enough for the font (padding + border included) */
        hex_h = font->height + 2.0f * edit_pad_y + 2.0f * edit_border;
        matrix.h = NK_MAX(0.0f, bounds.h - (hex_h + padding.y));
        hex_bounds.x = bounds.x;
        hex_bounds.y = bounds.y + matrix.h + padding.y;
        hex_bounds.w = bounds.w;
        hex_bounds.h = hex_h;
    } else {
        matrix.h = bounds.h;
        hex_h = 0.0f;
    }
    matrix.w = bounds.w - (3 * padding.x + 2 * bar_w);

    hue_bar.w = bar_w;
    hue_bar.y = bounds.y;
    hue_bar.h = matrix.h;
    hue_bar.x = matrix.x + matrix.w + padding.x;

    alpha_bar.x = hue_bar.x + hue_bar.w + padding.x;
    alpha_bar.y = bounds.y;
    alpha_bar.w = bar_w;
    alpha_bar.h = matrix.h;

    ret = nk_color_picker_behavior(state, &bounds, &matrix, &hue_bar,
        (fmt == NK_RGBA) ? &alpha_bar:0, col, in);
    nk_draw_color_picker(out, &matrix, &hue_bar, (fmt == NK_RGBA) ? &alpha_bar:0, *col);

    /* embedded hexadecimal input field, drawn inside the same widget
     * bounds so neighboring layout rows keep their original positions */
    if (hex_buffer && hex_len && hex_active && hex_edit) {
        nk_flags edit_flags;
        int max = (fmt == NK_RGBA) ? NK_COLOR_HEX_RGBA_MAX: NK_COLOR_HEX_RGB_MAX;
        nk_bool was_active = *hex_active;
        nk_bool now_active;
        struct nk_input *edit_in = (struct nk_input*)in;

        /* 1. update edit activation from mouse/keyboard input first so
         *      the text field and the HSV controls never fight over input */
        nk_color_picker_hex_behavior(state, in, hex_bounds,
                                     hex_active, hex_edit);
        now_active = *hex_active;

        /* 2. on the frame editing ends, parse the typed hex value.
         *      read the length before the fixed buffer is re-bound below */
        if (was_active && !now_active) {
            nk_color_picker_hex_apply(hex_buffer, col, fmt, *hex_len);
        }

        /* 3. while inactive the field mirrors the picker color */
        if (!now_active)
            *hex_len = nk_color_picker_hex_format(hex_buffer, *col, fmt);

        /* 4. bind the caller-owned fixed buffer (no allocation). Reserve
         *      one extra byte for the terminating NUL so all hex digits
         *      fit (6/8 content bytes + NUL = 7/9 total). Do NOT call
         *      nk_textedit_clear_state here: it would wipe the caret and
         *      selection that nk_do_edit must carry across frames. */
        *hex_len = NK_MIN(*hex_len, max - 1);
        nk_str_init_fixed(&hex_edit->string, hex_buffer, (nk_size)(max + 1));
        hex_edit->string.buffer.allocated = (nk_size)*hex_len;
        hex_edit->string.len = nk_utf_len(hex_buffer, *hex_len);
        hex_edit->single_line = 1;
        hex_edit->filter = nk_filter_hex;
        hex_edit->active = now_active;
        hex_edit->mode = NK_TEXT_EDIT_MODE_INSERT;
        if (!was_active) {
            /* fresh activation: start with the caret at the end */
            hex_edit->cursor = *hex_len;
            hex_edit->select_start = *hex_len;
            hex_edit->select_end = *hex_len;
        } else {
            hex_edit->cursor = NK_CLAMP(0, hex_edit->cursor, *hex_len);
            hex_edit->select_start = NK_CLAMP(0, hex_edit->select_start, *hex_len);
            hex_edit->select_end = NK_CLAMP(0, hex_edit->select_end, *hex_len);
        }

        edit_flags = NK_EDIT_SIMPLE;
        nk_widget_state_reset(state);
        nk_do_edit(state, out, hex_bounds, edit_flags, nk_filter_hex,
                   hex_edit, edit_style, now_active ? edit_in : 0, font);
        /* write back the length the editor produced this frame and keep
         * the fixed buffer NUL-terminated for direct caller inspection */
        *hex_len = (int)hex_edit->string.buffer.allocated;
        hex_buffer[*hex_len] = '\0';
    }
    return ret;
}
NK_API nk_bool
nk_color_pick(struct nk_context * ctx, struct nk_colorf *color,
    enum nk_color_format fmt)
{
    struct nk_window *win;
    struct nk_panel *layout;
    const struct nk_style *config;
    const struct nk_input *in;

    enum nk_widget_layout_states state;
    struct nk_rect bounds;

    NK_ASSERT(ctx);
    NK_ASSERT(color);
    NK_ASSERT(ctx->current);
    NK_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout || !color)
        return 0;

    win = ctx->current;
    config = &ctx->style;
    layout = win->layout;
    state = nk_widget(&bounds, ctx);
    if (!state) return 0;
    in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
    return nk_do_color_picker(&ctx->last_widget_state, &win->buffer, color, fmt, bounds,
                nk_vec2(0,0), in, config->font);
}
NK_API struct nk_colorf
nk_color_picker(struct nk_context *ctx, struct nk_colorf color,
    enum nk_color_format fmt)
{
    nk_color_pick(ctx, &color, fmt);
    return color;
}

NK_API nk_bool
nk_color_pick_hex(struct nk_context *ctx, struct nk_colorf *color,
    enum nk_color_format fmt, char *hex_buffer, int hex_max,
    int *hex_len, nk_bool *hex_active, struct nk_text_edit *hex_edit)
{
    struct nk_window *win;
    struct nk_panel *layout;
    const struct nk_style *config;
    const struct nk_input *in;
    enum nk_widget_layout_states state;
    struct nk_rect bounds;
    int need;

    NK_ASSERT(ctx);
    NK_ASSERT(color);
    NK_ASSERT(hex_buffer);
    NK_ASSERT(hex_len);
    NK_ASSERT(hex_active);
    NK_ASSERT(hex_edit);
    NK_ASSERT(ctx->current);
    NK_ASSERT(ctx->current->layout);
    if (!ctx || !color || !hex_buffer || !hex_len || !hex_active || !hex_edit ||
        !ctx->current || !ctx->current->layout)
        return 0;

    need = (fmt == NK_RGBA) ? NK_COLOR_HEX_RGBA_MAX: NK_COLOR_HEX_RGB_MAX;
    if (hex_max < need) return 0;

    win = ctx->current;
    config = &ctx->style;
    layout = win->layout;
    state = nk_widget(&bounds, ctx);
    if (!state) return 0;
    in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED ||
          layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
    return nk_do_color_picker_hex(&ctx->last_widget_state, &win->buffer,
        color, fmt, bounds, nk_vec2(0,0), in, config->font,
        hex_buffer, hex_len, hex_active, hex_edit, &config->edit);
}
