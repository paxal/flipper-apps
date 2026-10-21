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
    expect_true("packed nibble order", buf[0] == 0x1B);
}

static void test_duty_table(void) {
    expect_true("black duty 3", gray2bit_duty(0) == 3);
    expect_true("dark duty 2", gray2bit_duty(1) == 2);
    expect_true("light duty 1", gray2bit_duty(2) == 1);
    expect_true("white duty 0", gray2bit_duty(3) == 0);
}

static void test_ti_plane_truth_table(void) {
    bool d = false;
    bool l = false;

    gray2bit_ti_plane_bits(GrayLevelBlack, &d, &l);
    expect_true("TI black both planes", d && l);

    gray2bit_ti_plane_bits(GrayLevelDark, &d, &l);
    expect_true("TI dark only dark plane", d && !l);

    gray2bit_ti_plane_bits(GrayLevelLight, &d, &l);
    expect_true("TI light only light plane", !d && l);

    gray2bit_ti_plane_bits(GrayLevelWhite, &d, &l);
    expect_true("TI white neither plane", !d && !l);
}

static void test_ti_schedule_2_to_1(void) {
    expect_true("phase0 dark plane", gray2bit_ti_plane_index(0) == 0);
    expect_true("phase1 dark plane", gray2bit_ti_plane_index(1) == 0);
    expect_true("phase2 light plane", gray2bit_ti_plane_index(2) == 1);
}

static void test_ti_plane_duties(void) {
    /* Over a 3-tick TI schedule, plane membership yields classic duties. */
    for(uint8_t level = 0; level < 4; level++) {
        bool dark = false;
        bool light = false;
        gray2bit_ti_plane_bits(level, &dark, &light);
        uint8_t hits = 0;
        for(uint8_t phase = 0; phase < GRAY2BIT_FRC_PERIOD; phase++) {
            bool ink = (gray2bit_ti_plane_index(phase) == 0) ? dark : light;
            if(ink) {
                hits++;
            }
        }
        char name[64];
        snprintf(name, sizeof(name), "TI duty level %u", level);
        expect_true(name, hits == gray2bit_duty(level));
    }
}

static void test_stagger_average_duty(void) {
    for(uint8_t level = 0; level < 4; level++) {
        uint8_t hits = 0;
        for(uint8_t phase = 0; phase < GRAY2BIT_FRC_PERIOD; phase++) {
            for(uint8_t spatial = 0; spatial < GRAY2BIT_FRC_PERIOD; spatial++) {
                if(gray2bit_stagger_ink(level, phase, spatial)) {
                    hits++;
                }
            }
        }
        uint8_t expect = (uint8_t)(gray2bit_duty(level) * GRAY2BIT_FRC_PERIOD);
        char name[64];
        snprintf(name, sizeof(name), "stagger duty level %u", level);
        expect_true(name, hits == expect);
    }
}

static void test_bayer_counts(void) {
    uint8_t counts[4] = {0};
    for(uint8_t level = 0; level < 4; level++) {
        for(uint16_t y = 0; y < 2; y++) {
            for(uint16_t x = 0; x < 2; x++) {
                if(gray2bit_bayer_ink(level, x, y)) {
                    counts[level]++;
                }
            }
        }
    }
    expect_true("bayer black 4/4", counts[0] == 4);
    expect_true("bayer dark 2/4", counts[1] == 2);
    expect_true("bayer light 1/4", counts[2] == 1);
    expect_true("bayer white 0/4", counts[3] == 0);
}

static void test_render_modes(void) {
    uint8_t packed[4];
    memset(packed, 0, sizeof(packed));
    gray2bit_set(packed, 8, 0, 0, 0);
    gray2bit_set(packed, 8, 1, 0, 0);
    gray2bit_set(packed, 8, 2, 0, 1);
    gray2bit_set(packed, 8, 3, 0, 1);
    gray2bit_set(packed, 8, 4, 0, 2);
    gray2bit_set(packed, 8, 5, 0, 2);
    gray2bit_set(packed, 8, 6, 0, 3);
    gray2bit_set(packed, 8, 7, 0, 3);

    uint8_t dark[1] = {0};
    uint8_t light[1] = {0};
    gray2bit_pack_to_ti_planes(packed, 8, 1, dark, light);
    /* black+dark → bits 0..3 in dark plane; black+light → bits 0,1,4,5 in light */
    expect_true("TI dark plane bits", dark[0] == 0x0F);
    expect_true("TI light plane bits", light[0] == 0x33);

    uint8_t xbm_th = 0;
    gray2bit_render_xbm(packed, 8, 1, GrayModeThreshold, 0, NULL, NULL, &xbm_th);
    expect_true("threshold bits", xbm_th == 0x0F);

    uint8_t counts[8] = {0};
    for(uint8_t phase = 0; phase < GRAY2BIT_FRC_PERIOD; phase++) {
        uint8_t xbm = 0;
        gray2bit_render_xbm(packed, 8, 1, GrayModeTiPlanes, phase, dark, light, &xbm);
        for(uint8_t bit = 0; bit < 8; bit++) {
            if(xbm & (1u << bit)) {
                counts[bit]++;
            }
        }
    }
    expect_true("TI render black", counts[0] == 3 && counts[1] == 3);
    expect_true("TI render dark", counts[2] == 2 && counts[3] == 2);
    expect_true("TI render light", counts[4] == 1 && counts[5] == 1);
    expect_true("TI render white", counts[6] == 0 && counts[7] == 0);
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

static void test_mode_helpers(void) {
    expect_true("TI needs FRC", gray2bit_mode_needs_frc(GrayModeTiPlanes));
    expect_true("Bayer static", !gray2bit_mode_needs_frc(GrayModeBayer));
    expect_true("name TI", strcmp(gray2bit_mode_name(GrayModeTiPlanes), "TI planes") == 0);
}

int main(void) {
    test_pack_roundtrip();
    test_duty_table();
    test_ti_plane_truth_table();
    test_ti_schedule_2_to_1();
    test_ti_plane_duties();
    test_stagger_average_duty();
    test_bayer_counts();
    test_render_modes();
    test_image_has_all_levels();
    test_mode_helpers();
    if(failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
