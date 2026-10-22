#pragma once

#include "level.h"
#include "levels/levels_data.h"

#include <stdbool.h>
#include <stdint.h>

#define ICE_TILE          8
#define ICE_CUBE          8 /* drawn size */
#define ICE_BOX           6 /* collision box, inset by 1 */
#define ICE_VIEW_W        128
#define ICE_VIEW_H        64 /* the map uses the whole screen; the HUD is an overlay */
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
#define ICE_HURRY_SEC     20 /* the original lit BUSY below this */

/* Camera lead: the view shifts toward the direction of travel by this many
 * pixels per unit of speed, moving ICE_LEAD_STEP pixels per tick. */
#define ICE_LEAD_X    8
#define ICE_LEAD_Y    5
#define ICE_LEAD_STEP 2

/* HUD overlay box: "L9 EXIT D9" in the 3x5 font plus a border and padding. */
#define ICE_HUD_W      43
#define ICE_HUD_H      9
#define ICE_HUD_MARGIN 12 /* move the box when the cube gets this close */

typedef enum {
    IceDirLeft = 0,
    IceDirRight,
    IceDirUp,
    IceDirDown,
} IceDir;

typedef enum {
    IcePhaseTitle = 0,
    IcePhaseRoute, /* route picture and START / CONTINUE / QUIT menu before a stage */
    IcePhasePlay,
    IcePhasePause,
    IcePhaseComic, /* end-of-stage picture */
    IcePhaseGameOver,
    IcePhaseWin,
} IcePhase;

typedef enum {
    IceRouteStart = 0,
    IceRouteContinue,
    IceRouteQuit,
    IceRouteCount,
} IceRouteSel;

/* One-shot events for sound, vibration and LED; read with ice_game_take_events(). */
typedef enum {
    IceEventPawn = 1 << 0,
    IceEventDynamite = 1 << 1,
    IceEventBlast = 1 << 2,
    IceEventDeath = 1 << 3,
    IceEventCleared = 1 << 4,
    IceEventExtraLife = 1 << 5,
    IceEventHurry = 1 << 6,
    IceEventExitOpen = 1 << 7,
} IceEventBit;

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
    int16_t spawn_cell;
    int16_t px;
    int16_t py;
    int8_t vx;
    int8_t vy;
    bool key_left;
    bool key_right;
    bool key_up;
    bool key_down;
    uint8_t taps; /* IceDir bits pressed since the last tick, so short taps count */
    uint8_t held; /* IceDir bits held or tapped during the current tick */

    int16_t time_cell; /* index of the first timer digit cell, or -1 */
    uint8_t key; /* cube state 0..3 set by 0x38..0x3A; also picks the cube sprite */
    uint8_t dynamite;
    uint8_t pawns_left;
    uint8_t lives;
    uint32_t score;
    uint32_t next_life_score;
    uint16_t time_ticks;
    uint8_t blast;
    uint8_t anim;

    bool can_continue; /* the route menu offers CONTINUE */
    uint8_t route_sel; /* IceRouteSel */
    int16_t lead_x;
    int16_t lead_y;
    uint8_t hud_corner; /* bit 0 = right, bit 1 = bottom */
    uint16_t events; /* IceEventBit */
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
void ice_game_resume(IceGame* g);
void ice_game_tick(IceGame* g);
void ice_game_ack(IceGame* g);
void ice_game_route_move(IceGame* g, int delta);
bool ice_game_route_available(const IceGame* g, IceRouteSel sel);
uint16_t ice_game_take_events(IceGame* g);
uint16_t ice_time_seconds(const IceGame* g);
void ice_game_camera(const IceGame* g, int16_t* cam_x, int16_t* cam_y);
void ice_game_update_view(IceGame* g);
void ice_game_hud_rect(const IceGame* g, int16_t* x, int16_t* y, int16_t* w, int16_t* h);
