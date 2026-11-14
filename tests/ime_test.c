/*
 * IME preedit/composition tests for nuklear edit widgets.
 * Headless: drives nk_input_ime_* events through nk_edit_string frames.
 */
#include <stdio.h>
#include <string.h>

#define NK_IMPLEMENTATION
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "../nuklear.h"

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { \
        g_failures++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* CJK UTF-8 literals */
#define S_NIHON      "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93"       /* nihon */
#define S_NIHONGO    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"       /* 日本語 */
#define S_NIHON_     "\xE6\x97\xA5\xE6\x9C\xAC"                   /* 日本 */
#define S_KANA       "\xE3\x81\x8B\xE3\x81\xAA"                   /* かな */
#define S_KANA_KANJI "\xE4\xBB\xAE\xE5\x90\x8D"                   /* 仮名 */
#define S_KIN        "\xE9\x87\x91"                               /* 金 */
#define S_KANA_KATA  "\xE3\x82\xAB\xE3\x83\x8A"                   /* カナ */
#define S_TESUTO     "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"       /* てすと */
#define S_CHUUBUN    "\xE4\xB8\xAD\xE6\x96\x87"                   /* 中文 */
#define S_NICHI      "\xE6\x97\xA5"                               /* 日 */
#define S_A_HIRA     "\xE3\x81\x82"                               /* あ */
#define S_I_HIRA     "\xE3\x81\x84"                               /* い */
#define S_U_HIRA     "\xE3\x81\x86"                               /* う */

static float
font_width(nk_handle handle, float height, const char *text, int len)
{
    NK_UNUSED(handle);
    NK_UNUSED(height);
    return (float)(8 * nk_utf_len(text, len));
}

static struct nk_context ctx;
static unsigned char arena[128 * 1024];
static char g_buf[256];
static int g_len;
static int g_max;
static int g_focus;
static int g_unfocus;

static void
setup(int max)
{
    static struct nk_user_font font;
    memset(&ctx, 0, sizeof(ctx));
    memset(arena, 0, sizeof(arena));
    font.userdata = nk_handle_ptr(0);
    font.height = 16.0f;
    font.width = font_width;
    nk_init_fixed(&ctx, arena, sizeof(arena), &font);
    nk_ime_enable(&ctx, nk_true);
    memset(g_buf, 0, sizeof(g_buf));
    g_len = 0;
    g_max = max;
    g_focus = 1;
    g_unfocus = 0;
}

static nk_flags
frame(void)
{
    nk_flags st = 0;
    nk_input_end(&ctx);
    if (nk_begin(&ctx, "win", nk_rect(0, 0, 480, 320), 0)) {
        nk_layout_row_dynamic(&ctx, 30, 1);
        if (g_focus) {
            nk_edit_focus(&ctx, NK_EDIT_ALWAYS_INSERT_MODE);
            g_focus = 0;
        }
        if (g_unfocus) {
            nk_edit_unfocus(&ctx);
            g_unfocus = 0;
        }
        st = nk_edit_string(&ctx, NK_EDIT_FIELD, g_buf, &g_len, g_max,
            nk_filter_default);
    }
    nk_end(&ctx);
    nk_clear(&ctx);
    return st;
}

static void
test_cjk_composition(void)
{
    nk_flags st;
    printf("test_cjk_composition\n");
    setup(64);

    /* activate the edit widget */
    nk_input_begin(&ctx);
    st = frame();
    CHECK(st & NK_EDIT_ACTIVE);

    /* composition string must not touch the committed text */
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NIHON, 9, 9);
    frame();
    CHECK(nk_ime_is_composing(&ctx));
    CHECK(ctx.ime.preedit_len == 9);
    CHECK(ctx.ime.preedit_cursor == 9);
    CHECK(g_len == 0);
    CHECK(nk_ime_cursor_rect(&ctx).w > 0.0f);

    /* composition cursor moves inside the preedit string */
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NIHON, 9, 3);
    frame();
    CHECK(ctx.ime.preedit_cursor == 3);
    CHECK(g_len == 0);

    /* commit replaces the composition and updates caret + undo */
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, S_NIHONGO, 9);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 9);
    CHECK(memcmp(g_buf, S_NIHONGO, 9) == 0);
    CHECK(nk_ime_cursor_rect(&ctx).w == 0.0f);
    CHECK(ctx.begin->edit.cursor == 3);
    CHECK(ctx.text_edit.undo.undo_point == 3);

    /* undo restores the text from before the commit */
    nk_textedit_undo(&ctx.text_edit);
    CHECK((int)ctx.text_edit.string.buffer.allocated == 6);
    CHECK(memcmp(g_buf, S_NIHON_, 6) == 0);
}

static void
test_candidate_selection(void)
{
    const char *cands[] = {S_KANA, S_KIN, S_KANA_KANJI, S_KANA_KATA};
    printf("test_candidate_selection\n");
    setup(64);
    nk_input_begin(&ctx);
    frame();

    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_KANA, 6, 6);
    nk_input_ime_candidates(&ctx, cands, 4, 2);
    frame();
    CHECK(ctx.ime.candidate_count == 4);
    CHECK(ctx.ime.candidate_selected == 2);
    CHECK(strcmp(ctx.ime.candidates[2].text, S_KANA_KANJI) == 0);

    /* out of range candidate selection is ignored */
    nk_input_begin(&ctx);
    nk_input_ime_select_candidate(&ctx, 17);
    frame();
    CHECK(g_len == 0);
    CHECK(nk_ime_is_composing(&ctx));

    /* selecting a candidate commits its text */
    nk_input_begin(&ctx);
    nk_input_ime_select_candidate(&ctx, 2);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 6);
    CHECK(memcmp(g_buf, S_KANA_KANJI, 6) == 0);
    CHECK(ctx.ime.candidate_count == 0);
}

static void
test_cancel(void)
{
    printf("test_cancel\n");
    setup(64);
    memcpy(g_buf, "abc", 3);
    g_len = 3;
    nk_input_begin(&ctx);
    frame();

    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_TESUTO, 9, 9);
    frame();
    CHECK(nk_ime_is_composing(&ctx));

    /* cancel drops the composition and keeps the text + undo untouched */
    nk_input_begin(&ctx);
    nk_input_ime_cancel(&ctx);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 3);
    CHECK(memcmp(g_buf, "abc", 3) == 0);
    CHECK(ctx.text_edit.undo.undo_point == 0);
}

static void
test_focus_switch(void)
{
    printf("test_focus_switch\n");
    setup(64);
    nk_input_begin(&ctx);
    frame();

    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_CHUUBUN, 6, 6);
    frame();
    CHECK(nk_ime_is_composing(&ctx));

    /* clicking outside the edit cancels the composition */
    nk_input_begin(&ctx);
    nk_input_motion(&ctx, 10, 200);
    nk_input_button(&ctx, NK_BUTTON_LEFT, 10, 200, nk_true);
    frame();
    nk_input_begin(&ctx);
    nk_input_button(&ctx, NK_BUTTON_LEFT, 10, 200, nk_false);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 0);

    /* programmatic unfocus also cancels the composition */
    g_focus = 1;
    nk_input_begin(&ctx);
    frame();
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_CHUUBUN, 6, 6);
    frame();
    CHECK(nk_ime_is_composing(&ctx));
    nk_input_begin(&ctx);
    g_unfocus = 1;
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 0);
}

static void
test_overlong_input(void)
{
    char big[300];
    const char *many[12];
    char longcand[80];
    char mb[65];
    int i;
    printf("test_overlong_input\n");
    setup(256);
    nk_input_begin(&ctx);
    frame();

    /* preedit is truncated to the fixed buffer size */
    memset(big, 'a', sizeof(big));
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, big, 200, 200);
    frame();
    CHECK(ctx.ime.preedit_len == NK_IME_MAX_PREEDIT - 1);
    CHECK(ctx.ime.preedit_cursor == NK_IME_MAX_PREEDIT - 1);

    /* multibyte truncation never splits a rune (32x 2-byte rune = 64) */
    for (i = 0; i < 32; ++i) {
        mb[2 * i] = '\xC3';
        mb[2 * i + 1] = '\xA9';
    }
    mb[64] = 0;
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, mb, 64, 64);
    frame();
    CHECK(ctx.ime.preedit_len == 62);
    CHECK(ctx.ime.preedit_cursor == 62);

    /* composition cursor is clamped to a rune boundary */
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NIHON_, 6, 4);
    frame();
    CHECK(ctx.ime.preedit_cursor == 3);

    /* candidate list and candidate text are bounded */
    for (i = 0; i < 12; ++i)
        many[i] = "x";
    memset(longcand, 'b', sizeof(longcand) - 1);
    longcand[sizeof(longcand) - 1] = 0;
    many[3] = longcand;
    nk_input_begin(&ctx);
    nk_input_ime_candidates(&ctx, many, 12, 10);
    frame();
    CHECK(ctx.ime.candidate_count == NK_IME_MAX_CANDIDATES);
    CHECK(ctx.ime.candidates[3].len == NK_IME_MAX_CANDIDATE_LEN - 1);
    CHECK(ctx.ime.candidate_selected == -1);

    /* overlong commit is truncated at a rune boundary and committed */
    memset(big, 'c', sizeof(big));
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, big, 200);
    frame();
    CHECK(g_len == NK_IME_MAX_COMMIT - 1);
    CHECK(!nk_ime_is_composing(&ctx));
}

static void
test_commit_buffer_full(void)
{
    printf("test_commit_buffer_full\n");
    setup(8);
    memcpy(g_buf, "abcdefg", 7);
    g_len = 7;
    nk_input_begin(&ctx);
    frame();

    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NICHI, 3, 3);
    frame();
    CHECK(nk_ime_is_composing(&ctx));

    /* commit does not fit: composition cleared, original text kept */
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, S_NIHON_, 6);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 7);
    CHECK(memcmp(g_buf, "abcdefg", 7) == 0);
}

static void
test_empty_and_repeated_commit(void)
{
    printf("test_empty_and_repeated_commit\n");
    setup(64);
    nk_input_begin(&ctx);
    frame();

    /* empty preedit clears the composition */
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NICHI, 3, 3);
    frame();
    CHECK(nk_ime_is_composing(&ctx));
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, "", 0, 0);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(g_len == 0);

    /* empty commit is a no-op */
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, "", 0);
    frame();
    CHECK(g_len == 0);

    /* repeated commit in one frame: latest wins, inserted once */
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, S_A_HIRA, 3);
    nk_input_ime_commit(&ctx, S_I_HIRA, 3);
    frame();
    CHECK(g_len == 3);
    CHECK(memcmp(g_buf, S_I_HIRA, 3) == 0);

    /* commits in consecutive frames both apply */
    nk_input_begin(&ctx);
    nk_input_ime_commit(&ctx, S_U_HIRA, 3);
    frame();
    CHECK(g_len == 6);
    CHECK(memcmp(g_buf, S_I_HIRA S_U_HIRA, 6) == 0);
}

static void
test_ime_disabled(void)
{
    printf("test_ime_disabled\n");
    setup(64);
    nk_ime_enable(&ctx, nk_false);
    nk_input_begin(&ctx);
    frame();

    /* IME events are ignored while disabled */
    nk_input_begin(&ctx);
    nk_input_ime_preedit(&ctx, S_NICHI, 3, 3);
    nk_input_ime_commit(&ctx, S_NICHI, 3);
    frame();
    CHECK(!nk_ime_is_composing(&ctx));
    CHECK(ctx.ime.preedit_len == 0);
    CHECK(ctx.ime.commit_pending == nk_false);
    CHECK(g_len == 0);

    /* regular keyboard input is unaffected */
    nk_input_begin(&ctx);
    nk_input_char(&ctx, 'a');
    frame();
    CHECK(g_len == 1);
    CHECK(g_buf[0] == 'a');
}

static void
scenario_type_abc(void)
{
    nk_input_begin(&ctx);
    frame();
    nk_input_begin(&ctx);
    nk_input_char(&ctx, 'a');
    frame();
    nk_input_begin(&ctx);
    nk_input_char(&ctx, 'b');
    nk_input_key(&ctx, NK_KEY_LEFT, nk_true);
    frame();
    nk_input_begin(&ctx);
    nk_input_key(&ctx, NK_KEY_LEFT, nk_false);
    nk_input_char(&ctx, 'c');
    frame();
}

static void
test_byte_identical_output(void)
{
    static unsigned char reference[128 * 1024];
    printf("test_byte_identical_output\n");

    /* reference run: IME disabled */
    setup(64);
    nk_ime_enable(&ctx, nk_false);
    scenario_type_abc();
    memcpy(reference, arena, sizeof(arena));
    CHECK(g_len == 3);
    CHECK(memcmp(g_buf, "bca", 3) == 0);

    /* IME enabled but unused: frame output must be byte identical */
    setup(64);
    scenario_type_abc();
    CHECK(memcmp(reference, arena, sizeof(arena)) == 0);
    CHECK(g_len == 3);
    CHECK(memcmp(g_buf, "bca", 3) == 0);
}

int
main(void)
{
    test_cjk_composition();
    test_candidate_selection();
    test_cancel();
    test_focus_switch();
    test_overlong_input();
    test_commit_buffer_full();
    test_empty_and_repeated_commit();
    test_ime_disabled();
    test_byte_identical_output();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
