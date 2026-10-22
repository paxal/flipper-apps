/* Host tool: render the game's screens to PNG files for eyeballing.
 * Usage: ./render_frames <out_dir>
 */
#include "game.h"
#include "helpers/gray2bit.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static void crc_init(void) {
    for(uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for(int k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
}

static uint32_t crc(const uint8_t* buf, size_t len, uint32_t c) {
    c ^= 0xFFFFFFFFu;
    for(size_t i = 0; i < len; i++) {
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static void put32(FILE* f, uint32_t v) {
    fputc((v >> 24) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc(v & 0xFF, f);
}

static void chunk(FILE* f, const char* tag, const uint8_t* data, size_t len) {
    uint32_t c;
    put32(f, (uint32_t)len);
    fwrite(tag, 1, 4, f);
    if(len) {
        fwrite(data, 1, len, f);
    }
    c = crc((const uint8_t*)tag, 4, 0);
    c = crc(data, len, c);
    put32(f, c);
}

/* 8-bit grayscale PNG, zlib stream made of stored deflate blocks. */
static void write_png(const char* path, int w, int h, const uint8_t* gray, int scale) {
    int sw = w * scale;
    int sh = h * scale;
    size_t row_len = (size_t)sw + 1;
    size_t raw_len = row_len * (size_t)sh;
    uint8_t* raw = malloc(raw_len);
    size_t blocks = (raw_len + 65534) / 65535;
    size_t zlen = 2 + raw_len + blocks * 5 + 4;
    uint8_t* z = malloc(zlen);
    size_t zi = 0;
    uint32_t a = 1;
    uint32_t b = 0;
    uint8_t ihdr[13];
    FILE* f;

    for(int y = 0; y < sh; y++) {
        raw[(size_t)y * row_len] = 0;
        for(int x = 0; x < sw; x++) {
            raw[(size_t)y * row_len + 1 + (size_t)x] =
                gray[(size_t)(y / scale) * (size_t)w + (size_t)(x / scale)];
        }
    }
    for(size_t i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    z[zi++] = 0x78;
    z[zi++] = 0x01;
    for(size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off;
        if(n > 65535) {
            n = 65535;
        }
        z[zi++] = (off + n == raw_len) ? 1 : 0;
        z[zi++] = (uint8_t)(n & 0xFF);
        z[zi++] = (uint8_t)(n >> 8);
        z[zi++] = (uint8_t)(~n & 0xFF);
        z[zi++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + zi, raw + off, n);
        zi += n;
    }
    z[zi++] = (uint8_t)((b << 16 >> 24) & 0xFF);
    z[zi++] = (uint8_t)(b & 0xFF);
    z[zi++] = (uint8_t)((a >> 8) & 0xFF);
    z[zi++] = (uint8_t)(a & 0xFF);

    f = fopen(path, "wb");
    if(f == NULL) {
        perror(path);
        exit(1);
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    ihdr[0] = (uint8_t)(sw >> 24);
    ihdr[1] = (uint8_t)(sw >> 16);
    ihdr[2] = (uint8_t)(sw >> 8);
    ihdr[3] = (uint8_t)sw;
    ihdr[4] = (uint8_t)(sh >> 24);
    ihdr[5] = (uint8_t)(sh >> 16);
    ihdr[6] = (uint8_t)(sh >> 8);
    ihdr[7] = (uint8_t)sh;
    ihdr[8] = 8;
    ihdr[9] = 0;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zi);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

/* Screens without gray are shown thresholded on the device; do the same here. */
static void save(const char* dir, const char* name, const uint8_t* packed, IcePhase phase) {
    static const uint8_t gray_of[4] = {0, 85, 170, 255};
    uint8_t gray[GRAY2BIT_SCREEN_W * GRAY2BIT_SCREEN_H];
    bool levels = ice_phase_uses_gray(phase);
    char path[512];
    for(int y = 0; y < GRAY2BIT_SCREEN_H; y++) {
        for(int x = 0; x < GRAY2BIT_SCREEN_W; x++) {
            uint8_t level = gray2bit_get(packed, GRAY2BIT_SCREEN_W, (uint16_t)x, (uint16_t)y);
            if(!levels) {
                level = level < GrayLevelLight ? GrayLevelBlack : GrayLevelWhite;
            }
            gray[y * GRAY2BIT_SCREEN_W + x] = gray_of[level];
        }
    }
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    write_png(path, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H, gray, 3);
    printf("wrote %s\n", path);
}

static void shot(const char* dir, const char* name, const IceGame* game, uint8_t* packed) {
    ice_render(game, packed);
    save(dir, name, packed, game->phase);
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    static uint8_t packed[GRAY2BIT_PACKED_SIZE];
    static IceGame game;
    static const char* const corner_names[4] = {"tl", "tr", "bl", "br"};
    char name[48];

    crc_init();
    ice_game_init(&game);
    shot(dir, "title", &game, packed);

    ice_game_begin(&game);
    shot(dir, "route", &game, packed);
    ice_game_route_move(&game, 1);
    shot(dir, "route_sel_quit", &game, packed);
    ice_game_route_move(&game, 1);

    ice_game_ack(&game);
    shot(dir, "play_stage01", &game, packed);

    for(int i = 0; i < 60; i++) {
        ice_game_set_keys(&game, false, true, false, false);
        ice_game_tick(&game);
    }
    shot(dir, "play_stage01_moved", &game, packed);

    game.phase = IcePhasePause;
    shot(dir, "pause", &game, packed);

    /* A quit leads to the route menu with CONTINUE selected. */
    ice_game_quit_stage(&game);
    ice_game_ack(&game);
    shot(dir, "route_continue", &game, packed);

    for(int o = 0; o < 6; o++) {
        game.phase = IcePhaseComic;
        game.outcome = (IceOutcome)o;
        snprintf(name, sizeof(name), "comic_%d", o);
        shot(dir, name, &game, packed);
    }

    for(uint8_t s = 1; s < ICE_STAGE_COUNT; s++) {
        ice_game_load_stage(&game, s);
        snprintf(name, sizeof(name), "route_stage%02u", (unsigned)s + 1);
        if(s == 4 || s == 11) {
            shot(dir, name, &game, packed); /* passed-stage marks */
        }
        ice_game_start(&game);
        snprintf(name, sizeof(name), "play_stage%02u", (unsigned)s + 1);
        shot(dir, name, &game, packed);
    }

    /* The HUD box in each corner, with the cube pushed into the opposite one. */
    ice_game_load_stage(&game, 6);
    ice_game_start(&game);
    for(uint8_t c = 0; c < 4; c++) {
        game.px = (int16_t)((c & 1) ? ICE_TILE : 38 * ICE_TILE);
        game.py = (int16_t)((c & 2) ? ICE_TILE : 30 * ICE_TILE);
        game.hud_corner = (uint8_t)(3 - c);
        ice_game_update_view(&game);
        snprintf(name, sizeof(name), "hud_%s", corner_names[game.hud_corner]);
        shot(dir, name, &game, packed);
    }

    /* Under ICE_HURRY_SEC the box blinks inverted; all pawns taken shows EXIT. */
    game.time_ticks = 10 * ICE_TICKS_PER_SEC;
    game.pawns_left = 0;
    game.dynamite = 2;
    shot(dir, "hud_hurry_exit", &game, packed);

    game.phase = IcePhaseGameOver;
    shot(dir, "game_over", &game, packed);
    game.phase = IcePhaseWin;
    shot(dir, "win", &game, packed);
    return 0;
}
