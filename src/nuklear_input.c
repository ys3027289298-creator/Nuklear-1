#include "nuklear.h"
#include "nuklear_internal.h"

/* ===============================================================
 *
 *                          INPUT
 *
 * ===============================================================*/
NK_API void
nk_input_begin(struct nk_context *ctx)
{
    int i;
    struct nk_input *in;
    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    for (i = 0; i < NK_BUTTON_MAX; ++i)
        in->mouse.buttons[i].clicked = 0;

    in->keyboard.text_len = 0;
    in->mouse.scroll_delta = nk_vec2(0,0);
    in->mouse.prev.x = in->mouse.pos.x;
    in->mouse.prev.y = in->mouse.pos.y;
    in->mouse.delta.x = 0;
    in->mouse.delta.y = 0;
    for (i = 0; i < NK_KEY_MAX; i++)
        in->keyboard.keys[i].clicked = 0;

    /* IME events live for a single frame: events queued after this call
     * are consumed by the active edit widget during the widget pass and
     * stale events are dropped here at the beginning of the next frame.
     * The composition (preedit) string itself persists between frames. */
    ctx->ime.commit_pending = nk_false;
    ctx->ime.commit_len = 0;
    ctx->ime.cancel_pending = nk_false;
    ctx->ime.preedit_dirty = nk_false;
}
NK_API void
nk_input_end(struct nk_context *ctx)
{
    struct nk_input *in;
    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    if (in->mouse.grab)
        in->mouse.grab = 0;
    if (in->mouse.ungrab) {
        in->mouse.grabbed = 0;
        in->mouse.ungrab = 0;
        in->mouse.grab = 0;
    }
}
NK_API void
nk_input_motion(struct nk_context *ctx, int x, int y)
{
    struct nk_input *in;
    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    in->mouse.pos.x = (float)x;
    in->mouse.pos.y = (float)y;
    in->mouse.delta.x = in->mouse.pos.x - in->mouse.prev.x;
    in->mouse.delta.y = in->mouse.pos.y - in->mouse.prev.y;
}
NK_API void
nk_input_key(struct nk_context *ctx, enum nk_keys key, nk_bool down)
{
    struct nk_input *in;
    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
#ifdef NK_KEYSTATE_BASED_INPUT
    if (in->keyboard.keys[key].down != down)
        in->keyboard.keys[key].clicked++;
#else
    in->keyboard.keys[key].clicked++;
#endif
    in->keyboard.keys[key].down = down;
}
NK_API void
nk_input_button(struct nk_context *ctx, enum nk_buttons id, int x, int y, nk_bool down)
{
    struct nk_mouse_button *btn;
    struct nk_input *in;
    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    if (in->mouse.buttons[id].down == down) return;

    btn = &in->mouse.buttons[id];
    btn->clicked_pos.x = (float)x;
    btn->clicked_pos.y = (float)y;
    btn->down = down;
    btn->clicked++;

    /* Fix Click-Drag for touch events. */
    in->mouse.delta.x = 0;
    in->mouse.delta.y = 0;
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
    if (down == 1 && id == NK_BUTTON_LEFT)
    {
        in->mouse.down_pos.x = btn->clicked_pos.x;
        in->mouse.down_pos.y = btn->clicked_pos.y;
    }
#endif
}
NK_API void
nk_input_scroll(struct nk_context *ctx, struct nk_vec2 val)
{
    NK_ASSERT(ctx);
    if (!ctx) return;
    ctx->input.mouse.scroll_delta.x += val.x;
    ctx->input.mouse.scroll_delta.y += val.y;
}
NK_API void
nk_input_glyph(struct nk_context *ctx, const nk_glyph glyph)
{
    int len = 0;
    nk_rune unicode;
    struct nk_input *in;

    NK_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;

    len = nk_utf_decode(glyph, &unicode, NK_UTF_SIZE);
    if (len && ((in->keyboard.text_len + len) < NK_INPUT_MAX)) {
        nk_utf_encode(unicode, &in->keyboard.text[in->keyboard.text_len],
            NK_INPUT_MAX - in->keyboard.text_len);
        in->keyboard.text_len += len;
    }
}
NK_API void
nk_input_char(struct nk_context *ctx, char c)
{
    nk_glyph glyph = {0};
    NK_ASSERT(ctx);
    if (!ctx) return;
    glyph[0] = c;
    nk_input_glyph(ctx, glyph);
}
NK_API void
nk_input_unicode(struct nk_context *ctx, nk_rune unicode)
{
    nk_glyph rune;
    NK_ASSERT(ctx);
    if (!ctx) return;
    nk_utf_encode(unicode, rune, NK_UTF_SIZE);
    nk_input_glyph(ctx, rune);
}
NK_API nk_bool
nk_input_has_mouse_click(const struct nk_input *i, enum nk_buttons id)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
    return (btn->clicked && btn->down == nk_false) ? nk_true : nk_false;
}
NK_API nk_bool
nk_input_has_mouse_click_in_rect(const struct nk_input *i, enum nk_buttons id,
    struct nk_rect b)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
    if (!NK_INBOX(btn->clicked_pos.x,btn->clicked_pos.y,b.x,b.y,b.w,b.h))
        return nk_false;
    return nk_true;
}
NK_API nk_bool
nk_input_has_mouse_click_in_button_rect(const struct nk_input *i, enum nk_buttons id,
    struct nk_rect b)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
    if (!NK_INBOX(btn->clicked_pos.x,btn->clicked_pos.y,b.x,b.y,b.w,b.h)
        || !NK_INBOX(i->mouse.down_pos.x,i->mouse.down_pos.y,b.x,b.y,b.w,b.h))
#else
    if (!NK_INBOX(btn->clicked_pos.x,btn->clicked_pos.y,b.x,b.y,b.w,b.h))
#endif
        return nk_false;
    return nk_true;
}
NK_API nk_bool
nk_input_has_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id,
    struct nk_rect b, nk_bool down)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
    return nk_input_has_mouse_click_in_rect(i, id, b) && (btn->down == down);
}
NK_API nk_bool
nk_input_is_mouse_click_in_rect(const struct nk_input *i, enum nk_buttons id,
    struct nk_rect b)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
    return (nk_input_has_mouse_click_down_in_rect(i, id, b, nk_false) &&
            btn->clicked) ? nk_true : nk_false;
}
NK_API nk_bool
nk_input_is_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id,
    struct nk_rect b, nk_bool down)
{
    const struct nk_mouse_button *btn;
    if (!i) return nk_false;
    btn = &i->mouse.buttons[id];
    return (nk_input_has_mouse_click_down_in_rect(i, id, b, down) &&
            btn->clicked) ? nk_true : nk_false;
}
NK_API nk_bool
nk_input_any_mouse_click_in_rect(const struct nk_input *in, struct nk_rect b)
{
    int i, down = 0;
    for (i = 0; i < NK_BUTTON_MAX; ++i)
        down = down || nk_input_is_mouse_click_in_rect(in, (enum nk_buttons)i, b);
    return down;
}
NK_API nk_bool
nk_input_is_mouse_hovering_rect(const struct nk_input *i, struct nk_rect rect)
{
    if (!i) return nk_false;
    return NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h);
}

/**
 * # nk_input_is_mouse_hovering_still_rect
 * Returns true if the mouse is hovering over rect and hasn't moved since the last
 * frame, false otherwise.
 */
NK_API nk_bool
nk_input_is_mouse_hovering_still_rect(const struct nk_input *i, struct nk_rect rect)
{
    if (!i) return nk_false;
    return (NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h) &&
            !nk_input_is_mouse_moved(i));
}

/**
 * # nk_input_is_mouse_hovering_delay_rect
 * Returns true if the mouse has been hovering over rect for `delay` seconds or more.
 *
 * Parameter   | Description
 * ------------|---------------------------------------------------------------
 * \param[in] ctx     | Must point to an either stack or heap allocated `nk_context` struct
 * \param[in] rect    | The rect area you're checking against
 * \param[in|out] timer | Must point to a float used to track the total seconds hovered across frames
 * \param[in] delay     | The wait time in seconds
 *
 * \returns `true` if the mouse has hovered long enough, `false otherwise`
 */
NK_API nk_bool
nk_input_is_mouse_hovering_delay_rect(const struct nk_context *ctx, struct nk_rect rect, float* timer, float delay)
{
    NK_ASSERT(ctx);
    if (!ctx) {
        return nk_false;
    } else {
        const struct nk_input* i = &ctx->input;
        if (NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h)) {
            *timer += ctx->delta_time_seconds;
            return *timer >= delay;
        } else if (NK_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h)) {
            *timer = 0;
        }
        return nk_false;
    }

}

/**
 * # nk_input_is_mouse_hovering_still_delay_rect
 * Returns true if the mouse has been hovering motionless over rect for `delay` seconds or more. The timer
 * does not reset once the delay is reached as long as you are still hovering over the rect.
 *
 * Parameter   | Description
 * ------------|---------------------------------------------------------------
 * \param[in] ctx     | Must point to an either stack or heap allocated `nk_context` struct
 * \param[in] rect    | The rect area you're checking against
 * \param[in|out] timer | Must point to a float used to track the total seconds hovered across frames
 * \param[in] delay     | The wait time in seconds
 *
 * \returns `true` if the mouse has hovered without moving long enough, `false otherwise`
 */
NK_API nk_bool
nk_input_is_mouse_hovering_still_delay_rect(const struct nk_context *ctx, struct nk_rect rect, float* timer, float delay)
{
    NK_ASSERT(ctx);
    if (!ctx) {
        return nk_false;
    } else {
        const struct nk_input* i = &ctx->input;
        if (NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h)) {
            /* once it triggers, moving within the bounds should not make it disappear */
            if (*timer >= delay) {
                return nk_true;
            }
            if (!nk_input_is_mouse_moved(i)) {
                *timer += ctx->delta_time_seconds;
                return *timer >= delay;
            }
            *timer = 0;
        } else if (NK_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h)) {
            *timer = 0;
        }
        return nk_false;
    }
}

/**
 * # nk_input_is_mouse_hovering_still_delay_clicked_rect
 * Works the same as `nk_input_is_mouse_hovering_still_delay_rect` unless clicked is set. If clicked is true,
 * then it returns false regardless of the timer value as long as the mouse is motionless. It goes back to working
 * as normal once the mouse has moved (clicked is set to false, timer to 0).
 *
 * Parameter   | Description
 * ------------|---------------------------------------------------------------
 * \param[in] ctx     | Must point to an either stack or heap allocated `nk_context` struct
 * \param[in] rect    | The rect area you're checking against
 * \param[in|out] timer | Must point to a float used to track the total seconds hovered across frames
 * \param[in] delay     | The wait time in seconds
 * \param[in|out] clicked | Must point to an nk_bool used to indicate whether the item in question has been clicked (reset to false internally on mouse motion)
 *
 * \returns `true` if the mouse has hovered without moving long enough, `false otherwise`
 */
NK_API nk_bool
nk_input_is_mouse_hovering_still_delay_clicked_rect(const struct nk_context *ctx, struct nk_rect rect, float* timer, float delay, nk_bool* clicked)
{
    NK_ASSERT(ctx);
    if (!ctx) {
        return nk_false;
    } else {
        const struct nk_input* i = &ctx->input;
        if (*clicked) {
            /* could also be based on maintaining hover rather than motionless once clicked */
            if (!nk_input_is_mouse_moved(i)) {
                return nk_false;
            }
            *clicked = nk_false;
            *timer = 0;
        }
        if (NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h)) {
            /* once it triggers, moving within the bounds should not make it disappear */
            if (*timer >= delay) {
                return nk_true;
            }
            if (!nk_input_is_mouse_moved(i)) {
                *timer += ctx->delta_time_seconds;
                return *timer >= delay;
            }
            *timer = 0;
        } else if (NK_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h)) {
            *timer = 0;
        }
        return nk_false;
    }
}
NK_API nk_bool
nk_input_is_mouse_prev_hovering_rect(const struct nk_input *i, struct nk_rect rect)
{
    if (!i) return nk_false;
    return NK_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h);
}
NK_API nk_bool
nk_input_mouse_clicked(const struct nk_input *i, enum nk_buttons id, struct nk_rect rect)
{
    if (!i) return nk_false;
    if (!nk_input_is_mouse_hovering_rect(i, rect)) return nk_false;
    return nk_input_is_mouse_click_in_rect(i, id, rect);
}
NK_API nk_bool
nk_input_is_mouse_down(const struct nk_input *i, enum nk_buttons id)
{
    if (!i) return nk_false;
    return i->mouse.buttons[id].down;
}
NK_API nk_bool
nk_input_is_mouse_pressed(const struct nk_input *i, enum nk_buttons id)
{
    const struct nk_mouse_button *b;
    if (!i) return nk_false;
    b = &i->mouse.buttons[id];
    if (b->down && b->clicked)
        return nk_true;
    return nk_false;
}
NK_API nk_bool
nk_input_is_mouse_released(const struct nk_input *i, enum nk_buttons id)
{
    if (!i) return nk_false;
    return (!i->mouse.buttons[id].down && i->mouse.buttons[id].clicked);
}
NK_API nk_bool
nk_input_is_mouse_moved(const struct nk_input *i)
{
    if (!i) return nk_false;
    return i->mouse.delta.x != 0 || i->mouse.delta.y != 0;
}
NK_API nk_bool
nk_input_is_key_pressed(const struct nk_input *i, enum nk_keys key)
{
    const struct nk_key *k;
    if (!i) return nk_false;
    k = &i->keyboard.keys[key];
    if ((k->down && k->clicked) || (!k->down && k->clicked >= 2))
        return nk_true;
    return nk_false;
}
NK_API nk_bool
nk_input_is_key_released(const struct nk_input *i, enum nk_keys key)
{
    const struct nk_key *k;
    if (!i) return nk_false;
    k = &i->keyboard.keys[key];
    if ((!k->down && k->clicked) || (k->down && k->clicked >= 2))
        return nk_true;
    return nk_false;
}
NK_API nk_bool
nk_input_is_key_down(const struct nk_input *i, enum nk_keys key)
{
    const struct nk_key *k;
    if (!i) return nk_false;
    k = &i->keyboard.keys[key];
    if (k->down) return nk_true;
    return nk_false;
}

/* ===============================================================
 *
 *                              IME
 *
 * ===============================================================*/
NK_LIB int
nk_ime_truncate_utf8(const char *text, int len, int max)
{
    int lead;
    int need;
    unsigned char c;

    /* clamp len to [0, max] without splitting a UTF-8 rune */
    if (!text || len <= 0 || max <= 0) return 0;
    len = NK_MIN(len, max);

    /* find the lead byte of the trailing rune */
    lead = len - 1;
    while (lead > 0 && ((unsigned char)text[lead] & 0xC0) == 0x80)
        lead--;

    /* expected rune length from the lead byte */
    c = (unsigned char)text[lead];
    if (c < 0x80) need = 1;
    else if ((c & 0xE0) == 0xC0) need = 2;
    else if ((c & 0xF0) == 0xE0) need = 3;
    else if ((c & 0xF8) == 0xF0) need = 4;
    else need = 1; /* invalid lead byte: treat as a single byte */

    /* drop the trailing rune if it is incomplete */
    if (lead + need > len)
        len = lead;
    return len;
}
NK_LIB void
nk_ime_clear_composition(struct nk_ime *ime)
{
    NK_ASSERT(ime);
    if (!ime) return;
    ime->composing = nk_false;
    ime->preedit_len = 0;
    ime->preedit_cursor = 0;
    ime->preedit_dirty = nk_false;
    ime->candidate_count = 0;
    ime->candidate_selected = -1;
    ime->cursor_rect = nk_rect(0,0,0,0);
}
NK_API void
nk_ime_enable(struct nk_context *ctx, nk_bool enable)
{
    NK_ASSERT(ctx);
    if (!ctx) return;
    ctx->ime.enabled = enable;
    if (!enable) {
        nk_ime_clear_composition(&ctx->ime);
        ctx->ime.commit_pending = nk_false;
        ctx->ime.commit_len = 0;
        ctx->ime.cancel_pending = nk_false;
    }
}
NK_API nk_bool
nk_ime_is_composing(const struct nk_context *ctx)
{
    NK_ASSERT(ctx);
    if (!ctx) return nk_false;
    return (nk_bool)(ctx->ime.enabled && ctx->ime.composing);
}
NK_API struct nk_rect
nk_ime_cursor_rect(const struct nk_context *ctx)
{
    NK_ASSERT(ctx);
    if (!ctx || !ctx->ime.enabled || !ctx->ime.composing)
        return nk_rect(0,0,0,0);
    return ctx->ime.cursor_rect;
}
NK_API void
nk_input_ime_preedit(struct nk_context *ctx, const char *text, int len, int cursor)
{
    struct nk_ime *ime;
    NK_ASSERT(ctx);
    if (!ctx) return;
    ime = &ctx->ime;
    if (!ime->enabled) return;

    ime->preedit_len = nk_ime_truncate_utf8(text, len, NK_IME_MAX_PREEDIT - 1);
    if (ime->preedit_len)
        NK_MEMCPY(ime->preedit, text, (nk_size)ime->preedit_len);
    ime->preedit[ime->preedit_len] = '\0';

    /* clamp the composition cursor to a rune boundary */
    ime->preedit_cursor = NK_CLAMP(0, cursor, ime->preedit_len);
    while (ime->preedit_cursor > 0 &&
        ((unsigned char)ime->preedit[ime->preedit_cursor] & 0xC0) == 0x80)
        ime->preedit_cursor--;

    ime->composing = (nk_bool)(ime->preedit_len > 0);
    ime->preedit_dirty = nk_true;
    if (!ime->composing) {
        ime->candidate_count = 0;
        ime->candidate_selected = -1;
        ime->cursor_rect = nk_rect(0,0,0,0);
    }
}
NK_API void
nk_input_ime_candidates(struct nk_context *ctx, const char **candidates,
    int count, int selected)
{
    int i;
    struct nk_ime *ime;
    NK_ASSERT(ctx);
    if (!ctx) return;
    ime = &ctx->ime;
    if (!ime->enabled) return;

    if (!candidates || count <= 0) {
        ime->candidate_count = 0;
        ime->candidate_selected = -1;
        return;
    }
    count = NK_MIN(count, NK_IME_MAX_CANDIDATES);
    for (i = 0; i < count; ++i) {
        int len;
        if (!candidates[i]) {
            ime->candidates[i].len = 0;
            ime->candidates[i].text[0] = '\0';
            continue;
        }
        len = nk_ime_truncate_utf8(candidates[i], nk_strlen(candidates[i]),
            NK_IME_MAX_CANDIDATE_LEN - 1);
        if (len)
            NK_MEMCPY(ime->candidates[i].text, candidates[i], (nk_size)len);
        ime->candidates[i].text[len] = '\0';
        ime->candidates[i].len = len;
    }
    ime->candidate_count = count;
    ime->candidate_selected = (selected >= 0 && selected < count) ? selected : -1;
}
NK_API void
nk_input_ime_select_candidate(struct nk_context *ctx, int index)
{
    struct nk_ime *ime;
    NK_ASSERT(ctx);
    if (!ctx) return;
    ime = &ctx->ime;
    if (!ime->enabled) return;
    if (index < 0 || index >= ime->candidate_count) return;
    if (!ime->candidates[index].len) return;
    nk_input_ime_commit(ctx, ime->candidates[index].text,
        ime->candidates[index].len);
}
NK_API void
nk_input_ime_commit(struct nk_context *ctx, const char *text, int len)
{
    struct nk_ime *ime;
    NK_ASSERT(ctx);
    if (!ctx) return;
    ime = &ctx->ime;
    if (!ime->enabled) return;

    ime->commit_len = nk_ime_truncate_utf8(text, len, NK_IME_MAX_COMMIT - 1);
    if (ime->commit_len)
        NK_MEMCPY(ime->commit, text, (nk_size)ime->commit_len);
    ime->commit[ime->commit_len] = '\0';
    ime->commit_pending = nk_true;
}
NK_API void
nk_input_ime_cancel(struct nk_context *ctx)
{
    NK_ASSERT(ctx);
    if (!ctx) return;
    if (!ctx->ime.enabled) return;
    ctx->ime.cancel_pending = nk_true;
}
