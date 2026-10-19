#include "falldown_game.h"

#include <stdlib.h>
#include <furi.h>

#define BALL_SPEED_X 2.2f
#define GRAVITY 0.28f
#define MAX_FALL_SPEED 3.2f
#define BASE_SCROLL 0.55f
#define MAX_SCROLL 2.4f
#define SCROLL_PER_SCORE 0.035f

static int16_t falldown_rand_range(int16_t min, int16_t max) {
    if(max <= min) {
        return min;
    }
    return (int16_t)(min + rand() % (max - min + 1));
}

static void falldown_spawn_platform(FalldownPlatform* p, float y) {
    p->y = y;
    p->gap_w = falldown_rand_range(FALLDOWN_GAP_MIN, FALLDOWN_GAP_MAX);
    p->gap_x = falldown_rand_range(0, FALLDOWN_SCREEN_W - p->gap_w);
    p->scored = false;
}

static void falldown_reset_round(FalldownState* state) {
    state->ball_x = FALLDOWN_SCREEN_W / 2.0f;
    state->ball_y = 12.0f;
    state->ball_vy = 0.0f;
    state->move_dir = 0;
    state->score = 0;
    state->scroll_speed = BASE_SCROLL;

    float y = 28.0f;
    for(uint8_t i = 0; i < FALLDOWN_PLATFORM_COUNT; i++) {
        falldown_spawn_platform(&state->platforms[i], y);
        y += FALLDOWN_PLATFORM_SPACING;
    }
}

void falldown_game_init(FalldownState* state, uint32_t high_score) {
    furi_check(state);
    state->high_score = high_score;
    state->phase = FalldownPhaseTitle;
    falldown_reset_round(state);
}

void falldown_game_start(FalldownState* state) {
    furi_check(state);
    falldown_reset_round(state);
    state->phase = FalldownPhasePlaying;
}

void falldown_game_set_move_dir(FalldownState* state, int8_t dir) {
    furi_check(state);
    state->move_dir = dir;
}

static bool falldown_ball_in_gap(const FalldownState* state, const FalldownPlatform* p) {
    float left = state->ball_x - FALLDOWN_BALL_R;
    float right = state->ball_x + FALLDOWN_BALL_R;
    return left >= p->gap_x && right <= (p->gap_x + p->gap_w);
}

static void falldown_game_over(FalldownState* state) {
    state->phase = FalldownPhaseGameOver;
    if(state->score > state->high_score) {
        state->high_score = state->score;
    }
}

void falldown_game_tick(FalldownState* state) {
    furi_check(state);

    if(state->phase != FalldownPhasePlaying) {
        return;
    }

    state->ball_x += state->move_dir * BALL_SPEED_X;
    if(state->ball_x < FALLDOWN_BALL_R) {
        state->ball_x = FALLDOWN_BALL_R;
    } else if(state->ball_x > FALLDOWN_SCREEN_W - FALLDOWN_BALL_R) {
        state->ball_x = FALLDOWN_SCREEN_W - FALLDOWN_BALL_R;
    }

    state->ball_vy += GRAVITY;
    if(state->ball_vy > MAX_FALL_SPEED) {
        state->ball_vy = MAX_FALL_SPEED;
    }
    state->ball_y += state->ball_vy;

    bool on_platform = false;

    for(uint8_t i = 0; i < FALLDOWN_PLATFORM_COUNT; i++) {
        FalldownPlatform* p = &state->platforms[i];
        float top = p->y;
        float bottom = p->y + FALLDOWN_PLATFORM_H;
        float ball_bottom = state->ball_y + FALLDOWN_BALL_R;
        float ball_top = state->ball_y - FALLDOWN_BALL_R;

        if(state->ball_vy >= 0.0f && ball_bottom >= top && ball_top < bottom &&
           !falldown_ball_in_gap(state, p)) {
            state->ball_y = top - FALLDOWN_BALL_R;
            state->ball_vy = 0.0f;
            on_platform = true;
        }
    }

    state->scroll_speed = BASE_SCROLL + state->score * SCROLL_PER_SCORE;
    if(state->scroll_speed > MAX_SCROLL) {
        state->scroll_speed = MAX_SCROLL;
    }

    for(uint8_t i = 0; i < FALLDOWN_PLATFORM_COUNT; i++) {
        FalldownPlatform* p = &state->platforms[i];
        p->y -= state->scroll_speed;

        if(!p->scored && (p->y + FALLDOWN_PLATFORM_H) <
                             (state->ball_y - FALLDOWN_BALL_R)) {
            p->scored = true;
            state->score++;
        }

        if(p->y < -FALLDOWN_PLATFORM_H) {
            float lowest = -FALLDOWN_PLATFORM_H;
            for(uint8_t j = 0; j < FALLDOWN_PLATFORM_COUNT; j++) {
                if(j == i) {
                    continue;
                }
                if(state->platforms[j].y > lowest) {
                    lowest = state->platforms[j].y;
                }
            }
            float next_y = lowest + FALLDOWN_PLATFORM_SPACING;
            if(next_y < FALLDOWN_SCREEN_H + 4) {
                next_y = FALLDOWN_SCREEN_H + 4;
            }
            falldown_spawn_platform(p, next_y);
        }
    }

    if(on_platform) {
        state->ball_y -= state->scroll_speed;
    }

    if(state->ball_y - FALLDOWN_BALL_R <= 0.0f) {
        state->ball_y = FALLDOWN_BALL_R;
        falldown_game_over(state);
        return;
    }

    if(state->ball_y + FALLDOWN_BALL_R > FALLDOWN_SCREEN_H) {
        state->ball_y = FALLDOWN_SCREEN_H - FALLDOWN_BALL_R;
    }
}
