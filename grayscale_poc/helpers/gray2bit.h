#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Flipper LCD size used by the POC image. */
#define GRAY2BIT_SCREEN_W 128
#define GRAY2BIT_SCREEN_H 64

/** Packed size for a full-screen 2-bit framebuffer (2 bits/pixel). */
#define GRAY2BIT_PACKED_SIZE ((GRAY2BIT_SCREEN_W * GRAY2BIT_SCREEN_H) / 4)

/** XBM size for a full-screen 1-bit framebuffer (8 pixels/byte, MSB-left row-major). */
#define GRAY2BIT_XBM_SIZE ((GRAY2BIT_SCREEN_W * GRAY2BIT_SCREEN_H) / 8)

/** FRC period for 4 gray levels: duty 3/3, 2/3, 1/3, 0/3. */
#define GRAY2BIT_FRC_PERIOD 3

/**
 * Gray levels (dark → light). On Flipper, "on" means black ink.
 *   0 = black (always ink)
 *   1 = dark gray (ink 2/3 frames)
 *   2 = light gray (ink 1/3 frames)
 *   3 = white (never ink)
 */
typedef enum {
    GrayLevelBlack = 0,
    GrayLevelDark = 1,
    GrayLevelLight = 2,
    GrayLevelWhite = 3,
} GrayLevel;

/** Read a 2-bit pixel from a packed buffer (row-major, 4 pixels/byte, MSBits first). */
uint8_t gray2bit_get(const uint8_t* packed, uint16_t width, uint16_t x, uint16_t y);

/** Write a 2-bit pixel into a packed buffer. level is clamped to 0..3. */
void gray2bit_set(uint8_t* packed, uint16_t width, uint16_t x, uint16_t y, uint8_t level);

/**
 * Frame-rate-control decision: should this gray level be drawn as black ink
 * on the given FRC phase?
 *
 * @param level   0..3 gray level
 * @param phase   0..GRAY2BIT_FRC_PERIOD-1 frame counter
 * @param spatial per-pixel phase offset (typically (x + y) % period) to desync flicker
 */
bool gray2bit_frc_ink(uint8_t level, uint8_t phase, uint8_t spatial);

/**
 * Render a packed 2-bit image to a 1-bit XBM buffer for one FRC frame.
 * XBM layout: row-major, 1 bit/pixel, LSB = leftmost pixel in each byte
 * (u8g2 / Flipper canvas_draw_xbm convention). width must be a multiple of 8.
 */
void gray2bit_render_xbm(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    uint8_t phase,
    bool use_frc,
    uint8_t* xbm_out);

/** Average ink duty (0..period) for a gray level — useful for host tests. */
uint8_t gray2bit_duty(uint8_t level);

#ifdef __cplusplus
}
#endif
