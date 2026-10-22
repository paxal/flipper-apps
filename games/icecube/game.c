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
            g->events |= IceEventExtraLife;
        }
        g->next_life_score += ICE_LIFE_EVERY;
    }
}

static uint8_t* cell_at(IceGame* g, int tx, int ty) {
    return &g->map[(uint16_t)ty * g->width + (uint16_t)tx];
}

static void clear_input(IceGame* g) {
    g->key_left = false;
    g->key_right = false;
    g->key_up = false;
    g->key_down = false;
    g->taps = 0;
    g->held = 0;
}

typedef enum {
    HitNone = 0,
    HitStop,
    HitBounce,
    HitDead,
} Hit;

/* Probe the collision box at pixel (x, y); *deadly receives the tile that kills.
 * Death is checked on all four corners before anything solid, so the outcome of
 * a step never depends on corner order. */
static Hit probe_box(IceGame* g, int x, int y, uint8_t* deadly) {
    static const int ox[4] = {0, ICE_BOX - 1, 0, ICE_BOX - 1};
    static const int oy[4] = {0, 0, ICE_BOX - 1, ICE_BOX - 1};
    uint8_t cells[4];
    Hit hit = HitNone;

    if(x < 0 || y < 0 || x + ICE_BOX > g->width * ICE_TILE || y + ICE_BOX > g->height * ICE_TILE) {
        return HitStop;
    }

    for(int i = 0; i < 4; i++) {
        cells[i] = *cell_at(g, (x + ox[i]) / ICE_TILE, (y + oy[i]) / ICE_TILE);
        if(ice_tile_deadly(cells[i])) {
            if(deadly != NULL) {
                *deadly = cells[i];
            }
            return HitDead;
        }
    }
    for(int i = 0; i < 4; i++) {
        IceTileKind kind = ice_tile_kind(cells[i]);
        if(kind == IceTileBlock || (kind == IceTileExit && g->pawns_left > 0)) {
            return HitStop;
        }
        if(kind == IceTileWall) {
            hit = HitBounce;
        }
    }
    return hit;
}

/* Exit codes follow the original: after water, worm, mine or a quit the route
 * menu offers CONTINUE; after a time-out it does not (ICE.ENT PLAY, the branch
 * after the comic that stores the remaining time and the continue flag). */
static void end_stage(IceGame* g, IceOutcome outcome) {
    g->outcome = outcome;
    g->vx = 0;
    g->vy = 0;
    g->can_continue = false;
    if(outcome == IceOutcomeCleared) {
        award(g, ICE_CLEAR_SCORE + (uint32_t)ice_time_seconds(g) * ICE_TIME_BONUS);
        g->events |= IceEventCleared;
    } else {
        if(g->lives > 0) {
            g->lives--;
        }
        if(outcome != IceOutcomeQuit) {
            g->events |= IceEventDeath;
        }
        g->can_continue = g->lives > 0 && outcome != IceOutcomeTimeOut;
    }
    g->phase = IcePhaseComic;
}

/* Screen rectangle of HUD corner c (bit 0 = right, bit 1 = bottom). */
static void corner_rect(uint8_t c, int* x, int* y) {
    *x = (c & 1) ? ICE_VIEW_W - ICE_HUD_W : 0;
    *y = (c & 2) ? ICE_VIEW_H - ICE_HUD_H : 0;
}

/* Gap in pixels between the cube on screen and HUD corner c (0 when they touch). */
static int corner_gap(const IceGame* g, uint8_t c) {
    int16_t cam_x;
    int16_t cam_y;
    int rx;
    int ry;
    int sx;
    int sy;
    int dx;
    int dy;

    ice_game_camera(g, &cam_x, &cam_y);
    sx = g->px - cam_x;
    sy = g->py - cam_y;
    corner_rect(c, &rx, &ry);
    dx = rx - (sx + ICE_CUBE);
    if(sx - (rx + ICE_HUD_W) > dx) {
        dx = sx - (rx + ICE_HUD_W);
    }
    dy = ry - (sy + ICE_CUBE);
    if(sy - (ry + ICE_HUD_H) > dy) {
        dy = sy - (ry + ICE_HUD_H);
    }
    if(dx < 0) {
        dx = 0;
    }
    if(dy < 0) {
        dy = 0;
    }
    return dx > dy ? dx : dy;
}

/* Keep the HUD where it is until the cube comes close; then take the corner that
 * is far from the cube and not where the camera leads, so the box covers neither
 * the cube nor the way ahead. */
static void pick_hud_corner(IceGame* g, bool force) {
    static const uint8_t order[4] = {2, 1, 3, 0}; /* tie-break: bottom-left first */
    int best = -1000;
    uint8_t best_c = g->hud_corner;

    if(!force && corner_gap(g, g->hud_corner) >= ICE_HUD_MARGIN) {
        return;
    }
    for(int i = 0; i < 4; i++) {
        uint8_t c = order[i];
        int score = corner_gap(g, c);
        bool ahead_x = g->lead_x == 0 || ((g->lead_x > 0) == ((c & 1) != 0));
        bool ahead_y = g->lead_y == 0 || ((g->lead_y > 0) == ((c & 2) != 0));
        if(score > 24) {
            score = 24;
        }
        if((g->lead_x != 0 || g->lead_y != 0) && ahead_x && ahead_y) {
            score -= 16;
        }
        if(score > best) {
            best = score;
            best_c = c;
        }
    }
    g->hud_corner = best_c;
}

static void place_cube_at_spawn(IceGame* g) {
    g->px = (int16_t)((g->spawn_cell % g->width) * ICE_TILE);
    g->py = (int16_t)((g->spawn_cell / g->width) * ICE_TILE);
    g->vx = 0;
    g->vy = 0;
    g->key = 0;
    g->dynamite = 0;
    g->blast = 0;
    g->lead_x = 0;
    g->lead_y = 0;
    clear_input(g);
    pick_hud_corner(g, true);
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
    g->spawn_cell = (int16_t)spawn;
    g->anim = 0;
    g->time_ticks = (uint16_t)(g->time_sec * ICE_TICKS_PER_SEC);
    g->can_continue = false;
    g->route_sel = IceRouteStart;
    place_cube_at_spawn(g);
    g->phase = IcePhaseRoute;
}

/* CONTINUE: the map stays as it was left (pawns taken, slots opened, walls
 * blasted) and so does the remaining time; a new white cube starts at the spawn
 * without dynamite, as the manual describes. */
static void continue_stage(IceGame* g) {
    g->can_continue = false;
    place_cube_at_spawn(g);
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
        if(hit == HitStop || hit == HitBounce) {
            /* The engine negates the velocity on a bounce wall (code_00041
             * L00C4A) and never loses speed. Holding the key toward the wall at
             * impact stops the cube instead, so a slow cube can be parked. */
            IceDir toward = horizontal ? (step > 0 ? IceDirRight : IceDirLeft) :
                                         (step > 0 ? IceDirDown : IceDirUp);
            int8_t next = 0;
            if(hit == HitBounce && (g->held & (1u << toward)) == 0) {
                next = (int8_t)-vel;
            }
            if(horizontal) {
                g->vx = next;
            } else {
                g->vy = next;
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
        g->events |= g->pawns_left == 0 ? IceEventExitOpen : IceEventPawn;
        award(g, ICE_PAWN_SCORE);
        break;
    case IceTileDynamite:
        *cell = ICE_TILE_FLOOR;
        if(g->dynamite < ICE_MAX_DYNAMITE) {
            g->dynamite++;
        }
        g->events |= IceEventDynamite;
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

static int16_t approach(int16_t current, int target, int step) {
    if(current < target) {
        return (int16_t)(current + step > target ? target : current + step);
    }
    if(current > target) {
        return (int16_t)(current - step < target ? target : current - step);
    }
    return current;
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
    g->hud_corner = 2;
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
    g->lives = ICE_START_LIVES;
    g->custom = false;
    ice_game_load_stage(g, 0);
}

void ice_game_start(IceGame* g) {
    if(g->phase == IcePhaseRoute) {
        g->can_continue = false;
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
    g->events |= IceEventBlast;
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

void ice_game_resume(IceGame* g) {
    if(g->phase == IcePhasePause) {
        clear_input(g);
        g->phase = IcePhasePlay;
    }
}

void ice_game_tick(IceGame* g) {
    uint16_t seconds_before;

    if(g->phase != IcePhasePlay) {
        return;
    }

    g->held = g->taps;
    g->held |= g->key_left ? (1u << IceDirLeft) : 0;
    g->held |= g->key_right ? (1u << IceDirRight) : 0;
    g->held |= g->key_up ? (1u << IceDirUp) : 0;
    g->held |= g->key_down ? (1u << IceDirDown) : 0;
    g->taps = 0;

    if(g->held & (1u << IceDirLeft)) {
        g->vx = clamp_speed(g->vx - 1);
    }
    if(g->held & (1u << IceDirRight)) {
        g->vx = clamp_speed(g->vx + 1);
    }
    if(g->held & (1u << IceDirUp)) {
        g->vy = clamp_speed(g->vy - 1);
    }
    if(g->held & (1u << IceDirDown)) {
        g->vy = clamp_speed(g->vy + 1);
    }

    seconds_before = ice_time_seconds(g);
    if(g->time_ticks > 0) {
        g->time_ticks--;
    }
    if(seconds_before > ICE_HURRY_SEC && ice_time_seconds(g) <= ICE_HURRY_SEC) {
        g->events |= IceEventHurry;
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
    ice_game_update_view(g);
    animate_tiles(g);
    if(g->blast > 0) {
        g->blast--;
    }
}

bool ice_game_route_available(const IceGame* g, IceRouteSel sel) {
    return sel != IceRouteContinue || g->can_continue;
}

void ice_game_route_move(IceGame* g, int delta) {
    int sel = g->route_sel;
    if(g->phase != IcePhaseRoute || delta == 0) {
        return;
    }
    for(int i = 0; i < IceRouteCount; i++) {
        sel = (sel + (delta > 0 ? 1 : IceRouteCount - 1)) % IceRouteCount;
        if(ice_game_route_available(g, (IceRouteSel)sel)) {
            break;
        }
    }
    g->route_sel = (uint8_t)sel;
}

void ice_game_ack(IceGame* g) {
    switch(g->phase) {
    case IcePhaseTitle:
    case IcePhaseGameOver:
    case IcePhaseWin:
        ice_game_begin(g);
        break;
    case IcePhaseRoute:
        if(g->route_sel == IceRouteQuit) {
            g->can_continue = false;
            g->phase = IcePhaseTitle;
        } else if(g->route_sel == IceRouteContinue && g->can_continue) {
            continue_stage(g);
            ice_game_start(g);
        } else {
            reload(g);
            ice_game_start(g);
        }
        break;
    case IcePhaseComic:
        if(g->outcome == IceOutcomeCleared) {
            if(g->custom || (uint8_t)(g->stage + 1) >= ICE_STAGE_COUNT) {
                g->phase = IcePhaseWin;
            } else {
                ice_game_load_stage(g, (uint8_t)(g->stage + 1));
            }
        } else if(g->lives == 0) {
            g->phase = IcePhaseGameOver;
        } else {
            /* Back to the route menu with the map as it was left; START reloads. */
            g->route_sel = g->can_continue ? IceRouteContinue : IceRouteStart;
            g->phase = IcePhaseRoute;
        }
        break;
    default:
        break;
    }
}

uint16_t ice_game_take_events(IceGame* g) {
    uint16_t events = g->events;
    g->events = 0;
    return events;
}

void ice_game_camera(const IceGame* g, int16_t* cam_x, int16_t* cam_y) {
    int world_w = g->width * ICE_TILE;
    int world_h = g->height * ICE_TILE;
    int cx = g->px + ICE_CUBE / 2 + g->lead_x - ICE_VIEW_W / 2;
    int cy = g->py + ICE_CUBE / 2 + g->lead_y - ICE_VIEW_H / 2;

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

void ice_game_update_view(IceGame* g) {
    g->lead_x = approach(g->lead_x, g->vx * ICE_LEAD_X, ICE_LEAD_STEP);
    g->lead_y = approach(g->lead_y, g->vy * ICE_LEAD_Y, ICE_LEAD_STEP);
    pick_hud_corner(g, false);
}

void ice_game_hud_rect(const IceGame* g, int16_t* x, int16_t* y, int16_t* w, int16_t* h) {
    int rx;
    int ry;
    corner_rect(g->hud_corner, &rx, &ry);
    *x = (int16_t)rx;
    *y = (int16_t)ry;
    *w = ICE_HUD_W;
    *h = ICE_HUD_H;
}
