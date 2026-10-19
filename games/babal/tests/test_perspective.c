#include "render_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void expect_true(const char* name, int cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void expect_near_int(const char* name, int got, int want, int tol) {
    if(abs(got - want) > tol) {
        fprintf(stderr, "FAIL: %s got=%d want=%d tol=%d\n", name, got, want, tol);
        failures++;
    }
}

int main(void) {
    /* Anchors (float oracle, screen-up coords). */
    expect_near_int(
        "y0 near", (int)lroundf(babal_level_to_viewport_y_f(0.0f)), (int)BABAL_VIEWPORT_Y_0, 1);
    expect_near_int(
        "yBall",
        (int)lroundf(babal_level_to_viewport_y_f(BABAL_LEVEL_Y_BALL)),
        (int)BABAL_VIEWPORT_Y_BALL,
        1);
    expect_near_int(
        "yMax horizon",
        (int)lroundf(babal_level_to_viewport_y_f(1000.0f)),
        (int)BABAL_VIEWPORT_Y_MAX,
        1);

    /* Monotonic toward horizon. */
    float prev = babal_level_to_viewport_y_f(0.0f);
    for(int i = 1; i < 40; i++) {
        float y = babal_level_to_viewport_y_f((float)i * 0.5f);
        expect_true("mono y", y + 1e-4f >= prev);
        prev = y;
    }

    /* X symmetry. */
    float yv = babal_level_to_viewport_y_f(2.0f);
    float xp = babal_level_to_viewport_x_f(2.0f, yv);
    float xn = babal_level_to_viewport_x_f(-2.0f, yv);
    expect_near_int("x symmetry", (int)lroundf(xp + xn), 0, 1);

    /* Q16 vs float oracle (±1 px). */
    for(int i = 0; i < 20; i++) {
        float ylf = (float)i * 0.35f;
        babal_q16_t ylq = BABAL_Q16_FROM_FLOAT(ylf);
        int fy = (int)lroundf(babal_level_to_viewport_y_f(ylf));
        int qy = BABAL_Q16_TO_INT(babal_level_to_viewport_y_q16(ylq));
        expect_near_int("q16 y", qy, fy, 1);

        float yv2 = babal_level_to_viewport_y_f(ylf);
        float xf = babal_level_to_viewport_x_f(1.5f, yv2);
        babal_q16_t xq =
            babal_level_to_viewport_x_q16(BABAL_Q16_FROM_FLOAT(1.5f), BABAL_Q16_FROM_FLOAT(yv2));
        expect_near_int("q16 x", BABAL_Q16_TO_INT(xq), (int)lroundf(xf), 1);
    }

    if(failures) {
        fprintf(stderr, "%d perspective failures\n", failures);
        return 1;
    }
    puts("T1 perspective OK");
    return 0;
}
