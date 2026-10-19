#include "game.h"

#include <math.h>
#include <string.h>

#define BABAL_VZ_MIN             0.05f
#define BABAL_VZ_MAX             0.45f
#define BABAL_VZ_ACCEL           0.012f
#define BABAL_VZ_BRAKE           0.018f
#define BABAL_VX_ACCEL           0.08f
#define BABAL_VX_FRICTION        0.04f
#define BABAL_VX_MAX             0.35f
#define BABAL_JUMP_IMPULSE       0.22f
#define BABAL_GRAVITY            0.018f
#define BABAL_BOUNCE_RESTITUTION 0.55f

void babal_game_init(BabalGame* g) {
    memset(g, 0, sizeof(*g));
    g->phase = BabalPhaseTitle;
    g->level_index = 0;
    g->level = babal_level_get(0);
    g->lane = 4.0f;
    g->vz = 0.15f;
    g->best_ms = 0;
    g->session_best_ms = 0;
}

void babal_game_start(BabalGame* g, uint8_t level_index) {
    g->level_index = level_index;
    g->level = babal_level_get(level_index);
    g->phase = BabalPhasePlaying;
    g->progress = 0.0f;
    g->lane = 4.0f;
    g->height = 0.0f;
    g->vz = 0.15f;
    g->vx = 0.0f;
    g->vy = 0.0f;
    g->ticks = 0;
    g->elapsed_ms = 0;
    g->crash_flash = 0;
    g->key_left = g->key_right = g->key_up = g->key_down = false;
}

void babal_game_set_keys(BabalGame* g, bool left, bool right, bool up, bool down) {
    g->key_left = left;
    g->key_right = right;
    g->key_up = up;
    g->key_down = down;
}

void babal_game_jump(BabalGame* g) {
    if(g->phase != BabalPhasePlaying) {
        return;
    }
    if(g->height <= 0.001f) {
        g->height = 0.0f;
        g->vy = BABAL_JUMP_IMPULSE;
    }
}

void babal_game_next_level(BabalGame* g) {
    uint8_t n = babal_level_count();
    g->level_index = (uint8_t)((g->level_index + 1) % n);
    g->level = babal_level_get(g->level_index);
}

void babal_game_prev_level(BabalGame* g) {
    uint8_t n = babal_level_count();
    g->level_index = (uint8_t)((g->level_index + n - 1) % n);
    g->level = babal_level_get(g->level_index);
}

void babal_game_toggle_invert(BabalGame* g) {
    g->invert = !g->invert;
}

void babal_game_toggle_bounce(BabalGame* g) {
    g->bounce = !g->bounce;
}

static void babal_apply_rewind(BabalGame* g) {
    g->progress -= (float)BABAL_REWIND_PLATFORMS;
    if(g->progress < 0.0f) {
        g->progress = 0.0f;
    }
    g->height = 0.0f;
    g->vy = 0.0f;
    g->vx = 0.0f;
    g->crash_flash = 12;
}

void babal_game_tick(BabalGame* g, uint32_t dt_ms) {
    if(g->phase != BabalPhasePlaying) {
        return;
    }

    g->ticks++;
    g->elapsed_ms += dt_ms;
    if(g->crash_flash > 0) {
        g->crash_flash--;
    }

    /* Speed */
    if(g->key_up) {
        g->vz += BABAL_VZ_ACCEL;
    }
    if(g->key_down) {
        g->vz -= BABAL_VZ_BRAKE;
    }
    if(g->vz < BABAL_VZ_MIN) {
        g->vz = BABAL_VZ_MIN;
    }
    if(g->vz > BABAL_VZ_MAX) {
        g->vz = BABAL_VZ_MAX;
    }

    /* Lateral */
    if(g->key_left && !g->key_right) {
        g->vx -= BABAL_VX_ACCEL;
    } else if(g->key_right && !g->key_left) {
        g->vx += BABAL_VX_ACCEL;
    } else {
        if(g->vx > BABAL_VX_FRICTION) {
            g->vx -= BABAL_VX_FRICTION;
        } else if(g->vx < -BABAL_VX_FRICTION) {
            g->vx += BABAL_VX_FRICTION;
        } else {
            g->vx = 0.0f;
        }
    }
    if(g->vx > BABAL_VX_MAX) {
        g->vx = BABAL_VX_MAX;
    }
    if(g->vx < -BABAL_VX_MAX) {
        g->vx = -BABAL_VX_MAX;
    }

    g->lane += g->vx;
    g->progress += g->vz;

    /* Vertical */
    g->vy -= BABAL_GRAVITY;
    g->height += g->vy;
    if(g->height <= 0.0f) {
        g->height = 0.0f;
        if(g->bounce && g->vy < -0.05f) {
            g->vy = -g->vy * BABAL_BOUNCE_RESTITUTION;
            if(g->vy < 0.04f) {
                g->vy = 0.0f;
            }
        } else {
            g->vy = 0.0f;
        }
    }

    /* Finish */
    if(g->progress >= (float)g->level->length - 1.0f) {
        g->progress = (float)g->level->length - 1.0f;
        g->phase = BabalPhaseFinished;
        if(g->session_best_ms == 0 || g->elapsed_ms < g->session_best_ms) {
            g->session_best_ms = g->elapsed_ms;
        }
        if(g->best_ms == 0 || g->elapsed_ms < g->best_ms) {
            g->best_ms = g->elapsed_ms;
        }
        return;
    }

    /* Fall off sides or holes while on ground */
    if(g->height <= 0.001f) {
        if(g->lane < 0.0f || g->lane >= (float)BABAL_LEVEL_WIDTH) {
            babal_apply_rewind(g);
            g->lane = 4.0f;
            return;
        }
        if(!babal_level_is_safe(g->level, g->progress, g->lane)) {
            babal_apply_rewind(g);
        }
    }
}

void babal_game_render_soft(BabalGame* g) {
    BabalLevelView view = {
        .rows = g->level->rows,
        .length = g->level->length,
    };
    /* lane_shift: 0 when lane==4 (center). */
    float shift = g->lane - 4.0f;
    int16_t ball_h = (int16_t)lroundf(g->height * 40.0f);
    if(ball_h < 0) {
        ball_h = 0;
    }
    if(ball_h > 20) {
        ball_h = 20;
    }
    bool show_ball = (g->crash_flash % 2 == 0);
    g->cells_drawn = babal_render_frame(
        &g->frame,
        &view,
        BABAL_Q16_FROM_FLOAT(g->progress),
        BABAL_Q16_FROM_FLOAT(shift),
        ball_h,
        show_ball && g->phase == BabalPhasePlaying,
        g->invert);
}
