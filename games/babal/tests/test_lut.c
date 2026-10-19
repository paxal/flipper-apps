#include "render_math.h"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void expect_true(const char* name, int cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    BabalDepthLut lut;
    babal_lut_build(&lut, BABAL_Q16_FROM_FLOAT(0.0f));

    for(uint8_t i = 0; i < BABAL_MAX_DEPTH; i++) {
        expect_true("y_top <= y_bot", lut.rows[i].y_top <= lut.rows[i].y_bot);
        expect_true("half_w_near >= 0", lut.rows[i].half_w_near >= 0);
        expect_true("half_w_far >= 0", lut.rows[i].half_w_far >= 0);
    }
    /* Near row should be closer to bottom of screen (larger Y) than far rows. */
    expect_true("depth order", lut.rows[0].y_bot >= lut.rows[BABAL_MAX_DEPTH - 1].y_bot);

    BabalDepthLut lut2;
    babal_lut_build(&lut2, BABAL_Q16_FROM_FLOAT(1.0f));
    /* After +1 progress, geometry should remain in-bounds. */
    for(uint8_t i = 0; i < BABAL_MAX_DEPTH; i++) {
        for(int8_t lane = 0; lane < BABAL_LEVEL_WIDTH; lane++) {
            int16_t x0n, x1n, x0f, x1f;
            babal_lut_span_for_cell(&lut2, i, lane, &x0n, &x1n, &x0f, &x1f);
            expect_true("x0n bounds", x0n >= 0 && x0n < BABAL_SCREEN_W);
            expect_true("x1n bounds", x1n >= 0 && x1n < BABAL_SCREEN_W);
            expect_true("x0f bounds", x0f >= 0 && x0f < BABAL_SCREEN_W);
            expect_true("x1f bounds", x1f >= 0 && x1f < BABAL_SCREEN_W);
            expect_true("left<=right near", x0n <= x1n);
            expect_true("left<=right far", x0f <= x1f);
        }
    }

    /* Hot path uses LUT only: calling span after build must not need rebuild. */
    int16_t a, b, c, d;
    babal_lut_span_for_cell(&lut, 0, 3, &a, &b, &c, &d);
    expect_true("span center non-empty", b >= a);

    if(failures) {
        fprintf(stderr, "%d lut failures\n", failures);
        return 1;
    }
    puts("T2 lut OK");
    return 0;
}
