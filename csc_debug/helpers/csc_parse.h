#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Bluetooth SIG Cycling Speed and Cadence Service UUID */
#define CSC_SERVICE_UUID 0x1816
/** CSC Measurement characteristic */
#define CSC_MEASUREMENT_UUID 0x2A5B
/** CSC Feature characteristic */
#define CSC_FEATURE_UUID 0x2A5C
/** Sensor Location characteristic */
#define CSC_SENSOR_LOCATION_UUID 0x2A5D
/** SC Control Point characteristic */
#define CSC_CONTROL_POINT_UUID 0x2A55

#define CSC_FLAG_WHEEL_PRESENT (1u << 0)
#define CSC_FLAG_CRANK_PRESENT (1u << 1)

/** Default 700x25C circumference in millimetres */
#define CSC_DEFAULT_WHEEL_CIRCUMFERENCE_MM 2105u

typedef struct {
    uint8_t flags;
    bool wheel_present;
    bool crank_present;
    uint32_t cumulative_wheel_revs;
    uint16_t last_wheel_event_time; /**< units of 1/1024 s */
    uint16_t cumulative_crank_revs;
    uint16_t last_crank_event_time; /**< units of 1/1024 s */
} CscMeasurement;

typedef struct {
    bool valid;
    float speed_kmh;
    float cadence_rpm;
    uint32_t wheel_revs;
    uint16_t crank_revs;
    uint32_t delta_wheel_revs;
    uint16_t delta_crank_revs;
    uint16_t delta_wheel_time; /**< 1/1024 s ticks */
    uint16_t delta_crank_time;
} CscDerived;

typedef struct {
    bool has_prev;
    CscMeasurement prev;
    uint32_t wheel_circumference_mm;
} CscTracker;

/** Parse a CSC Measurement notification payload.
 *  Returns false if the buffer is too short for the declared flags. */
bool csc_measurement_parse(const uint8_t* data, uint16_t len, CscMeasurement* out);

/** Reset tracker state (call on connect / disconnect). */
void csc_tracker_reset(CscTracker* tracker, uint32_t wheel_circumference_mm);

/** Update tracker with a new sample and compute speed/cadence. */
void csc_tracker_update(CscTracker* tracker, const CscMeasurement* sample, CscDerived* out);

/** Decode incomplete/complete 16-bit UUID lists in advertising data.
 *  Returns true if target_uuid appears. */
bool csc_adv_has_service_uuid(const uint8_t* adv, uint8_t adv_len, uint16_t target_uuid);

/** Human-readable sensor location (CSC Sensor Location values). */
const char* csc_sensor_location_name(uint8_t location);
