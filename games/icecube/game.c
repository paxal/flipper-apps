#include "game.h"

#include <string.h>

static int8_t clamp_speed(int value) {
    if(value > ICE_MAX_SPEED) {
        return ICE_MAX_SPEED;
    }
    if(value < -ICE_MAX_SPEED) {
        return -ICE_MAX_SPEED;
    }
    return (int8_t)value;
}

static void award(IceGame* g, uint32_t points) {
    g->score += points;
    if(g->next_life_score == 0) {
        g->next_life_score = ICE_LIFE_EVERY;
    }
    while(g->score >= g->next_life_score) {
        if(g->lives < ICE_MAX_LIVES) {
            g->lives++;
        }
        g->next_life_score += ICE_LIFE_EVERY;
    }
}

static uint8_t* cell_at(IceGame* g, int tx, int ty) {
    return &g->map[(uint16_t)ty * g->width + (uint16_t)tx];
}

typedef enum {
    HitNone = 0,
    HitStop,
    HitBounce,
    HitDead,
} Hit;

/* Probe the collision box at pixel (x, y); *deadly receives the tile that kills. */
static Hit probe_box(IceGame* g, int x, int y, uint8_t* deadly) {
    static const int ox[4] = {0, ICE_BOX - 1, 0, ICE_BOX - 1};
    static const int oy[4] = {0, 0, ICE_BOX - 1, ICE_BOX - 1};
    Hit hit = HitNone;

    if(x < 0 || y < 0 || x + ICE_BOX > g->width * ICE_TILE || y + ICE_BOX > g->height * ICE_TILE) {
        return HitStop;
    }

    for(int i = 0; i < 4; i++) {
        int tx = (x + ox[i]) / ICE_TILE;
        int ty = (y + oy[i]) / ICE_TILE;
        uint8_t cell = *cell_at(g, tx, ty);
        IceTileKind kind = ice_tile_kind(cell);
        if(ice_tile_deadly(cell)) {
            if(deadly != NULL) {
                *deadly = cell;
            }
            return HitDead;
        }
        if(kind == IceTileWall) {
            hit = HitBounce;
        } else if(kind == IceTileBlock) {
            return HitStop;
        } else if(kind == IceTileExit && g->pawns_left > 0) {
            return HitStop;
        }
    }
    return hit;
}

static void end_stage(IceGame* g, IceOutcome outcome) {
    g->outcome = outcome;
    g->vx = 0;
    g->vy = 0;
    if(outcome == IceOutcomeCleared) {
        if(!g->custom && g->stage < ICE_STAGE_COUNT) {
            g->done_mask = (uint16_t)(g->done_mask | (uint16_t)(1u << g->stage));
        }
        award(g, ICE_CLEAR_SCORE + (uint32_t)ice_time_seconds(g) * ICE_TIME_BONUS);
    } else if(g->lives > 0) {
        g->lives--;
    }
    g->phase = IcePhaseComic;
}

static void reload(IceGame* g) {
    uint16_t cells = (uint16_t)g->width * g->height;
    int spawn = -1;

    memcpy(g->map, g->src, cells);

    g->pawns_left = 0;
    g->time_cell = -1;
    for(uint16_t i = 0; i < cells; i++) {
        uint8_t raw = g->map[i];
        if(raw == ICE_TILE_PAWN) {
            g->pawns_left++;
        } else if(raw == ICE_TILE_SPAWN) {
            spawn = (int)i;
        } else if(raw == ICE_TILE_TIME && i + 3 < cells) {
            g->time_cell = (int16_t)(i + 1);
        }
    }
    g->pawns_total = g->pawns_left;

    if(spawn < 0) {
        for(uint16_t i = 0; i < cells; i++) {
            if(ice_tile_kind(g->map[i]) == IceTileFloor) {
                spawn = (int)i;
                break;
            }
        }
    }
    if(spawn < 0) {
        spawn = 0;
    }
    g->px = (int16_t)((spawn % g->width) * ICE_TILE);
    g->py = (int16_t)((spawn / g->width) * ICE_TILE);
    g->vx = 0;
    g->vy = 0;
    g->key = 0;
    g->dynamite = 0;
    g->blast = 0;
    g->anim = 0;
    g->time_ticks = (uint16_t)(g->time_sec * ICE_TICKS_PER_SEC);
    g->phase = IcePhaseRoute;
}

static void write_timer_digits(IceGame* g) {
    unsigned seconds = ice_time_seconds(g);
    if(g->time_cell < 0) {
        return;
    }
    if(seconds > 999) {
        seconds = 999;
    }
    g->map[g->time_cell] = (uint8_t)(ICE_TILE_DIGIT0 + seconds / 100);
    g->map[g->time_cell + 1] = (uint8_t)(ICE_TILE_DIGIT0 + (seconds / 10) % 10);
    g->map[g->time_cell + 2] = (uint8_t)(ICE_TILE_DIGIT0 + seconds % 10);
}

/* Every ICE_ANIM_TICKS the engine steps the low nibble of each bit-7 byte
 * within its group of 16; the worm's lethal 0x8A..0x8F phase travels along it. */
static void animate_tiles(IceGame* g) {
    uint16_t cells = (uint16_t)g->width * g->height;
    g->anim = (uint8_t)((g->anim + 1) % ICE_ANIM_TICKS);
    if(g->anim != 0) {
        return;
    }
    for(uint16_t i = 0; i < cells; i++) {
        uint8_t raw = g->map[i];
        if(raw & 0x80) {
            g->map[i] = (uint8_t)((raw & 0xF0) | ((raw + 1) & 0x0F));
        }
    }
}

/* Returns true when the stage ended. */
static bool move_axis(IceGame* g, bool horizontal) {
    int8_t vel = horizontal ? g->vx : g->vy;
    int step;
    int steps;

    if(vel == 0) {
        return false;
    }
    step = vel > 0 ? 1 : -1;
    steps = vel > 0 ? vel : -vel;

    for(int i = 0; i < steps; i++) {
        int nx = g->px + 1 + (horizontal ? step : 0);
        int ny = g->py + 1 + (horizontal ? 0 : step);
        uint8_t deadly = 0;
        Hit hit = probe_box(g, nx, ny, &deadly);
        if(hit == HitDead) {
            end_stage(g, ice_tile_outcome(deadly));
            return true;
        }
        if(hit == HitStop) {
            if(horizontal) {
                g->vx = 0;
            } else {
                g->vy = 0;
            }
            return false;
        }
        if(hit == HitBounce) {
            /* The engine negates the velocity (code_00041 L00C4A). A bounce here
             * also loses one speed unit so the cube settles instead of
             * ping-ponging, and pushing into the wall just holds it still. */
            bool pushing = horizontal ? (step > 0 ? g->key_right : g->key_left) :
                                        (step > 0 ? g->key_down : g->key_up);
            int8_t rebound = pushing ? 0 : (int8_t)(-step * (steps - 1));
            if(horizontal) {
                g->vx = rebound;
            } else {
                g->vy = rebound;
            }
            return false;
        }
        g->px = (int16_t)(nx - 1);
        g->py = (int16_t)(ny - 1);
    }
    return false;
}

/* Tile under the cube centre: pickups, exit, pushers, switches. Returns true when the stage ended. */
static bool touch_center(IceGame* g) {
    int tx = (g->px + ICE_CUBE / 2) / ICE_TILE;
    int ty = (g->py + ICE_CUBE / 2) / ICE_TILE;
    uint8_t* cell;

    if(tx < 0 || ty < 0 || tx >= g->width || ty >= g->height) {
        return false;
    }
    cell = cell_at(g, tx, ty);
    switch(ice_tile_kind(*cell)) {
    case IceTilePawn:
        *cell = ICE_TILE_FLOOR;
        if(g->pawns_left > 0) {
            g->pawns_left--;
        }
        award(g, ICE_PAWN_SCORE);
        break;
    case IceTileDynamite:
        *cell = ICE_TILE_FLOOR;
        if(g->dynamite < ICE_MAX_DYNAMITE) {
            g->dynamite++;
        }
        break;
    case IceTileExit:
        if(g->pawns_left == 0) {
            end_stage(g, IceOutcomeCleared);
            return true;
        }
        break;
    case IceTilePush:
        if(*cell == ICE_TILE_PUSH_UP) {
            g->vy = clamp_speed(g->vy - 1);
        } else if(*cell == ICE_TILE_PUSH_DOWN) {
            g->vy = clamp_speed(g->vy + 1);
        } else if(*cell == ICE_TILE_PUSH_LEFT) {
            g->vx = clamp_speed(g->vx - 1);
        } else {
            g->vx = clamp_speed(g->vx + 1);
        }
        break;
    case IceTileSwitch:
        g->key = (uint8_t)(*cell - ICE_TILE_SWITCH1 + 1);
        break;
    case IceTileSlot:
        if((uint8_t)(*cell - ICE_TILE_SLOT0) == g->key && tx + 1 < g->width) {
            *cell_at(g, tx + 1, ty) = ICE_TILE_FLOOR;
        }
        break;
    default:
        break;
    }
    return false;
}

uint16_t ice_time_seconds(const IceGame* g) {
    return (uint16_t)((g->time_ticks + ICE_TICKS_PER_SEC - 1) / ICE_TICKS_PER_SEC);
}

void ice_game_init(IceGame* g) {
    memset(g, 0, sizeof(*g));
    g->lives = ICE_START_LIVES;
    g->next_life_score = ICE_LIFE_EVERY;
    g->phase = IcePhaseTitle;
    g->name = "";
    g->time_cell = -1;
}

void ice_game_load_raw(
    IceGame* g,
    const uint8_t* cells,
    uint8_t width,
    uint8_t height,
    uint16_t time_sec) {
    if(width == 0 || height == 0 || width > ICE_MAX_MAP_W || height > ICE_MAX_MAP_H) {
        return;
    }
    g->custom = true;
    g->stage = 0;
    g->name = "TEST";
    g->src = cells;
    g->width = width;
    g->height = height;
    g->time_sec = time_sec;
    reload(g);
}

void ice_game_load_stage(IceGame* g, uint8_t index) {
    const IceStageDef* def;
    if(index >= ICE_STAGE_COUNT) {
        return;
    }
    def = &ice_stages[index];
    g->custom = false;
    g->stage = index;
    g->name = def->name;
    g->src = def->cells;
    g->width = def->width;
    g->height = def->height;
    g->time_sec = def->time_sec;
    reload(g);
}

void ice_game_begin(IceGame* g) {
    g->score = 0;
    g->next_life_score = ICE_LIFE_EVERY;
    g->done_mask = 0;
    g->lives = ICE_START_LIVES;
    g->custom = false;
    ice_game_load_stage(g, 0);
}

void ice_game_start(IceGame* g) {
    if(g->phase == IcePhaseRoute) {
        write_timer_digits(g);
        g->phase = IcePhasePlay;
    }
}

void ice_game_set_keys(IceGame* g, bool left, bool right, bool up, bool down) {
    g->key_left = left;
    g->key_right = right;
    g->key_up = up;
    g->key_down = down;
    g->taps = 0;
}

void ice_game_key(IceGame* g, IceDir dir, bool down) {
    bool* key = dir == IceDirLeft  ? &g->key_left :
                dir == IceDirRight ? &g->key_right :
                dir == IceDirUp    ? &g->key_up :
                                     &g->key_down;
    *key = down;
    if(down) {
        g->taps = (uint8_t)(g->taps | (1u << dir));
    }
}

void ice_game_set_velocity(IceGame* g, int8_t vx, int8_t vy) {
    g->vx = clamp_speed(vx);
    g->vy = clamp_speed(vy);
}

void ice_game_detonate(IceGame* g) {
    int tx0;
    int ty0;

    if(g->phase != IcePhasePlay || g->dynamite == 0 || g->width == 0) {
        return;
    }
    g->dynamite--;
    g->blast = ICE_BLAST_TICKS;
    /* The engine clears the 8 neighbours of the cube's cell (code_00041 L01309). */
    tx0 = (g->px + ICE_CUBE / 2) / ICE_TILE - 1;
    ty0 = (g->py + ICE_CUBE / 2) / ICE_TILE - 1;
    for(int dy = 0; dy <= 2; dy++) {
        for(int dx = 0; dx <= 2; dx++) {
            int x = tx0 + dx;
            int y = ty0 + dy;
            uint8_t* cell;
            if((dx == 1 && dy == 1) || x < 0 || y < 0 || x >= g->width || y >= g->height) {
                continue;
            }
            cell = cell_at(g, x, y);
            if(ice_tile_breakable(*cell)) {
                *cell = ICE_TILE_BLASTED;
            }
        }
    }
}

void ice_game_quit_stage(IceGame* g) {
    if(g->phase == IcePhasePlay || g->phase == IcePhasePause) {
        end_stage(g, IceOutcomeQuit);
    }
}

void ice_game_tick(IceGame* g) {
    if(g->phase != IcePhasePlay) {
        return;
    }

    if(g->key_left || (g->taps & (1u << IceDirLeft))) {
        g->vx = clamp_speed(g->vx - 1);
    }
    if(g->key_right || (g->taps & (1u << IceDirRight))) {
        g->vx = clamp_speed(g->vx + 1);
    }
    if(g->key_up || (g->taps & (1u << IceDirUp))) {
        g->vy = clamp_speed(g->vy - 1);
    }
    if(g->key_down || (g->taps & (1u << IceDirDown))) {
        g->vy = clamp_speed(g->vy + 1);
    }
    g->taps = 0;

    if(g->time_ticks > 0) {
        g->time_ticks--;
    }
    write_timer_digits(g);
    if(g->time_ticks == 0) {
        end_stage(g, IceOutcomeTimeOut);
        return;
    }

    if(move_axis(g, true)) {
        return;
    }
    if(move_axis(g, false)) {
        return;
    }
    if(touch_center(g)) {
        return;
    }
    animate_tiles(g);
    if(g->blast > 0) {
        g->blast--;
    }
}

void ice_game_ack(IceGame* g) {
    switch(g->phase) {
    case IcePhaseTitle:
    case IcePhaseGameOver:
    case IcePhaseWin:
        ice_game_begin(g);
        break;
    case IcePhaseRoute:
        ice_game_start(g);
        break;
    case IcePhaseComic:
        if(g->outcome == IceOutcomeCleared) {
            if(g->custom || (uint8_t)(g->stage + 1) >= ICE_STAGE_COUNT) {
                g->phase = IcePhaseWin;
            } else {
                ice_game_load_stage(g, (uint8_t)(g->stage + 1));
            }
        } else if(g->lives == 0 || g->outcome == IceOutcomeQuit) {
            g->phase = g->lives == 0 ? IcePhaseGameOver : IcePhaseTitle;
        } else {
            reload(g);
        }
        break;
    default:
        break;
    }
}

void ice_game_camera(const IceGame* g, int16_t* cam_x, int16_t* cam_y) {
    int world_w = g->width * ICE_TILE;
    int world_h = g->height * ICE_TILE;
    int cx = g->px + ICE_CUBE / 2 - ICE_VIEW_W / 2;
    int cy = g->py + ICE_CUBE / 2 - ICE_VIEW_H / 2;

    if(cx > world_w - ICE_VIEW_W) {
        cx = world_w - ICE_VIEW_W;
    }
    if(cy > world_h - ICE_VIEW_H) {
        cy = world_h - ICE_VIEW_H;
    }
    if(cx < 0) {
        cx = 0;
    }
    if(cy < 0) {
        cy = 0;
    }
    *cam_x = (int16_t)cx;
    *cam_y = (int16_t)cy;
}
