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
    /* Ink frames out of GRAY2BIT_FRC_PERIOD: black→white */
    static const uint8_t k_duty[4] = {3, 2, 1, 0};
    return k_duty[level & 0x03u];
}

bool gray2bit_frc_ink(uint8_t level, uint8_t phase, uint8_t spatial) {
    uint8_t duty = gray2bit_duty(level);
    if(duty == 0) {
        return false;
    }
    if(duty >= GRAY2BIT_FRC_PERIOD) {
        return true;
    }
    /* Rotate which sub-frames are inked using spatial offset. */
    uint8_t slot = (uint8_t)((phase + spatial) % GRAY2BIT_FRC_PERIOD);
    return slot < duty;
}

void gray2bit_render_xbm(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    uint8_t phase,
    bool use_frc,
    uint8_t* xbm_out) {
    uint16_t row_bytes = (uint16_t)(width / 8);
    phase = (uint8_t)(phase % GRAY2BIT_FRC_PERIOD);

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t bx = 0; bx < row_bytes; bx++) {
            uint8_t byte = 0;
            for(uint8_t bit = 0; bit < 8; bit++) {
                uint16_t x = (uint16_t)(bx * 8u + bit);
                uint8_t level = gray2bit_get(packed, width, x, y);
                bool ink;
                if(use_frc) {
                    uint8_t spatial = (uint8_t)((x + y) % GRAY2BIT_FRC_PERIOD);
                    ink = gray2bit_frc_ink(level, phase, spatial);
                } else {
                    /* Static threshold: dark half → black, light half → white. */
                    ink = level < 2;
                }
                if(ink) {
                    byte |= (uint8_t)(1u << bit); /* LSB = leftmost */
                }
            }
            xbm_out[y * row_bytes + bx] = byte;
        }
    }
}
