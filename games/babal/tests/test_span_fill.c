#include "render_math.h"
#include "level.h"
#include "game.h"

#include <math.h>
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

static const char* const k_solid_rows[] = {
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
    "********",
};

static const char* const k_holes_rows[] = {
    "********",
    "* * * * ",
    "********",
    " * * * *",
    "********",
    "* * * * ",
    "********",
    " * * * *",
    "********",
    "* * * * ",
    "********",
    " * * * *",
    "********",
    "* * * * ",
    "********",
    "********",
};

static const char* const k_empty_rows[] = {
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

static void check_or_record(const char* name, const BabalSoftFb* fb) {
    char path[256];
    snprintf(path, sizeof(path), "goldens/%s.pbm", name);
    const char* rec = getenv("RECORD_GOLDEN");
    if(rec && rec[0] == '1') {
        expect_true("write golden", babal_fb_write_pbm_file(fb, path));
        printf("recorded %s\n", path);
        return;
    }
    BabalSoftFb golden;
    if(!babal_fb_load_pbm_file(&golden, path)) {
        fprintf(
            stderr, "FAIL: missing golden %s (run RECORD_GOLDEN=1 make record_golden)\n", path);
        failures++;
        return;
    }
    expect_true(name, babal_fb_equal(fb, &golden));
}

int main(void) {
    BabalSoftFb fb;
    BabalLevelView solid = {.rows = k_solid_rows, .length = 16};
    BabalLevelView holes = {.rows = k_holes_rows, .length = 16};
    BabalLevelView empty = {.rows = k_empty_rows, .length = 16};

    babal_render_frame(&fb, &solid, BABAL_Q16_FROM_FLOAT(0.0f), 0, 0, true, false);
    check_or_record("solid_p0", &fb);

    babal_render_frame(&fb, &solid, BABAL_Q16_FROM_FLOAT(0.5f), 0, 0, true, false);
    check_or_record("solid_p05", &fb);

    babal_render_frame(&fb, &solid, BABAL_Q16_FROM_FLOAT(1.0f), 0, 0, true, false);
    check_or_record("solid_p1", &fb);

    babal_render_frame(&fb, &holes, BABAL_Q16_FROM_FLOAT(0.0f), 0, 0, true, false);
    check_or_record("holes_p0", &fb);

    /* Ball only: empty track still draws checkerboard dither disc. */
    babal_render_frame(&fb, &empty, BABAL_Q16_FROM_FLOAT(0.0f), 0, 0, true, false);
    check_or_record("ball_only", &fb);
    int16_t cy = babal_adjust_y((int16_t)BABAL_VIEWPORT_Y_BALL);
    int set_n = 0, clear_n = 0;
    for(int16_t dy = -BABAL_BALL_R; dy <= BABAL_BALL_R; dy++) {
        for(int16_t dx = -BABAL_BALL_R; dx <= BABAL_BALL_R; dx++) {
            if(dx * dx + dy * dy > BABAL_BALL_R * BABAL_BALL_R) {
                continue;
            }
            if(babal_fb_get(&fb, (int16_t)(BABAL_VIEWPORT_X_CENTER + dx), (int16_t)(cy + dy))) {
                set_n++;
            } else {
                clear_n++;
            }
        }
    }
    expect_true("ball dither has ink", set_n > 8);
    expect_true("ball dither has voids", clear_n > 8);

    /* Holes leave some cleared columns vs solid. */
    BabalSoftFb solid_fb, hole_fb;
    babal_render_frame(&solid_fb, &solid, 0, 0, 0, false, false);
    babal_render_frame(&hole_fb, &holes, 0, 0, 0, false, false);
    expect_true("holes differ from solid", !babal_fb_equal(&solid_fb, &hole_fb));

    /* Thales edges are straight: silhouette within ±1px of ideal half-width. */
    babal_render_frame(&solid_fb, &solid, BABAL_Q16_FROM_FLOAT(0.5f), 0, 0, false, false);
    int edge_bad = 0;
    for(int16_t y = 0; y < BABAL_SCREEN_H; y++) {
        int16_t L = -1, R = -1;
        for(int16_t x = 0; x < BABAL_SCREEN_W; x++) {
            if(babal_fb_get(&solid_fb, x, y)) {
                if(L < 0) {
                    L = x;
                }
                R = x;
            }
        }
        if(L < 0) {
            continue;
        }
        float y_up = (float)(BABAL_SCREEN_H - 1 - y);
        float half = fabsf(babal_level_to_viewport_x_f(4.0f, y_up));
        int16_t iL = (int16_t)lroundf((float)BABAL_VIEWPORT_X_CENTER - half);
        int16_t iR = (int16_t)lroundf((float)BABAL_VIEWPORT_X_CENTER + half);
        if(iL < 0) {
            iL = 0;
        }
        if(iR > BABAL_SCREEN_W - 1) {
            iR = BABAL_SCREEN_W - 1;
        }
        if(abs((int)L - (int)iL) > 1 || abs((int)R - (int)iR) > 1) {
            edge_bad++;
        }
    }
    expect_true("thales edges straight", edge_bad <= 2);

    if(failures) {
        fprintf(stderr, "%d span_fill failures\n", failures);
        return 1;
    }
    puts("T3 span_fill OK");
    return 0;
}
