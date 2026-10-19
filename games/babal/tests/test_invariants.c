#include "render_math.h"
#include "level.h"
#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, int cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static uint32_t xorshift(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

int main(void) {
    /* Drawn cells ≤ 8*8 */
    BabalGame g;
    babal_game_init(&g);
    babal_game_start(&g, 0);
    babal_game_render_soft(&g);
    expect_true("cells <= 64", g.cells_drawn <= BABAL_LEVEL_WIDTH * BABAL_MAX_DEPTH);

    /* Jump does not change LUT geometry of track — compare frames without ball. */
    BabalSoftFb a, b;
    BabalLevelView view = {.rows = g.level->rows, .length = g.level->length};
    babal_render_frame(&a, &view, BABAL_Q16_FROM_FLOAT(g.progress), 0, 0, false, false);
    babal_render_frame(&b, &view, BABAL_Q16_FROM_FLOAT(g.progress), 0, 10, false, false);
    expect_true("jump height no track change", babal_fb_equal(&a, &b));

    /* Hole cells never set platform bits: empty row renders blank (no ball). */
    static const char* const empty_rows[] = {
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
        "        ",
    };
    BabalLevelView empty = {.rows = empty_rows, .length = 16};
    BabalSoftFb blank;
    uint16_t drawn = babal_render_frame(&blank, &empty, 0, 0, 0, false, false);
    expect_true("empty drawn 0", drawn == 0);
    int any = 0;
    for(int i = 0; i < BABAL_FB_BYTES; i++) {
        if(blank.pixels[i]) {
            any = 1;
            break;
        }
    }
    expect_true("empty fb clear", !any);

    /* Level parse custom format */
    const char* text = "NAME Unit\n********\n***  ***\n********\n";
    char name[32];
    char storage[256];
    const char* rows[16];
    uint16_t n = 0;
    expect_true(
        "parse custom",
        babal_level_parse_text(text, name, sizeof(name), storage, sizeof(storage), rows, 16, &n));
    expect_true("parse name", strcmp(name, "Unit") == 0);
    expect_true("parse rows", n == 3);

    /* Seeded fuzz: random strips stay in-bounds / no crash */
    uint32_t seed = 0xC0FFEEu;
    char rowbuf[32][9];
    const char* fuzz_rows[32];
    for(int trial = 0; trial < 50; trial++) {
        uint16_t len = 16;
        for(uint16_t r = 0; r < len; r++) {
            for(int c = 0; c < 8; c++) {
                rowbuf[r][c] = (xorshift(&seed) & 1) ? '*' : ' ';
            }
            rowbuf[r][8] = '\0';
            fuzz_rows[r] = rowbuf[r];
        }
        BabalLevelView fuzz = {.rows = fuzz_rows, .length = len};
        BabalSoftFb fb;
        uint16_t d = babal_render_frame(
            &fb,
            &fuzz,
            BABAL_Q16_FROM_FLOAT((float)(seed % 5)),
            BABAL_Q16_FROM_FLOAT(0.0f),
            0,
            true,
            false);
        expect_true("fuzz cells", d <= BABAL_LEVEL_WIDTH * BABAL_MAX_DEPTH);
    }

    /* Game tick rewind on hole */
    babal_game_start(&g, 1); /* holes level */
    g.progress = 1.0f;
    g.lane = 3.5f;
    g.height = 0.0f;
    /* Force a known hole if present */
    float before = g.progress;
    /* Step until we fall or progress a bit */
    for(int i = 0; i < 200; i++) {
        babal_game_tick(&g, 40);
        if(g.progress + 0.01f < before && g.crash_flash) {
            break;
        }
        before = g.progress;
    }

    if(failures) {
        fprintf(stderr, "%d invariant failures\n", failures);
        return 1;
    }
    puts("T4 invariants OK");
    return 0;
}
