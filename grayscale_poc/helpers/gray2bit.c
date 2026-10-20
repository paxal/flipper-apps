#include "gray2bit.h"

uint8_t gray2bit_get(const uint8_t* packed, uint16_t width, uint16_t x, uint16_t y) {
    uint32_t idx = (uint32_t)y * (uint32_t)width + (uint32_t)x;
    uint32_t byte_i = idx >> 2;
    uint8_t shift = (uint8_t)((3u - (idx & 3u)) << 1);
    return (uint8_t)((packed[byte_i] >> shift) & 0x03u);
}

void gray2bit_set(uint8_t* packed, uint16_t width, uint16_t x, uint16_t y, uint8_t level) {
    level &= 0x03u;
    uint32_t idx = (uint32_t)y * (uint32_t)width + (uint32_t)x;
    uint32_t byte_i = idx >> 2;
    uint8_t shift = (uint8_t)((3u - (idx & 3u)) << 1);
    uint8_t mask = (uint8_t)(0x03u << shift);
    packed[byte_i] = (uint8_t)((packed[byte_i] & (uint8_t)~mask) | (level << shift));
}

uint8_t gray2bit_duty(uint8_t level) {
    /* TI/HP 2:1 temporal weighting on a 3-tick cycle. */
    static const uint8_t k_duty[4] = {3, 2, 1, 0};
    return k_duty[level & 0x03u];
}

void gray2bit_ti_plane_bits(uint8_t level, bool* in_dark_plane, bool* in_light_plane) {
    /* TIGCC / TI-89 grayscale truth table (Plane1 = dark, Plane2 = light). */
    switch(level & 0x03u) {
    case GrayLevelBlack:
        *in_dark_plane = true;
        *in_light_plane = true;
        break;
    case GrayLevelDark:
        *in_dark_plane = true;
        *in_light_plane = false;
        break;
    case GrayLevelLight:
        *in_dark_plane = false;
        *in_light_plane = true;
        break;
    default:
        *in_dark_plane = false;
        *in_light_plane = false;
        break;
    }
}

uint8_t gray2bit_ti_plane_index(uint8_t phase) {
    /* Two ticks on dark plane, one on light → ~66% / ~34% like TI interrupt gray. */
    phase = (uint8_t)(phase % GRAY2BIT_FRC_PERIOD);
    return (phase < 2) ? 0 : 1;
}

bool gray2bit_stagger_ink(uint8_t level, uint8_t phase, uint8_t spatial) {
    uint8_t duty = gray2bit_duty(level);
    if(duty == 0) {
        return false;
    }
    if(duty >= GRAY2BIT_FRC_PERIOD) {
        return true;
    }
    uint8_t slot = (uint8_t)((phase + spatial) % GRAY2BIT_FRC_PERIOD);
    return slot < duty;
}

bool gray2bit_bayer_ink(uint8_t level, uint16_t x, uint16_t y) {
    /* 2×2 Bayer ranks 0..3. Ink the darkest N cells for each level
     * (N = 4,2,1,0) — spatial cousin of HP/Babal checker mid-tones. */
    static const uint8_t k_bayer[2][2] = {{0, 2}, {3, 1}};
    static const uint8_t k_slots[4] = {4, 2, 1, 0};
    return k_bayer[y & 1u][x & 1u] < k_slots[level & 0x03u];
}

bool gray2bit_mode_needs_frc(GrayMode mode) {
    return mode == GrayModeTiPlanes || mode == GrayModeStagger;
}

const char* gray2bit_mode_name(GrayMode mode) {
    switch(mode) {
    case GrayModeTiPlanes:
        return "TI planes";
    case GrayModeStagger:
        return "Stagger";
    case GrayModeBayer:
        return "HP Bayer";
    case GrayModeThreshold:
        return "Thresh";
    default:
        return "?";
    }
}

static void
    gray2bit_write_xbm_bit(uint8_t* xbm, uint16_t width, uint16_t x, uint16_t y, bool ink) {
    if(!ink) {
        return;
    }
    uint16_t row_bytes = (uint16_t)(width / 8);
    uint16_t bx = (uint16_t)(x / 8);
    uint8_t bit = (uint8_t)(x & 7u);
    xbm[y * row_bytes + bx] |= (uint8_t)(1u << bit);
}

void gray2bit_pack_to_ti_planes(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    uint8_t* dark_plane_xbm,
    uint8_t* light_plane_xbm) {
    size_t bytes = ((size_t)width * (size_t)height) / 8u;
    for(size_t i = 0; i < bytes; i++) {
        dark_plane_xbm[i] = 0;
        light_plane_xbm[i] = 0;
    }

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t x = 0; x < width; x++) {
            bool dark = false;
            bool light = false;
            gray2bit_ti_plane_bits(gray2bit_get(packed, width, x, y), &dark, &light);
            gray2bit_write_xbm_bit(dark_plane_xbm, width, x, y, dark);
            gray2bit_write_xbm_bit(light_plane_xbm, width, x, y, light);
        }
    }
}

void gray2bit_render_xbm(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    GrayMode mode,
    uint8_t phase,
    const uint8_t* ti_dark_plane,
    const uint8_t* ti_light_plane,
    uint8_t* xbm_out) {
    uint16_t row_bytes = (uint16_t)(width / 8);
    size_t bytes = (size_t)row_bytes * (size_t)height;
    phase = (uint8_t)(phase % GRAY2BIT_FRC_PERIOD);

    if(mode == GrayModeTiPlanes && ti_dark_plane && ti_light_plane) {
        const uint8_t* src = (gray2bit_ti_plane_index(phase) == 0) ? ti_dark_plane :
                                                                     ti_light_plane;
        for(size_t i = 0; i < bytes; i++) {
            xbm_out[i] = src[i];
        }
        return;
    }

    for(size_t i = 0; i < bytes; i++) {
        xbm_out[i] = 0;
    }

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t bx = 0; bx < row_bytes; bx++) {
            uint8_t byte = 0;
            for(uint8_t bit = 0; bit < 8; bit++) {
                uint16_t x = (uint16_t)(bx * 8u + bit);
                uint8_t level = gray2bit_get(packed, width, x, y);
                bool ink = false;

                switch(mode) {
                case GrayModeTiPlanes: {
                    bool dark = false;
                    bool light = false;
                    gray2bit_ti_plane_bits(level, &dark, &light);
                    ink = (gray2bit_ti_plane_index(phase) == 0) ? dark : light;
                    break;
                }
                case GrayModeStagger: {
                    uint8_t spatial = (uint8_t)((x + y) % GRAY2BIT_FRC_PERIOD);
                    ink = gray2bit_stagger_ink(level, phase, spatial);
                    break;
                }
                case GrayModeBayer:
                    ink = gray2bit_bayer_ink(level, x, y);
                    break;
                case GrayModeThreshold:
                default:
                    ink = level < 2;
                    break;
                }

                if(ink) {
                    byte |= (uint8_t)(1u << bit);
                }
            }
            xbm_out[y * row_bytes + bx] = byte;
        }
    }
}
