#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BABAL_SCREEN_W         128
#define BABAL_SCREEN_H         64
#define BABAL_LEVEL_WIDTH      8
#define BABAL_MAX_DEPTH        8
#define BABAL_REWIND_PLATFORMS 64

/* Soft 1-bit framebuffer: row-major, MSB left, 16 bytes per row. */
#define BABAL_FB_STRIDE ((BABAL_SCREEN_W + 7) / 8)
#define BABAL_FB_BYTES  (BABAL_FB_STRIDE * BABAL_SCREEN_H)

/* Q16 fixed-point (16.16). */
typedef int32_t babal_q16_t;

#define BABAL_Q16_ONE           ((babal_q16_t)65536)
#define BABAL_Q16_FROM_INT(i)   ((babal_q16_t)(i) * BABAL_Q16_ONE)
#define BABAL_Q16_FROM_FLOAT(f) ((babal_q16_t)((f) * 65536.0f + ((f) >= 0 ? 0.5f : -0.5f)))
#define BABAL_Q16_TO_INT(q)     ((int32_t)((q) >> 16))
#define BABAL_Q16_TO_FLOAT(q)   ((float)(q) / 65536.0f)

/*
 * SVG Babal anchors (ufoot svgbabal.svg), adapted to Flipper 128×64.
 * Soft FB: bit=1 = dark LCD ink (platforms / ball dither); bit=0 = light void.
 * Matches HP-48 Babal: solid dark pavés on light background, dithered ball.
 *
 * X_NEAR: SVG uses W/1.666 (unit width at y0), which fills the whole 128×64
 * frame for solid near pavés. Retune so a full-width track keeps light side
 * voids like the HP LCD screenshots (~60% width at the ball plane).
 */
#define BABAL_LEVEL_Y_BALL      0.1666f
#define BABAL_VIEWPORT_Y_MAX    64.0f
#define BABAL_VIEWPORT_Y_BALL   (BABAL_VIEWPORT_Y_MAX / 4.0f) /* ~H/4 */
#define BABAL_VIEWPORT_Y_0      0.0f
#define BABAL_VIEWPORT_X_CENTER 64
#define BABAL_VIEWPORT_X_NEAR   (BABAL_SCREEN_W / 11.0f)
#define BABAL_BALL_R            8 /* SVG BALL_SIZE=16 on H=64 → radius 8 */
#define BABAL_START_PROGRESS    0.5f /* SVG START_Y — near cell straddles ball */

typedef struct {
    int16_t y_top; /* screen Y of near edge of depth row (Flipper: down positive) */
    int16_t y_bot; /* screen Y of far edge of depth row */
    int16_t half_w_near; /* half-width of full track at near edge */
    int16_t half_w_far; /* half-width of full track at far edge */
} BabalLutRow;

typedef struct {
    BabalLutRow rows[BABAL_MAX_DEPTH];
    babal_q16_t progress; /* fractional depth offset into level */
} BabalDepthLut;

typedef struct {
    uint8_t pixels[BABAL_FB_BYTES];
} BabalSoftFb;

/* Float oracle (same SVG formulas) — for tests / LUT build. */
float babal_level_to_viewport_y_f(float y_level);
float babal_level_to_viewport_x_f(float x_level, float y_viewport);

/* Q16 projection. */
babal_q16_t babal_level_to_viewport_y_q16(babal_q16_t y_level);
babal_q16_t babal_level_to_viewport_x_q16(babal_q16_t x_level, babal_q16_t y_viewport);

/* Screen coords (Flipper Y down). */
int16_t babal_adjust_y(int16_t y_up);
int16_t babal_adjust_x(int16_t x_centered);

void babal_lut_build(BabalDepthLut* lut, babal_q16_t progress);
void babal_lut_span_for_cell(
    const BabalDepthLut* lut,
    uint8_t depth_row,
    int8_t lane /* 0..7 */,
    int16_t* x0_near,
    int16_t* x1_near,
    int16_t* x0_far,
    int16_t* x1_far);

void babal_fb_clear(BabalSoftFb* fb);
void babal_fb_set(BabalSoftFb* fb, int16_t x, int16_t y);
void babal_fb_clear_pixel(BabalSoftFb* fb, int16_t x, int16_t y);
bool babal_fb_get(const BabalSoftFb* fb, int16_t x, int16_t y);
void babal_fb_hline(BabalSoftFb* fb, int16_t x0, int16_t x1, int16_t y);
void babal_fb_fill_trap(
    BabalSoftFb* fb,
    int16_t x0_near,
    int16_t x1_near,
    int16_t y_near,
    int16_t x0_far,
    int16_t x1_far,
    int16_t y_far);
/* Checkerboard dither disc (HP Babal ball GROB look). */
void babal_fb_draw_ball(BabalSoftFb* fb, int16_t cx, int16_t cy, int16_t r);
void babal_fb_invert(BabalSoftFb* fb);

/* Draw visible strip from ASCII rows ("*"/" " / "#"=platform). */
typedef struct {
    const char* const* rows; /* each row length >= LEVEL_WIDTH */
    uint16_t length;
} BabalLevelView;

/*
 * Render platforms into soft FB. Returns number of platform cells drawn.
 * lane_shift: player lateral offset in lanes (float via Q16, 0 = centered on lane 4).
 * ball_height: jump height in screen-up pixels (0 on ground).
 * draw_ball: whether to blit ball sprite.
 */
uint16_t babal_render_frame(
    BabalSoftFb* fb,
    const BabalLevelView* level,
    babal_q16_t progress,
    babal_q16_t lane_shift,
    int16_t ball_height,
    bool draw_ball,
    bool invert);

/* Soft FB → PBM (P4). Caller frees *out via free(). */
bool babal_fb_to_pbm(const BabalSoftFb* fb, uint8_t** out, size_t* out_len);
bool babal_fb_write_pbm_file(const BabalSoftFb* fb, const char* path);
bool babal_fb_load_pbm_file(BabalSoftFb* fb, const char* path);
bool babal_fb_equal(const BabalSoftFb* a, const BabalSoftFb* b);
