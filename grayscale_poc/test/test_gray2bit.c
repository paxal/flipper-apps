#include "gray2bit.h"
#include "image_flipper.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        printf("FAIL: %s\n", name);
        failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static void test_pack_roundtrip(void) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    for(uint8_t g = 0; g < 4; g++) {
        gray2bit_set(buf, 8, g, 0, g);
        expect_true("roundtrip level", gray2bit_get(buf, 8, g, 0) == g);
    }
    /* Four pixels in first byte: 00 01 10 11 → 0x1B */
    expect_true("packed nibble order", buf[0] == 0x1B);
}

static void test_duty_table(void) {
    expect_true("black duty 3", gray2bit_duty(0) == 3);
    expect_true("dark duty 2", gray2bit_duty(1) == 2);
    expect_true("light duty 1", gray2bit_duty(2) == 1);
    expect_true("white duty 0", gray2bit_duty(3) == 0);
}

static void test_frc_average_duty(void) {
    for(uint8_t level = 0; level < 4; level++) {
        uint8_t hits = 0;
        for(uint8_t phase = 0; phase < GRAY2BIT_FRC_PERIOD; phase++) {
            for(uint8_t spatial = 0; spatial < GRAY2BIT_FRC_PERIOD; spatial++) {
                if(gray2bit_frc_ink(level, phase, spatial)) {
                    hits++;
                }
            }
        }
        /* Over all spatial offsets, each phase slot is equally likely →
         * hits == duty * PERIOD */
        uint8_t expect = (uint8_t)(gray2bit_duty(level) * GRAY2BIT_FRC_PERIOD);
        char name[64];
        snprintf(name, sizeof(name), "frc duty level %u", level);
        expect_true(name, hits == expect);
    }
}

static void test_render_threshold_vs_frc(void) {
    uint8_t packed[4];
    memset(packed, 0, sizeof(packed));
    /* 8x1 strip: levels 0,0,1,1,2,2,3,3 */
    gray2bit_set(packed, 8, 0, 0, 0);
    gray2bit_set(packed, 8, 1, 0, 0);
    gray2bit_set(packed, 8, 2, 0, 1);
    gray2bit_set(packed, 8, 3, 0, 1);
    gray2bit_set(packed, 8, 4, 0, 2);
    gray2bit_set(packed, 8, 5, 0, 2);
    gray2bit_set(packed, 8, 6, 0, 3);
    gray2bit_set(packed, 8, 7, 0, 3);

    uint8_t xbm_th = 0;
    gray2bit_render_xbm(packed, 8, 1, 0, false, &xbm_th);
    /* threshold: levels < 2 → ink → bits 0..3 set */
    expect_true("threshold bits", xbm_th == 0x0F);

    uint8_t counts[8] = {0};
    for(uint8_t phase = 0; phase < GRAY2BIT_FRC_PERIOD; phase++) {
        uint8_t xbm = 0;
        gray2bit_render_xbm(packed, 8, 1, phase, true, &xbm);
        for(uint8_t bit = 0; bit < 8; bit++) {
            if(xbm & (1u << bit)) {
                counts[bit]++;
            }
        }
    }
    expect_true("frc black always", counts[0] == 3 && counts[1] == 3);
    expect_true("frc dark mid", counts[2] == 2 && counts[3] == 2);
    expect_true("frc light low", counts[4] == 1 && counts[5] == 1);
    expect_true("frc white never", counts[6] == 0 && counts[7] == 0);
}

static void test_image_has_all_levels(void) {
    bool seen[4] = {false, false, false, false};
    for(uint16_t y = 0; y < GRAY2BIT_SCREEN_H; y++) {
        for(uint16_t x = 0; x < GRAY2BIT_SCREEN_W; x++) {
            seen[gray2bit_get(image_flipper_2bit, GRAY2BIT_SCREEN_W, x, y) & 3] = true;
        }
    }
    expect_true("image has black", seen[0]);
    expect_true("image has dark", seen[1]);
    expect_true("image has light", seen[2]);
    expect_true("image has white", seen[3]);
}

int main(void) {
    test_pack_roundtrip();
    test_duty_table();
    test_frc_average_duty();
    test_render_threshold_vs_frc();
    test_image_has_all_levels();
    if(failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
