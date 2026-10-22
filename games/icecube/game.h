#pragma once

#include "level.h"
#include "levels/levels_data.h"

#include <stdbool.h>
#include <stdint.h>

#define ICE_TILE          8
#define ICE_CUBE          8 /* drawn size */
#define ICE_BOX           6 /* collision box, inset by 1 */
#define ICE_VIEW_W        128
#define ICE_VIEW_H        56
#define ICE_TICKS_PER_SEC 20
#define ICE_START_LIVES   3
#define ICE_MAX_LIVES     9
#define ICE_MAX_SPEED     3
#define ICE_PAWN_SCORE    100
#define ICE_CLEAR_SCORE   200
#define ICE_LIFE_EVERY    1000
#define ICE_TIME_BONUS    10
#define ICE_MAX_DYNAMITE  9
#define ICE_BLAST_TICKS   4
#define ICE_ANIM_TICKS    8

typedef enum {
    IceDirLeft = 0,
    IceDirRight,
    IceDirUp,
    IceDirDown,
} IceDir;

typedef enum {
    IcePhaseTitle = 0,
    IcePhaseRoute, /* route picture before a stage */
    IcePhasePlay,
    IcePhasePause,
    IcePhaseComic, /* end-of-stage picture */
    IcePhaseGameOver,
    IcePhaseWin,
} IcePhase;

typedef struct {
    IcePhase phase;
    IceOutcome outcome;
    uint8_t stage;
    bool custom;
    const char* name;
    const uint8_t* src;
    uint8_t width;
    uint8_t height;
    uint16_t time_sec;

    uint8_t map[ICE_MAX_CELLS];
    int16_t px;
    int16_t py;
    int8_t vx;
    int8_t vy;
    bool key_left;
    bool key_right;
    bool key_up;
    bool key_down;
    uint8_t taps; /* IceDir bits pressed since the last tick, so short taps count */

    int16_t time_cell; /* index of the first timer digit cell, or -1 */
    uint8_t key; /* cube state 0..3 set by 0x38..0x3A; also picks the cube sprite */
    uint8_t dynamite;
    uint8_t pawns_left;
    uint8_t pawns_total;
    uint8_t lives;
    uint32_t score;
    uint32_t next_life_score;
    uint16_t time_ticks;
    uint16_t done_mask;
    uint8_t blast;
    uint8_t anim;
} IceGame;

void ice_game_init(IceGame* g);
void ice_game_begin(IceGame* g);
void ice_game_load_stage(IceGame* g, uint8_t index);
void ice_game_load_raw(
    IceGame* g,
    const uint8_t* cells,
    uint8_t width,
    uint8_t height,
    uint16_t time_sec);
void ice_game_start(IceGame* g);
void ice_game_set_keys(IceGame* g, bool left, bool right, bool up, bool down);
void ice_game_key(IceGame* g, IceDir dir, bool down);
void ice_game_set_velocity(IceGame* g, int8_t vx, int8_t vy);
void ice_game_detonate(IceGame* g);
void ice_game_quit_stage(IceGame* g);
void ice_game_tick(IceGame* g);
void ice_game_ack(IceGame* g);
uint16_t ice_time_seconds(const IceGame* g);
void ice_game_camera(const IceGame* g, int16_t* cam_x, int16_t* cam_y);
