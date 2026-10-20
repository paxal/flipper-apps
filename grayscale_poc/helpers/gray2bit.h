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

/** XBM size for a full-screen 1-bit framebuffer (row-major, LSB = left). */
#define GRAY2BIT_XBM_SIZE ((GRAY2BIT_SCREEN_W * GRAY2BIT_SCREEN_H) / 8)

/**
 * FRC period shared by TI dual-plane (2:1) and staggered duty modes.
 * Matches TI-89/83 grayscale (~66% dark plane / ~34% light plane) and
 * HP-48 GREYSCAL 2-frame 2:1 weighting when mapped onto three ticks.
 */
#define GRAY2BIT_FRC_PERIOD 3

/**
 * Gray levels (dark → light). On Flipper, "on" means black ink.
 *   0 = black (always ink)
 *   1 = dark gray
 *   2 = light gray
 *   3 = white (never ink)
 */
typedef enum {
    GrayLevelBlack = 0,
    GrayLevelDark = 1,
    GrayLevelLight = 2,
    GrayLevelWhite = 3,
} GrayLevel;

/**
 * Display modes — calculator-era tricks adapted to Flipper's 1-bit LCD.
 *
 * TI planes: classic TI-89/83 dual bitplane + 2:1 interrupt schedule
 *            (TIGCC grayscale tutorial). Flipper can't retarget LCD DMA,
 *            so we blit the active plane each tick instead.
 * Stagger:   same 3/3·2/3·1/3·0/3 duties with (x+y)%3 phase offset.
 * Bayer:     static ordered dither (HP/Babal-style spatial gray, no flicker).
 * Threshold: hard 1-bit clamp for A/B comparison.
 */
typedef enum {
    GrayModeTiPlanes = 0,
    GrayModeStagger = 1,
    GrayModeBayer = 2,
    GrayModeThreshold = 3,
    GrayModeCount = 4,
} GrayMode;

/** Read a 2-bit pixel from a packed buffer (row-major, 4 pixels/byte, MSBits first). */
uint8_t gray2bit_get(const uint8_t* packed, uint16_t width, uint16_t x, uint16_t y);

/** Write a 2-bit pixel into a packed buffer. level is clamped to 0..3. */
void gray2bit_set(uint8_t* packed, uint16_t width, uint16_t x, uint16_t y, uint8_t level);

/**
 * TI dual-plane bit for one gray level.
 * Dark plane holds black+dark; light plane holds black+light (TIGCC truth table).
 */
void gray2bit_ti_plane_bits(uint8_t level, bool* in_dark_plane, bool* in_light_plane);

/** Which plane to show on this FRC phase: 0 = dark (2 ticks), 1 = light (1 tick). */
uint8_t gray2bit_ti_plane_index(uint8_t phase);

/** Bake packed 2-bit image into TI dark + light XBM planes. */
void gray2bit_pack_to_ti_planes(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    uint8_t* dark_plane_xbm,
    uint8_t* light_plane_xbm);

/** Staggered temporal FRC ink decision (duty + spatial phase). */
bool gray2bit_stagger_ink(uint8_t level, uint8_t phase, uint8_t spatial);

/** Bayer 2×2 spatial dither ink decision (static). */
bool gray2bit_bayer_ink(uint8_t level, uint16_t x, uint16_t y);

/**
 * Render one frame for the given mode into an XBM buffer.
 * For GrayModeTiPlanes, prefers pre-baked planes when both non-NULL; otherwise
 * derives plane bits on the fly from packed.
 */
void gray2bit_render_xbm(
    const uint8_t* packed,
    uint16_t width,
    uint16_t height,
    GrayMode mode,
    uint8_t phase,
    const uint8_t* ti_dark_plane,
    const uint8_t* ti_light_plane,
    uint8_t* xbm_out);

/** Short HUD label for a mode (static string). */
const char* gray2bit_mode_name(GrayMode mode);

/** Ink duty in ticks out of GRAY2BIT_FRC_PERIOD for TI/stagger temporal modes. */
uint8_t gray2bit_duty(uint8_t level);

/** True if this mode advances the FRC phase each tick. */
bool gray2bit_mode_needs_frc(GrayMode mode);

#ifdef __cplusplus
}
#endif
