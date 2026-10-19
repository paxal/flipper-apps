/* Host-side unit tests for CSC Measurement parsing (no Flipper SDK).
 * Build: cc -I csc_debug/helpers -o /tmp/test_csc_parse \
 *          tests/test_csc_parse.c csc_debug/helpers/csc_parse.c -lm && /tmp/test_csc_parse
 */

#include "csc_parse.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static void expect_float_near(const char* name, float got, float want, float eps) {
    if(fabsf(got - want) > eps) {
        fprintf(stderr, "FAIL: %s (got %f want %f)\n", name, (double)got, (double)want);
        failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

int main(void) {
    /* Wheel + crank measurement */
    const uint8_t both[] = {
        0x03, /* flags: wheel + crank */
        0x10,
        0x00,
        0x00,
        0x00, /* wheel revs = 16 */
        0x00,
        0x04, /* wheel time = 1024 ticks = 1.0 s */
        0x20,
        0x00, /* crank revs = 32 */
        0x00,
        0x02, /* crank time = 512 ticks = 0.5 s */
    };
    CscMeasurement m;
    expect_true("parse both", csc_measurement_parse(both, sizeof(both), &m));
    expect_true("wheel present", m.wheel_present);
    expect_true("crank present", m.crank_present);
    expect_true("wheel revs", m.cumulative_wheel_revs == 16);
    expect_true("wheel time", m.last_wheel_event_time == 1024);
    expect_true("crank revs", m.cumulative_crank_revs == 32);
    expect_true("crank time", m.last_crank_event_time == 512);

    /* Truncated payload */
    expect_true("reject short", !csc_measurement_parse(both, 3, &m));

    /* Tracker speed/cadence */
    CscTracker tracker;
    csc_tracker_reset(&tracker, 2100); /* 2.1 m circumference */
    CscDerived d;
    CscMeasurement a = {
        .flags = 0x03,
        .wheel_present = true,
        .crank_present = true,
        .cumulative_wheel_revs = 100,
        .last_wheel_event_time = 0,
        .cumulative_crank_revs = 10,
        .last_crank_event_time = 0,
    };
    csc_tracker_update(&tracker, &a, &d);
    expect_true("first sample invalid", !d.valid);

    CscMeasurement b = a;
    b.cumulative_wheel_revs = 110; /* +10 revs */
    b.last_wheel_event_time = 1024; /* +1.0 s */
    b.cumulative_crank_revs = 12; /* +2 revs */
    b.last_crank_event_time = 1024; /* +1.0 s */
    csc_tracker_update(&tracker, &b, &d);
    expect_true("second sample valid", d.valid);
    /* distance = 10 * 2.1 m = 21 m in 1 s => 21 m/s => 75.6 km/h */
    expect_float_near("speed", d.speed_kmh, 75.6f, 0.2f);
    /* cadence = 2 rev / 1 s * 60 = 120 rpm */
    expect_float_near("cadence", d.cadence_rpm, 120.0f, 0.1f);

    /* Advertising UUID detection */
    const uint8_t adv[] = {
        0x02,
        0x01,
        0x06, /* flags */
        0x03,
        0x03,
        0x16,
        0x18, /* complete 16-bit UUID list: 0x1816 */
        0x05,
        0x09,
        'T',
        'e',
        's',
        't',
    };
    expect_true("adv has CSC", csc_adv_has_service_uuid(adv, sizeof(adv), CSC_SERVICE_UUID));
    expect_true("adv no HR", !csc_adv_has_service_uuid(adv, sizeof(adv), 0x180D));

    if(failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
