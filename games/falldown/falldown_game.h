#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FALLDOWN_SCREEN_W 128
#define FALLDOWN_SCREEN_H 64
#define FALLDOWN_BALL_R 3
#define FALLDOWN_PLATFORM_H 3
#define FALLDOWN_PLATFORM_COUNT 6
#define FALLDOWN_PLATFORM_SPACING 18
#define FALLDOWN_GAP_MIN 22
#define FALLDOWN_GAP_MAX 38

typedef enum {
    FalldownPhaseTitle,
    FalldownPhasePlaying,
    FalldownPhaseGameOver,
} FalldownPhase;

typedef struct {
    float y;
    int16_t gap_x;
    int16_t gap_w;
    bool scored;
} FalldownPlatform;

typedef struct {
    float ball_x;
    float ball_y;
    float ball_vy;
    int8_t move_dir;
    FalldownPlatform platforms[FALLDOWN_PLATFORM_COUNT];
    uint32_t score;
    uint32_t high_score;
    float scroll_speed;
    FalldownPhase phase;
} FalldownState;

void falldown_game_init(FalldownState* state, uint32_t high_score);
void falldown_game_start(FalldownState* state);
void falldown_game_tick(FalldownState* state);
void falldown_game_set_move_dir(FalldownState* state, int8_t dir);
