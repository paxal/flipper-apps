#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "level.h"
#include "render_math.h"

typedef enum {
    BabalPhaseTitle = 0,
    BabalPhasePlaying,
    BabalPhaseFinished,
    BabalPhaseAbout,
    BabalPhaseSetup,
} BabalPhase;

typedef struct {
    BabalPhase phase;
    const BabalLevelDef* level;
    uint8_t level_index;

    float progress; /* depth along level */
    float lane; /* 0..8 continuous; ball center */
    float height; /* jump height (world up); 0 on ground */
    float vz; /* forward speed */
    float vx; /* lateral speed */
    float vy; /* vertical speed */

    bool key_left;
    bool key_right;
    bool key_up;
    bool key_down;

    uint32_t ticks;
    uint32_t elapsed_ms;
    uint32_t best_ms; /* 0 = none */
    uint32_t session_best_ms;

    uint16_t crash_flash;

    bool invert;
    bool bounce;

    /* Soft render snapshot for host tests / HUD blit path. */
    BabalSoftFb frame;
    uint16_t cells_drawn;
} BabalGame;

void babal_game_init(BabalGame* g);
void babal_game_start(BabalGame* g, uint8_t level_index);
void babal_game_set_keys(BabalGame* g, bool left, bool right, bool up, bool down);
void babal_game_jump(BabalGame* g);
void babal_game_tick(BabalGame* g, uint32_t dt_ms);
void babal_game_render_soft(BabalGame* g);

void babal_game_next_level(BabalGame* g);
void babal_game_prev_level(BabalGame* g);
void babal_game_toggle_invert(BabalGame* g);
void babal_game_toggle_bounce(BabalGame* g);
