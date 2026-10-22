#include "render.h"
#include "helpers/gray2bit.h"
#include "levels/gfx_data.h"

#include <stdio.h>
#include <string.h>

static const uint8_t k_font[39][5] = {
    {7, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7},
    {7, 4, 6, 4, 4}, {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 7},
    {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {5, 7, 5, 5, 5}, {5, 7, 7, 5, 5}, {7, 5, 5, 5, 7},
    {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {7, 5, 7, 6, 5}, {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2},
    {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 5, 7, 5}, {5, 5, 2, 5, 5}, {5, 5, 7, 2, 2},
    {7, 1, 2, 4, 7}, {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}, {7, 1, 2, 0, 2}, {0, 0, 7, 0, 0}, {0, 2, 0, 2, 0},
};

static int glyph_index(char c) {
    if(c >= 'a' && c <= 'z') {
        c = (char)(c - 32);
    }
    if(c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if(c >= '0' && c <= '9') {
        return 26 + (c - '0');
    }
    if(c == '?') {
        return 36;
    }
    if(c == '-') {
        return 37;
    }
    if(c == ':') {
        return 38;
    }
    return -1;
}

static void put_px(uint8_t* packed, int x, int y, uint8_t level) {
    if((unsigned)x >= GRAY2BIT_SCREEN_W || (unsigned)y >= GRAY2BIT_SCREEN_H) {
        return;
    }
    gray2bit_set(packed, GRAY2BIT_SCREEN_W, (uint16_t)x, (uint16_t)y, level);
}

static uint8_t get_px(const uint8_t* packed, int x, int y) {
    return gray2bit_get(packed, GRAY2BIT_SCREEN_W, (uint16_t)x, (uint16_t)y);
}

static void draw_glyph(uint8_t* packed, int x, int y, int index, uint8_t ink, int scale) {
    const uint8_t* rows = k_font[index];
    for(int row = 0; row < 5; row++) {
        for(int col = 0; col < 3; col++) {
            if((rows[row] & (1 << (2 - col))) == 0) {
                continue;
            }
            for(int sy = 0; sy < scale; sy++) {
                for(int sx = 0; sx < scale; sx++) {
                    put_px(packed, x + col * scale + sx, y + row * scale + sy, ink);
                }
            }
        }
    }
}

static void draw_text(uint8_t* packed, int x, int y, const char* text, uint8_t ink, int scale) {
    for(; text != NULL && *text != '\0'; text++) {
        int index = glyph_index(*text);
        if(index >= 0) {
            draw_glyph(packed, x, y, index, ink, scale);
        }
        x += 4 * scale;
    }
}

static void draw_centered(uint8_t* packed, int y, const char* text, uint8_t ink, int scale) {
    int len = 0;
    int x;
    if(text == NULL) {
        return;
    }
    while(text[len] != '\0') {
        len++;
    }
    x = (GRAY2BIT_SCREEN_W - len * 4 * scale) / 2;
    if(x < 0) {
        x = 0;
    }
    draw_text(packed, x, y, text, ink, scale);
}

static void fill_rect(uint8_t* packed, int x0, int y0, int w, int h, uint8_t level) {
    for(int y = y0; y < y0 + h; y++) {
        for(int x = x0; x < x0 + w; x++) {
            put_px(packed, x, y, level);
        }
    }
}

/* Blit an 8x8 bank tile; rows below max_y are clipped (HUD strip). */
static void draw_tile(uint8_t* packed, int x, int y, uint8_t index, int max_y) {
    const uint8_t* rows;
    if(index >= ICE_TILE_COUNT || x <= -ICE_TILE || x >= GRAY2BIT_SCREEN_W || y <= -ICE_TILE ||
       y >= max_y) {
        return;
    }
    rows = &ice_tiles[(size_t)index * ICE_TILE_BYTES];
    for(int ly = 0; ly < ICE_TILE; ly++) {
        int sy = y + ly;
        if(sy < 0 || sy >= max_y) {
            continue;
        }
        for(int lx = 0; lx < ICE_TILE; lx++) {
            uint8_t level = (uint8_t)((rows[ly * 2 + lx / 4] >> ((3 - (lx & 3)) * 2)) & 3);
            put_px(packed, x + lx, sy, level);
        }
    }
}

/* The cube sprite: per row a mask byte (1 = keep background) then a pixel byte (1 = black). */
static void draw_cube(uint8_t* packed, int x, int y, uint8_t state, int max_y) {
    const uint8_t* rows = &ice_cube_rows[(size_t)(state & 3) * ICE_CUBE * 2];
    for(int ly = 0; ly < ICE_CUBE; ly++) {
        uint8_t mask = rows[ly * 2];
        uint8_t pix = rows[ly * 2 + 1];
        int sy = y + ly;
        if(sy < 0 || sy >= max_y) {
            continue;
        }
        for(int lx = 0; lx < ICE_CUBE; lx++) {
            if(pix & (1 << lx)) {
                put_px(packed, x + lx, sy, GrayLevelBlack);
            } else if((mask & (1 << lx)) == 0) {
                put_px(packed, x + lx, sy, GrayLevelWhite);
            }
        }
    }
}

static void draw_picture(uint8_t* packed, const uint8_t* pic) {
    memcpy(packed, pic, GRAY2BIT_PACKED_SIZE);
}

static void draw_world(uint8_t* packed, const IceGame* g) {
    int16_t cam_x;
    int16_t cam_y;
    int tx0;
    int ty0;

    ice_game_camera(g, &cam_x, &cam_y);
    tx0 = cam_x / ICE_TILE;
    ty0 = cam_y / ICE_TILE;

    for(int ty = ty0; ty <= ty0 + ICE_VIEW_H / ICE_TILE && ty < g->height; ty++) {
        for(int tx = tx0; tx <= tx0 + ICE_VIEW_W / ICE_TILE && tx < g->width; tx++) {
            uint8_t raw = g->map[(uint16_t)ty * g->width + (uint16_t)tx];
            draw_tile(packed, tx * ICE_TILE - cam_x, ty * ICE_TILE - cam_y, raw, ICE_VIEW_H);
        }
    }

    draw_cube(packed, g->px - cam_x, g->py - cam_y, g->key, ICE_VIEW_H);

    if(g->blast > 0) {
        int cx = g->px - cam_x + ICE_CUBE / 2;
        int cy = g->py - cam_y + ICE_CUBE / 2;
        for(int d = 5; d <= 9; d += 2) {
            put_px(packed, cx - d, cy, GrayLevelBlack);
            put_px(packed, cx + d, cy, GrayLevelBlack);
            put_px(packed, cx, cy - d, GrayLevelBlack);
            put_px(packed, cx, cy + d, GrayLevelBlack);
            put_px(packed, cx - d, cy - d, GrayLevelBlack);
            put_px(packed, cx + d, cy + d, GrayLevelBlack);
            put_px(packed, cx - d, cy + d, GrayLevelBlack);
            put_px(packed, cx + d, cy - d, GrayLevelBlack);
        }
    }
}

static void draw_hud(uint8_t* packed, const IceGame* g) {
    char buf[32];
    const char* flag = "";

    fill_rect(
        packed, 0, ICE_VIEW_H, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H - ICE_VIEW_H, GrayLevelWhite);
    if(g->pawns_left == 0) {
        flag = " EXIT";
    } else if(g->dynamite > 0) {
        flag = " ALERT";
    } else if(ice_time_seconds(g) <= 20) {
        flag = " BUSY";
    }
    snprintf(
        buf,
        sizeof(buf),
        "L%u P%02u D%u %05lu%s",
        (unsigned)g->lives,
        (unsigned)g->pawns_left,
        (unsigned)g->dynamite,
        (unsigned long)g->score,
        flag);
    draw_text(packed, 2, ICE_VIEW_H + 2, buf, GrayLevelBlack, 1);
}

/* Six 43x31 panels at x = 0, 44, 88 and y = 0, 33, in outcome order. */
static void draw_comic(uint8_t* packed, IceOutcome outcome) {
    int sel = (int)outcome;
    int sx0 = (sel % 3) * 44;
    int sy0 = (sel / 3) == 0 ? 0 : 33;
    int sh = (sel / 3) == 0 ? 32 : 31;

    draw_picture(packed, ice_pic_comic);
    for(int y = 0; y < GRAY2BIT_SCREEN_H; y++) {
        for(int x = 0; x < GRAY2BIT_SCREEN_W; x++) {
            bool inside = x >= sx0 && x < sx0 + 43 && y >= sy0 && y < sy0 + sh;
            if(inside) {
                continue;
            }
            uint8_t level = get_px(packed, x, y);
            if(level < GrayLevelWhite) {
                put_px(packed, x, y, (uint8_t)(level + 1));
            }
        }
    }
    for(int i = 0; i < 2; i++) {
        for(int x = sx0; x < sx0 + 43; x++) {
            put_px(packed, x, sy0 + i, GrayLevelBlack);
            put_px(packed, x, sy0 + sh - 1 - i, GrayLevelBlack);
        }
        for(int y = sy0; y < sy0 + sh; y++) {
            put_px(packed, sx0 + i, y, GrayLevelBlack);
            put_px(packed, sx0 + 42 - i, y, GrayLevelBlack);
        }
    }
}

static void draw_route(uint8_t* packed, const IceGame* g) {
    char line[8];
    draw_picture(packed, ice_pic_route);
    draw_centered(packed, 1, g->name, GrayLevelBlack, 1);
    snprintf(line, sizeof(line), "%u", (unsigned)g->lives);
    draw_text(packed, 64, 47, line, GrayLevelBlack, 1);
}

void ice_render(const IceGame* g, uint8_t* packed) {
    char line[28];

    memset(packed, 0xFF, GRAY2BIT_PACKED_SIZE);

    switch(g->phase) {
    case IcePhasePlay:
        draw_world(packed, g);
        draw_hud(packed, g);
        return;
    case IcePhasePause:
        draw_picture(packed, ice_pic_pause);
        return;
    case IcePhaseRoute:
        draw_route(packed, g);
        return;
    case IcePhaseComic:
        draw_comic(packed, g->outcome);
        return;
    case IcePhaseTitle:
        draw_picture(packed, ice_pic_title);
        fill_rect(packed, 92, 56, 36, 8, GrayLevelWhite);
        draw_text(packed, 94, 58, "OK PLAY", GrayLevelBlack, 1);
        return;
    case IcePhaseWin:
        draw_centered(packed, 12, "ALL CLEAR", GrayLevelBlack, 2);
        snprintf(line, sizeof(line), "SCORE %lu", (unsigned long)g->score);
        draw_centered(packed, 34, line, GrayLevelBlack, 1);
        draw_centered(packed, 50, "OK AGAIN", GrayLevelBlack, 1);
        return;
    default:
        draw_centered(packed, 12, "GAME OVER", GrayLevelBlack, 2);
        snprintf(line, sizeof(line), "SCORE %lu", (unsigned long)g->score);
        draw_centered(packed, 34, line, GrayLevelBlack, 1);
        draw_centered(packed, 50, "OK RETRY", GrayLevelBlack, 1);
        return;
    }
}
