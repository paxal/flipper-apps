#include "render_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Precomputed SVG coefficients for 128x64. */
static const float k_y_coef_c = BABAL_VIEWPORT_Y_MAX;
static const float k_y_coef_b = BABAL_LEVEL_Y_BALL *
                                (BABAL_VIEWPORT_Y_MAX - BABAL_VIEWPORT_Y_BALL) /
                                (BABAL_VIEWPORT_Y_BALL - BABAL_VIEWPORT_Y_0);
static const float k_y_coef_a = k_y_coef_b * (BABAL_VIEWPORT_Y_MAX - BABAL_VIEWPORT_Y_0);
static const float k_x_coef_d = BABAL_VIEWPORT_Y_MAX;
static const float k_x_coef_e =
    (BABAL_SCREEN_W / 1.666f) / (BABAL_VIEWPORT_Y_MAX - BABAL_VIEWPORT_Y_0);

float babal_level_to_viewport_y_f(float y_level) {
    if(y_level < 0.0f) {
        y_level = 0.0f;
    }
    return k_y_coef_c - k_y_coef_a / (k_y_coef_b + y_level);
}

float babal_level_to_viewport_x_f(float x_level, float y_viewport) {
    return (k_x_coef_d - y_viewport) * k_x_coef_e * x_level;
}

static babal_q16_t babal_q16_div(babal_q16_t num, babal_q16_t den) {
    if(den == 0) {
        return 0;
    }
    int64_t n = ((int64_t)num) << 16;
    return (babal_q16_t)(n / den);
}

static babal_q16_t babal_q16_mul(babal_q16_t a, babal_q16_t b) {
    return (babal_q16_t)(((int64_t)a * (int64_t)b) >> 16);
}

babal_q16_t babal_level_to_viewport_y_q16(babal_q16_t y_level) {
    if(y_level < 0) {
        y_level = 0;
    }
    babal_q16_t a = BABAL_Q16_FROM_FLOAT(k_y_coef_a);
    babal_q16_t b = BABAL_Q16_FROM_FLOAT(k_y_coef_b);
    babal_q16_t c = BABAL_Q16_FROM_FLOAT(k_y_coef_c);
    return c - babal_q16_div(a, b + y_level);
}

babal_q16_t babal_level_to_viewport_x_q16(babal_q16_t x_level, babal_q16_t y_viewport) {
    babal_q16_t d = BABAL_Q16_FROM_FLOAT(k_x_coef_d);
    babal_q16_t e = BABAL_Q16_FROM_FLOAT(k_x_coef_e);
    return babal_q16_mul(babal_q16_mul(d - y_viewport, e), x_level);
}

int16_t babal_adjust_y(int16_t y_up) {
    return (int16_t)(BABAL_SCREEN_H - 1 - y_up);
}

int16_t babal_adjust_x(int16_t x_centered) {
    return (int16_t)(BABAL_VIEWPORT_X_CENTER + x_centered);
}

static int16_t clamp_x(int16_t x) {
    if(x < 0) {
        return 0;
    }
    if(x > BABAL_SCREEN_W - 1) {
        return BABAL_SCREEN_W - 1;
    }
    return x;
}

void babal_lut_build(BabalDepthLut* lut, babal_q16_t progress) {
    lut->progress = progress;
    float p = BABAL_Q16_TO_FLOAT(progress);
    if(p < 0.0f) {
        p = 0.0f;
    }

    for(uint8_t i = 0; i < BABAL_MAX_DEPTH; i++) {
        float y0 = p + (float)i;
        float y1 = y0 + 1.0f;
        float vy0 = babal_level_to_viewport_y_f(y0);
        float vy1 = babal_level_to_viewport_y_f(y1);
        /* Full track spans x in [-LEVEL_WIDTH/2, +LEVEL_WIDTH/2] = [-4, 4]. */
        float half0 = fabsf(babal_level_to_viewport_x_f(4.0f, vy0));
        float half1 = fabsf(babal_level_to_viewport_x_f(4.0f, vy1));

        /* Convert SVG "up" Y to Flipper screen Y (down). Far = higher level Y → higher vy_up → smaller screen Y. */
        int16_t sy_near = babal_adjust_y((int16_t)lroundf(vy0));
        int16_t sy_far = babal_adjust_y((int16_t)lroundf(vy1));

        lut->rows[i].y_top = sy_far < sy_near ? sy_far : sy_near;
        lut->rows[i].y_bot = sy_far > sy_near ? sy_far : sy_near;
        lut->rows[i].half_w_near = (int16_t)lroundf(half0);
        lut->rows[i].half_w_far = (int16_t)lroundf(half1);
    }
}

void babal_lut_span_for_cell(
    const BabalDepthLut* lut,
    uint8_t depth_row,
    int8_t lane,
    int16_t* x0_near,
    int16_t* x1_near,
    int16_t* x0_far,
    int16_t* x1_far) {
    const BabalLutRow* row = &lut->rows[depth_row];
    /* Reconstruct edges from half-widths. Uses only table reads + mul/add/shift. */
    int16_t step_n = row->half_w_near >> 2; /* hn/4 */
    int16_t step_f = row->half_w_far >> 2;
    if(step_n == 0) {
        step_n = 1;
    }
    if(step_f == 0) {
        step_f = 1;
    }
    int16_t left = (int16_t)(lane - 4);
    int16_t right = (int16_t)(lane - 3);

    *x0_near = clamp_x(babal_adjust_x((int16_t)(left * step_n)));
    *x1_near = clamp_x(babal_adjust_x((int16_t)(right * step_n)));
    *x0_far = clamp_x(babal_adjust_x((int16_t)(left * step_f)));
    *x1_far = clamp_x(babal_adjust_x((int16_t)(right * step_f)));

    if(*x0_near > *x1_near) {
        int16_t t = *x0_near;
        *x0_near = *x1_near;
        *x1_near = t;
    }
    if(*x0_far > *x1_far) {
        int16_t t = *x0_far;
        *x0_far = *x1_far;
        *x1_far = t;
    }
}

void babal_fb_clear(BabalSoftFb* fb) {
    memset(fb->pixels, 0, BABAL_FB_BYTES);
}

void babal_fb_set(BabalSoftFb* fb, int16_t x, int16_t y) {
    if(x < 0 || x >= BABAL_SCREEN_W || y < 0 || y >= BABAL_SCREEN_H) {
        return;
    }
    uint16_t idx = (uint16_t)y * BABAL_FB_STRIDE + (uint16_t)(x >> 3);
    fb->pixels[idx] |= (uint8_t)(0x80 >> (x & 7));
}

bool babal_fb_get(const BabalSoftFb* fb, int16_t x, int16_t y) {
    if(x < 0 || x >= BABAL_SCREEN_W || y < 0 || y >= BABAL_SCREEN_H) {
        return false;
    }
    uint16_t idx = (uint16_t)y * BABAL_FB_STRIDE + (uint16_t)(x >> 3);
    return (fb->pixels[idx] & (uint8_t)(0x80 >> (x & 7))) != 0;
}

void babal_fb_hline(BabalSoftFb* fb, int16_t x0, int16_t x1, int16_t y) {
    if(y < 0 || y >= BABAL_SCREEN_H) {
        return;
    }
    if(x0 > x1) {
        int16_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if(x0 < 0) {
        x0 = 0;
    }
    if(x1 >= BABAL_SCREEN_W) {
        x1 = BABAL_SCREEN_W - 1;
    }
    for(int16_t x = x0; x <= x1; x++) {
        babal_fb_set(fb, x, y);
    }
}

void babal_fb_fill_trap(
    BabalSoftFb* fb,
    int16_t x0_near,
    int16_t x1_near,
    int16_t y_near,
    int16_t x0_far,
    int16_t x1_far,
    int16_t y_far) {
    if(y_near == y_far) {
        babal_fb_hline(fb, x0_near, x1_near, y_near);
        return;
    }
    int16_t y0 = y_near;
    int16_t y1 = y_far;
    if(y0 > y1) {
        int16_t t = y0;
        y0 = y1;
        y1 = t;
        t = x0_near;
        x0_near = x0_far;
        x0_far = t;
        t = x1_near;
        x1_near = x1_far;
        x1_far = t;
    }
    int32_t h = y1 - y0;
    for(int16_t y = y0; y <= y1; y++) {
        int32_t t = y - y0;
        int16_t xl = (int16_t)(x0_near + ((x0_far - x0_near) * t) / h);
        int16_t xr = (int16_t)(x1_near + ((x1_far - x1_near) * t) / h);
        babal_fb_hline(fb, xl, xr, y);
    }
}

void babal_fb_draw_ball(BabalSoftFb* fb, int16_t cx, int16_t cy, int16_t r) {
    int32_t r2 = (int32_t)r * r;
    for(int16_t dy = -r; dy <= r; dy++) {
        for(int16_t dx = -r; dx <= r; dx++) {
            if((int32_t)dx * dx + (int32_t)dy * dy <= r2) {
                babal_fb_set(fb, (int16_t)(cx + dx), (int16_t)(cy + dy));
            }
        }
    }
}

void babal_fb_invert(BabalSoftFb* fb) {
    for(size_t i = 0; i < BABAL_FB_BYTES; i++) {
        fb->pixels[i] = (uint8_t)~fb->pixels[i];
    }
}

static bool cell_is_platform(char c) {
    return c != ' ' && c != '\0' && c != '.';
}

uint16_t babal_render_frame(
    BabalSoftFb* fb,
    const BabalLevelView* level,
    babal_q16_t progress,
    babal_q16_t lane_shift,
    int16_t ball_height,
    bool draw_ball,
    bool invert) {
    babal_fb_clear(fb);

    BabalDepthLut lut;
    babal_lut_build(&lut, progress);

    float p = BABAL_Q16_TO_FLOAT(progress);
    int base = (int)floorf(p);
    if(base < 0) {
        base = 0;
    }

    uint16_t drawn = 0;
    float shift = BABAL_Q16_TO_FLOAT(lane_shift);

    for(uint8_t d = 0; d < BABAL_MAX_DEPTH; d++) {
        int row_i = base + (int)d;
        if(row_i < 0 || (uint16_t)row_i >= level->length) {
            continue;
        }
        const char* row = level->rows[row_i];
        const BabalLutRow* lr = &lut.rows[d];
        int16_t y_near = babal_adjust_y((int16_t)lroundf(babal_level_to_viewport_y_f(p + d)));
        int16_t y_far =
            babal_adjust_y((int16_t)lroundf(babal_level_to_viewport_y_f(p + d + 1.0f)));

        for(int8_t lane = 0; lane < BABAL_LEVEL_WIDTH; lane++) {
            char ch = row[lane];
            if(!cell_is_platform(ch)) {
                continue;
            }
            int16_t x0n, x1n, x0f, x1f;
            /* Apply player lateral shift by offsetting lane in fixed steps. */
            float lane_l = (float)lane - 4.0f - shift;
            float lane_r = lane_l + 1.0f;
            int16_t step_n = lr->half_w_near >> 2;
            int16_t step_f = lr->half_w_far >> 2;
            /* When step is 0, fall back to float projection once (edge case at horizon). */
            if(step_n == 0) {
                step_n = 1;
            }
            if(step_f == 0) {
                step_f = 1;
            }
            x0n = clamp_x(babal_adjust_x((int16_t)lroundf(lane_l * step_n)));
            x1n = clamp_x(babal_adjust_x((int16_t)lroundf(lane_r * step_n)));
            x0f = clamp_x(babal_adjust_x((int16_t)lroundf(lane_l * step_f)));
            x1f = clamp_x(babal_adjust_x((int16_t)lroundf(lane_r * step_f)));
            if(x0n > x1n) {
                int16_t t = x0n;
                x0n = x1n;
                x1n = t;
            }
            if(x0f > x1f) {
                int16_t t = x0f;
                x0f = x1f;
                x1f = t;
            }
            babal_fb_fill_trap(fb, x0n, x1n, y_near, x0f, x1f, y_far);
            drawn++;
        }
    }

    if(draw_ball) {
        int16_t cy = babal_adjust_y((int16_t)BABAL_VIEWPORT_Y_BALL) - ball_height;
        babal_fb_draw_ball(fb, BABAL_VIEWPORT_X_CENTER, cy, 3);
    }

    if(invert) {
        babal_fb_invert(fb);
    }

    return drawn;
}

bool babal_fb_equal(const BabalSoftFb* a, const BabalSoftFb* b) {
    return memcmp(a->pixels, b->pixels, BABAL_FB_BYTES) == 0;
}

bool babal_fb_to_pbm(const BabalSoftFb* fb, uint8_t** out, size_t* out_len) {
    char header[64];
    int hlen = snprintf(header, sizeof(header), "P4\n%d %d\n", BABAL_SCREEN_W, BABAL_SCREEN_H);
    if(hlen < 0) {
        return false;
    }
    size_t total = (size_t)hlen + BABAL_FB_BYTES;
    uint8_t* buf = (uint8_t*)malloc(total);
    if(!buf) {
        return false;
    }
    memcpy(buf, header, (size_t)hlen);
    memcpy(buf + hlen, fb->pixels, BABAL_FB_BYTES);
    *out = buf;
    *out_len = total;
    return true;
}

bool babal_fb_write_pbm_file(const BabalSoftFb* fb, const char* path) {
    uint8_t* buf = NULL;
    size_t len = 0;
    if(!babal_fb_to_pbm(fb, &buf, &len)) {
        return false;
    }
    FILE* f = fopen(path, "wb");
    if(!f) {
        free(buf);
        return false;
    }
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);
    return w == len;
}

bool babal_fb_load_pbm_file(BabalSoftFb* fb, const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) {
        return false;
    }
    char magic[3] = {0};
    if(fread(magic, 1, 2, f) != 2 || magic[0] != 'P' || magic[1] != '4') {
        fclose(f);
        return false;
    }
    int c = fgetc(f);
    if(c != '\n' && c != ' ' && c != '\r') {
        /* allow whitespace */
    }
    /* Skip optional comment lines. */
    for(;;) {
        long pos = ftell(f);
        char line[128];
        if(!fgets(line, sizeof(line), f)) {
            fclose(f);
            return false;
        }
        if(line[0] == '#') {
            continue;
        }
        fseek(f, pos, SEEK_SET);
        break;
    }
    int w = 0, h = 0;
    if(fscanf(f, "%d %d", &w, &h) != 2 || w != BABAL_SCREEN_W || h != BABAL_SCREEN_H) {
        fclose(f);
        return false;
    }
    /* Consume single whitespace before raster. */
    int ch;
    do {
        ch = fgetc(f);
    } while(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
    if(ch != EOF) {
        ungetc(ch, f);
    }
    size_t n = fread(fb->pixels, 1, BABAL_FB_BYTES, f);
    fclose(f);
    return n == BABAL_FB_BYTES;
}
