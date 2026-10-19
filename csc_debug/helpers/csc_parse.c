#include "csc_parse.h"

#include <string.h>

bool csc_measurement_parse(const uint8_t* data, uint16_t len, CscMeasurement* out) {
    if(!data || !out || len < 1) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->flags = data[0];
    out->wheel_present = (out->flags & CSC_FLAG_WHEEL_PRESENT) != 0;
    out->crank_present = (out->flags & CSC_FLAG_CRANK_PRESENT) != 0;

    uint16_t need = 1;
    if(out->wheel_present) {
        need += 6; /* uint32 revs + uint16 time */
    }
    if(out->crank_present) {
        need += 4; /* uint16 revs + uint16 time */
    }
    if(len < need) {
        return false;
    }

    uint16_t off = 1;
    if(out->wheel_present) {
        out->cumulative_wheel_revs = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8) |
                                     ((uint32_t)data[off + 2] << 16) |
                                     ((uint32_t)data[off + 3] << 24);
        out->last_wheel_event_time = (uint16_t)data[off + 4] | ((uint16_t)data[off + 5] << 8);
        off += 6;
    }
    if(out->crank_present) {
        out->cumulative_crank_revs = (uint16_t)data[off] | ((uint16_t)data[off + 1] << 8);
        out->last_crank_event_time = (uint16_t)data[off + 2] | ((uint16_t)data[off + 3] << 8);
    }
    return true;
}

void csc_tracker_reset(CscTracker* tracker, uint32_t wheel_circumference_mm) {
    if(!tracker) {
        return;
    }
    memset(tracker, 0, sizeof(*tracker));
    tracker->wheel_circumference_mm =
        wheel_circumference_mm ? wheel_circumference_mm : CSC_DEFAULT_WHEEL_CIRCUMFERENCE_MM;
}

static uint16_t csc_time_delta(uint16_t prev, uint16_t now) {
    return (uint16_t)(now - prev); /* unsigned wrap handles 16-bit rollover */
}

void csc_tracker_update(CscTracker* tracker, const CscMeasurement* sample, CscDerived* out) {
    if(!tracker || !sample || !out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->wheel_revs = sample->cumulative_wheel_revs;
    out->crank_revs = sample->cumulative_crank_revs;

    if(!tracker->has_prev) {
        tracker->prev = *sample;
        tracker->has_prev = true;
        out->valid = false;
        return;
    }

    out->valid = true;

    if(sample->wheel_present && tracker->prev.wheel_present) {
        uint32_t d_revs = sample->cumulative_wheel_revs - tracker->prev.cumulative_wheel_revs;
        uint16_t d_time =
            csc_time_delta(tracker->prev.last_wheel_event_time, sample->last_wheel_event_time);
        out->delta_wheel_revs = d_revs;
        out->delta_wheel_time = d_time;
        if(d_time > 0 && d_revs > 0 && d_revs < 256) {
            /* speed_m_s = (d_revs * circ_m) / (d_time / 1024)
             * km/h = m/s * 3.6 */
            float circ_m = (float)tracker->wheel_circumference_mm / 1000.0f;
            float seconds = (float)d_time / 1024.0f;
            float mps = ((float)d_revs * circ_m) / seconds;
            out->speed_kmh = mps * 3.6f;
        }
    }

    if(sample->crank_present && tracker->prev.crank_present) {
        uint16_t d_revs =
            (uint16_t)(sample->cumulative_crank_revs - tracker->prev.cumulative_crank_revs);
        uint16_t d_time =
            csc_time_delta(tracker->prev.last_crank_event_time, sample->last_crank_event_time);
        out->delta_crank_revs = d_revs;
        out->delta_crank_time = d_time;
        if(d_time > 0 && d_revs > 0 && d_revs < 64) {
            float seconds = (float)d_time / 1024.0f;
            out->cadence_rpm = ((float)d_revs * 60.0f) / seconds;
        }
    }

    tracker->prev = *sample;
}

bool csc_adv_has_service_uuid(const uint8_t* adv, uint8_t adv_len, uint16_t target_uuid) {
    if(!adv) {
        return false;
    }

    uint8_t i = 0;
    while(i + 1 < adv_len) {
        uint8_t field_len = adv[i];
        if(field_len == 0) {
            break;
        }
        if((uint16_t)i + 1u + field_len > adv_len) {
            break;
        }
        uint8_t type = adv[i + 1];
        /* 0x02 Incomplete List of 16-bit Service UUIDs
         * 0x03 Complete List of 16-bit Service UUIDs
         * 0x14 List of 16-bit Service Solicitation UUIDs */
        if(type == 0x02 || type == 0x03 || type == 0x14) {
            for(uint8_t j = 2; j + 1 < field_len + 1; j += 2) {
                uint16_t uuid = (uint16_t)adv[i + j] | ((uint16_t)adv[i + j + 1] << 8);
                if(uuid == target_uuid) {
                    return true;
                }
            }
        }
        i = (uint8_t)(i + 1 + field_len);
    }
    return false;
}

const char* csc_sensor_location_name(uint8_t location) {
    static const char* const names[] = {
        "Other",
        "Top of shoe",
        "In shoe",
        "Hip",
        "Front Wheel",
        "Left Crank",
        "Right Crank",
        "Left Pedal",
        "Right Pedal",
        "Front Hub",
        "Rear Dropout",
        "Chainstay",
        "Rear Wheel",
        "Rear Hub",
        "Chest",
        "Spider",
        "Chain Ring",
    };
    if(location < (sizeof(names) / sizeof(names[0]))) {
        return names[location];
    }
    return "Unknown";
}
