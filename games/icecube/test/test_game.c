#include "game.h"
#include "levels/gfx_data.h"
#include "render.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT(cond)                                                   \
    do {                                                               \
        if(!(cond)) {                                                  \
            fprintf(stderr, "%s:%d  %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while(0)

#define W 16
#define H 4

/* Pawn counts of the decoded stages, in order. */
static const uint8_t k_pawns[ICE_STAGE_COUNT] = {9, 11, 11, 10, 58, 10, 79, 22, 43, 6, 10, 31};
static const uint16_t k_times[ICE_STAGE_COUNT] =
    {61, 66, 91, 66, 81, 131, 501, 116, 71, 76, 101, 161};

static void floor_map(uint8_t* map) {
    memset(map, ICE_TILE_FLOOR, W * H);
    map[2] = ICE_TILE_SPAWN; /* cube starts at tile (2, 0) */
}

static void load(IceGame* game, uint8_t* map, uint16_t time_sec) {
    ice_game_init(game);
    ice_game_load_raw(game, map, W, H, time_sec);
    EXPECT(game->phase == IcePhaseRoute);
    ice_game_start(game);
    EXPECT(game->phase == IcePhasePlay);
}

static void run(IceGame* game, int ticks) {
    for(int i = 0; i < ticks && game->phase == IcePhasePlay; i++) {
        ice_game_tick(game);
    }
}

static void test_kinds(void) {
    EXPECT(ice_tile_kind(0x00) == IceTileFloor);
    EXPECT(ice_tile_kind(0x01) == IceTileFloor);
    EXPECT(ice_tile_kind(0x03) == IceTileFloor);
    EXPECT(ice_tile_kind(0x30) == IceTilePawn);
    EXPECT(ice_tile_kind(0x31) == IceTileExit);
    EXPECT(ice_tile_kind(0x32) == IceTileExit);
    EXPECT(ice_tile_kind(0x33) == IceTileMine);
    EXPECT(ice_tile_kind(0x34) == IceTilePush);
    EXPECT(ice_tile_kind(0x37) == IceTilePush);
    EXPECT(ice_tile_kind(0x38) == IceTileSwitch);
    EXPECT(ice_tile_kind(0x3A) == IceTileSwitch);
    EXPECT(ice_tile_kind(0x3B) == IceTileSlot);
    EXPECT(ice_tile_kind(0x3E) == IceTileSlot);
    EXPECT(ice_tile_kind(0x3F) == IceTileDynamite);
    EXPECT(ice_tile_kind(0x40) == IceTileWall);
    EXPECT(ice_tile_kind(0x7F) == IceTileWall);
    EXPECT(ice_tile_kind(0x80) == IceTileBlock);
    EXPECT(ice_tile_kind(0x89) == IceTileBlock);
    EXPECT(ice_tile_kind(0x8A) == IceTileWorm);
    EXPECT(ice_tile_kind(0x90) == IceTileWater);
    EXPECT(ice_tile_kind(0x98) == IceTileWater);
    EXPECT(ice_tile_deadly(0x33));
    EXPECT(ice_tile_deadly(0x8A));
    EXPECT(ice_tile_deadly(0x90));
    EXPECT(!ice_tile_deadly(0x80));
    EXPECT(!ice_tile_deadly(0x74));
    EXPECT(ice_tile_breakable(0x40));
    EXPECT(ice_tile_breakable(0x6F));
    EXPECT(!ice_tile_breakable(0x70));
    EXPECT(!ice_tile_breakable(0x74));
    EXPECT(!ice_tile_breakable(0x80));
    EXPECT(ice_tile_outcome(0x90) == IceOutcomeWater);
    EXPECT(ice_tile_outcome(0x8A) == IceOutcomeWorm);
    EXPECT(ice_tile_outcome(0x33) == IceOutcomeBoom);
}

static void test_stages(void) {
    IceGame game;
    unsigned sum = 0;

    EXPECT(strcmp(ice_stages[0].name, "THE PRISON Stage 1") == 0);
    EXPECT(strcmp(ice_stages[5].name, "THE CASTLE Stage 1") == 0);
    EXPECT(strcmp(ice_stages[11].name, "THE ?????? Stage ?") == 0);
    EXPECT(ice_stages[0].width == 23 && ice_stages[0].height == 15);
    EXPECT(ice_stages[6].width == 40 && ice_stages[6].height == 32);
    EXPECT(ice_stages[11].width == 65 && ice_stages[11].height == 13);
    EXPECT(ice_stages[0].cells[0] == 0x74);
    EXPECT(ice_stages[0].cells[23 * 1 + 1] == 0x56);

    ice_game_init(&game);
    for(uint8_t i = 0; i < ICE_STAGE_COUNT; i++) {
        unsigned spawns = 0;
        unsigned anchors = 0;
        unsigned exits = 0;
        ice_game_load_stage(&game, i);
        EXPECT(game.pawns_left == k_pawns[i]);
        EXPECT(ice_stages[i].pawns == k_pawns[i]);
        EXPECT(ice_stages[i].time_sec == k_times[i]);
        EXPECT(game.time_cell >= 0);
        EXPECT(game.phase == IcePhaseRoute);
        for(uint16_t c = 0; c < (uint16_t)ice_stages[i].width * ice_stages[i].height; c++) {
            uint8_t raw = ice_stages[i].cells[c];
            spawns += raw == ICE_TILE_SPAWN;
            anchors += raw == ICE_TILE_TIME;
            exits += raw == ICE_TILE_EXIT_L;
            EXPECT(raw < ICE_TILE_COUNT);
        }
        EXPECT(spawns == 1);
        EXPECT(anchors == 1);
        EXPECT(exits >= 1);
        sum += ice_stages[i].pawns;
    }
    EXPECT(sum == 300);
    EXPECT(ice_pawn_total == 300);

    ice_game_load_stage(&game, 0);
    ice_game_start(&game);
    /* The timer digits are written into the map after the TIME label. */
    EXPECT(game.map[game.time_cell] == ICE_TILE_DIGIT0 + 0);
    EXPECT(game.map[game.time_cell + 1] == ICE_TILE_DIGIT0 + 6);
    EXPECT(game.map[game.time_cell + 2] == ICE_TILE_DIGIT0 + 1);
}

static void test_slide(void) {
    uint8_t map[W * H];
    IceGame game;
    int start;

    floor_map(map);
    map[8] = 0x80; /* block: stops without bouncing */

    load(&game, map, 60);
    EXPECT(game.px == 2 * ICE_TILE && game.py == 0);
    start = game.px;
    ice_game_set_keys(&game, false, true, false, false);
    ice_game_tick(&game);
    ice_game_set_keys(&game, false, false, false, false);
    EXPECT(game.px > start);
    start = game.px;
    ice_game_tick(&game);
    EXPECT(game.px > start);
    EXPECT(game.vx > 0);

    load(&game, map, 60);
    start = game.px;
    ice_game_set_velocity(&game, 2, 0);
    ice_game_tick(&game);
    EXPECT(game.px == start + 2);

    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 80);
    EXPECT(game.vx == 0);
    EXPECT(game.px == 8 * ICE_TILE - ICE_CUBE + 1); /* box is inset by 1 */
    EXPECT(game.phase == IcePhasePlay);
}

static void test_bounce(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    map[5] = 0x54; /* wall: bounces */

    /* Elastic, like the original: the speed is kept, only the sign flips. */
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 10);
    EXPECT(game.vx == -3);
    EXPECT(game.px < 5 * ICE_TILE);
    EXPECT(game.phase == IcePhasePlay);

    /* A slow cube bounces too and keeps drifting back. */
    load(&game, map, 60);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 40);
    EXPECT(game.vx == -1);
    EXPECT(game.px < 5 * ICE_TILE - ICE_CUBE + 1);

    /* Holding the key toward the wall at impact parks the cube against it. */
    load(&game, map, 60);
    ice_game_set_keys(&game, false, true, false, false);
    run(&game, 40);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);
    EXPECT(game.vx == 0);
    ice_game_set_keys(&game, false, false, false, false);
    run(&game, 10);
    EXPECT(game.vx == 0);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);

    /* A tap landing in the tick of the impact counts as holding. */
    load(&game, map, 60);
    game.px = 5 * ICE_TILE - ICE_CUBE; /* one pixel from the wall */
    ice_game_set_velocity(&game, 1, 0);
    ice_game_key(&game, IceDirRight, true);
    ice_game_key(&game, IceDirRight, false);
    ice_game_tick(&game);
    ice_game_tick(&game);
    EXPECT(game.vx == 0);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);

    /* A slot with the wrong key leaves the wall alone and the cube bounces off it. */
    floor_map(map);
    map[3] = 0x3D; /* slot for key 2 */
    map[4] = 0x54;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 20);
    EXPECT(game.map[4] == 0x54);
    EXPECT(game.vx == -1);
}

static void test_pawn_and_exit(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    map[4] = ICE_TILE_PAWN;
    map[7] = ICE_TILE_EXIT_L;
    map[8] = ICE_TILE_EXIT_R;

    /* The exit is solid while pawns remain. */
    load(&game, map, 30);
    game.px = 5 * ICE_TILE;
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 20);
    EXPECT(game.phase == IcePhasePlay);
    EXPECT(game.pawns_left == 1);
    EXPECT(game.px < 7 * ICE_TILE);

    load(&game, map, 30);
    EXPECT(game.pawns_left == 1);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 40);
    EXPECT(game.pawns_left == 0);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeCleared);
    EXPECT(game.score == ICE_PAWN_SCORE + ICE_CLEAR_SCORE + 30 * ICE_TIME_BONUS);
    EXPECT(game.lives == ICE_START_LIVES);

    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseWin);

    load(&game, map, 30);
    game.score = 950;
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 40);
    EXPECT(game.lives == ICE_START_LIVES + 1);
}

static void test_blast(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    map[2] = ICE_TILE_SPAWN;
    map[1] = 0x74; /* not breakable */
    map[3] = 0x50; /* breakable */
    map[2 + W] = 0x6F; /* breakable */
    map[3 + W] = 0x90; /* water, untouched */
    load(&game, map, 60);
    game.dynamite = 1;
    ice_game_detonate(&game);
    EXPECT(game.dynamite == 0);
    EXPECT(game.blast > 0);
    EXPECT(game.map[3] == ICE_TILE_BLASTED);
    EXPECT(game.map[2 + W] == ICE_TILE_BLASTED);
    EXPECT(game.map[1] == 0x74);
    EXPECT(game.map[3 + W] == 0x90);
    EXPECT(game.map[2] == ICE_TILE_SPAWN);

    game.map[3] = 0x50;
    ice_game_detonate(&game);
    EXPECT(game.map[3] == 0x50);

    floor_map(map);
    map[3] = ICE_TILE_DYNAMITE;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 8);
    EXPECT(game.dynamite == 1);
    EXPECT(game.map[3] == ICE_TILE_FLOOR);
}

static void test_death(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    map[4] = 0x90;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 8);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeWater);
    EXPECT(game.lives == ICE_START_LIVES - 1);
    EXPECT(game.can_continue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute);
    EXPECT(game.route_sel == IceRouteContinue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhasePlay);
    EXPECT(game.px == 2 * ICE_TILE);
    EXPECT(game.dynamite == 0);

    floor_map(map);
    map[3] = ICE_TILE_MINE;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 8);
    EXPECT(game.outcome == IceOutcomeBoom);
    EXPECT(game.can_continue);

    /* Worm segments only kill in their 0x8A..0x8F phase, which the animation reaches. */
    floor_map(map);
    map[3] = 0x80;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 4);
    EXPECT(game.phase == IcePhasePlay);
    EXPECT(game.vx == 0);
    run(&game, ICE_ANIM_TICKS * 12);
    EXPECT(game.map[3] >= 0x8A || game.phase == IcePhaseComic);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 4);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeWorm);
    EXPECT(game.can_continue);

    /* A time-out costs a life and offers no CONTINUE. */
    floor_map(map);
    load(&game, map, 1);
    run(&game, ICE_TICKS_PER_SEC);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeTimeOut);
    EXPECT(game.lives == ICE_START_LIVES - 1);
    EXPECT(!game.can_continue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute);
    EXPECT(game.route_sel == IceRouteStart);

    load(&game, map, 1);
    game.lives = 1;
    run(&game, ICE_TICKS_PER_SEC);
    EXPECT(game.lives == 0);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseGameOver);

    /* Quitting a stage costs a life like in the original, and offers CONTINUE. */
    load(&game, map, 60);
    ice_game_take_events(&game);
    ice_game_quit_stage(&game);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeQuit);
    EXPECT(game.lives == ICE_START_LIVES - 1);
    EXPECT(game.can_continue);
    EXPECT((ice_game_take_events(&game) & IceEventDeath) == 0);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute);
    EXPECT(game.route_sel == IceRouteContinue);

    /* Quitting on the last life ends the game. */
    load(&game, map, 60);
    game.lives = 1;
    ice_game_quit_stage(&game);
    EXPECT(!game.can_continue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseGameOver);
}

static void test_switch_and_push(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    map[3] = 0x39; /* switch: key 2 */
    map[4] = 0x3D; /* slot for key 2 */
    map[5] = 0x54; /* wall it opens */
    load(&game, map, 60);
    EXPECT(game.key == 0);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 10);
    EXPECT(game.key == 2);
    run(&game, 10);
    EXPECT(game.map[5] == ICE_TILE_FLOOR);

    floor_map(map);
    map[3] = 0x3B; /* slot for key 0 */
    map[4] = 0x54;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 12);
    EXPECT(game.map[4] == ICE_TILE_FLOOR);

    floor_map(map);
    map[2] = ICE_TILE_PUSH_RIGHT;
    map[2 + W] = ICE_TILE_SPAWN;
    load(&game, map, 60);
    game.py = 0;
    run(&game, 3);
    EXPECT(game.vx > 0);
}

static void test_tap(void) {
    uint8_t map[W * H];
    IceGame game;

    /* A press and release between two ticks still gives one impulse. */
    floor_map(map);
    load(&game, map, 60);
    ice_game_key(&game, IceDirRight, true);
    ice_game_key(&game, IceDirRight, false);
    EXPECT(!game.key_right);
    ice_game_tick(&game);
    EXPECT(game.vx == 1);
    ice_game_tick(&game);
    EXPECT(game.vx == 1);
    EXPECT(game.px == 2 * ICE_TILE + 2);

    /* Holding keeps accelerating; set_keys clears any pending tap. */
    ice_game_key(&game, IceDirDown, true);
    ice_game_tick(&game);
    ice_game_tick(&game);
    EXPECT(game.vy == 2);
    ice_game_set_keys(&game, false, false, false, false);
    EXPECT(game.taps == 0);
}

static void test_camera(void) {
    IceGame game;
    int16_t cx;
    int16_t cy;

    ice_game_init(&game);
    ice_game_load_stage(&game, 6); /* 40x32 */
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cx >= 0 && cy >= 0);
    EXPECT(cx <= 40 * ICE_TILE - ICE_VIEW_W);
    EXPECT(cy <= 32 * ICE_TILE - ICE_VIEW_H);
    game.px = 0;
    game.py = 0;
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cx == 0 && cy == 0);
    game.px = 40 * ICE_TILE - ICE_CUBE;
    game.py = 32 * ICE_TILE - ICE_CUBE;
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cx == 40 * ICE_TILE - ICE_VIEW_W);
    EXPECT(cy == 32 * ICE_TILE - ICE_VIEW_H);

    ice_game_load_stage(&game, 2); /* 40x9: one row taller than the view */
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cy >= 0 && cy <= 9 * ICE_TILE - ICE_VIEW_H);
}

static void test_camera_lead(void) {
    IceGame game;
    int16_t cx;
    int16_t cy;
    const int world_w = 65 * ICE_TILE;

    ice_game_init(&game);
    ice_game_load_stage(&game, 11); /* 65x13, the widest map */
    ice_game_start(&game);
    game.px = 30 * ICE_TILE;
    game.py = 6 * ICE_TILE;
    ice_game_set_velocity(&game, 3, 0);
    ice_game_update_view(&game);
    EXPECT(game.lead_x == ICE_LEAD_STEP);
    for(int i = 0; i < 20; i++) {
        ice_game_update_view(&game);
    }
    EXPECT(game.lead_x == 3 * ICE_LEAD_X);
    EXPECT(game.lead_y == 0);
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cx == game.px + ICE_CUBE / 2 + 3 * ICE_LEAD_X - ICE_VIEW_W / 2);

    /* Near the right edge the lead cannot push the view past the map. */
    game.px = (int16_t)(world_w - 2 * ICE_TILE);
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cx == world_w - ICE_VIEW_W);

    /* Turning back swings the lead over at ICE_LEAD_STEP per tick. */
    game.px = 30 * ICE_TILE;
    ice_game_set_velocity(&game, -3, 2);
    ice_game_update_view(&game);
    EXPECT(game.lead_x == 3 * ICE_LEAD_X - ICE_LEAD_STEP);
    for(int i = 0; i < 40; i++) {
        ice_game_update_view(&game);
    }
    EXPECT(game.lead_x == -3 * ICE_LEAD_X);
    EXPECT(game.lead_y == 2 * ICE_LEAD_Y);
}

static bool hud_overlaps_cube(const IceGame* game, int margin) {
    int16_t cx;
    int16_t cy;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int sx;
    int sy;

    ice_game_camera(game, &cx, &cy);
    ice_game_hud_rect(game, &x, &y, &w, &h);
    sx = game->px - cx;
    sy = game->py - cy;
    return sx + ICE_CUBE + margin > x && sx - margin < x + w && sy + ICE_CUBE + margin > y &&
           sy - margin < y + h;
}

static void test_hud_corner(void) {
    IceGame game;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    ice_game_init(&game);
    ice_game_load_stage(&game, 6); /* 40x32 */
    ice_game_start(&game);
    EXPECT(!hud_overlaps_cube(&game, 0));

    /* The box sits flush in a screen corner. */
    game.hud_corner = 3;
    ice_game_hud_rect(&game, &x, &y, &w, &h);
    EXPECT(x + w == ICE_VIEW_W && y + h == ICE_VIEW_H);

    /* The cube in the top-left corner pushes a top-left box away. */
    game.px = ICE_TILE;
    game.py = ICE_TILE;
    game.hud_corner = 0;
    ice_game_update_view(&game);
    EXPECT(game.hud_corner != 0);
    EXPECT(!hud_overlaps_cube(&game, ICE_HUD_MARGIN - 1));

    /* Hysteresis: with the cube mid-screen the box stays where it is. */
    game.px = 20 * ICE_TILE;
    game.py = 16 * ICE_TILE;
    for(uint8_t c = 0; c < 4; c++) {
        game.hud_corner = c;
        ice_game_update_view(&game);
        EXPECT(game.hud_corner == c);
    }

    /* Sweep the cube over the whole map, three pixels per tick like full
     * speed: the box never covers it. */
    game.hud_corner = 0;
    for(int py = 0; py <= 31 * ICE_TILE; py += 12) {
        bool forward = (py / 12) % 2 == 0;
        for(int i = 0; i <= 39 * ICE_TILE; i += 3) {
            game.px = (int16_t)(forward ? i : 39 * ICE_TILE - i);
            game.py = (int16_t)py;
            ice_game_update_view(&game);
            EXPECT(!hud_overlaps_cube(&game, 0));
        }
    }
}

static void test_continue(void) {
    uint8_t map[W * H];
    IceGame game;
    uint16_t ticks_at_death;

    floor_map(map);
    map[4] = ICE_TILE_PAWN;
    map[10] = ICE_TILE_PAWN;
    map[3 + W] = 0x54; /* breakable wall below the path */
    map[8] = 0x90; /* water */
    load(&game, map, 60);
    game.dynamite = 2;
    game.key = 1;
    game.px = 3 * ICE_TILE; /* centre over tile 3 to blast tile (3, 1) */
    ice_game_detonate(&game);
    EXPECT(game.map[3 + W] == ICE_TILE_BLASTED);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 20);
    EXPECT(game.outcome == IceOutcomeWater);
    EXPECT(game.pawns_left == 1);
    ticks_at_death = game.time_ticks;
    EXPECT(ticks_at_death < 60 * ICE_TICKS_PER_SEC);

    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute && game.route_sel == IceRouteContinue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhasePlay);
    EXPECT(game.map[4] == ICE_TILE_FLOOR);
    EXPECT(game.map[3 + W] == ICE_TILE_BLASTED);
    EXPECT(game.pawns_left == 1);
    EXPECT(game.time_ticks == ticks_at_death);
    EXPECT(game.px == 2 * ICE_TILE && game.py == 0);
    EXPECT(game.key == 0);
    EXPECT(game.dynamite == 0);
    EXPECT(game.vx == 0 && game.vy == 0);
    EXPECT(!game.can_continue);

    /* START instead restores the stage as designed. */
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 20);
    EXPECT(game.outcome == IceOutcomeWater);
    ice_game_ack(&game);
    ice_game_route_move(&game, -1);
    EXPECT(game.route_sel == IceRouteStart);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhasePlay);
    EXPECT(game.pawns_left == 2);
    EXPECT(game.map[4] == ICE_TILE_PAWN);
    EXPECT(game.map[3 + W] == 0x54);
    EXPECT(game.time_ticks == 60 * ICE_TICKS_PER_SEC);

    /* A cleared stage offers no CONTINUE on the next route screen. */
    ice_game_init(&game);
    ice_game_load_stage(&game, 0);
    ice_game_start(&game);
    for(uint16_t i = 0; i < (uint16_t)game.width * game.height; i++) {
        if(game.map[i] == ICE_TILE_EXIT_L) {
            game.px = (int16_t)((i % game.width) * ICE_TILE);
            game.py = (int16_t)((i / game.width) * ICE_TILE);
            break;
        }
    }
    game.pawns_left = 0;
    ice_game_tick(&game);
    EXPECT(game.phase == IcePhaseComic && game.outcome == IceOutcomeCleared);
    EXPECT(!game.can_continue);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute && game.stage == 1);
    EXPECT(!game.can_continue && game.route_sel == IceRouteStart);
}

static void test_route_menu(void) {
    uint8_t map[W * H];
    IceGame game;

    floor_map(map);
    load(&game, map, 60);
    ice_game_quit_stage(&game);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute);
    EXPECT(ice_game_route_available(&game, IceRouteContinue));
    EXPECT(game.route_sel == IceRouteContinue);
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteQuit);
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteStart);
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteContinue);
    ice_game_route_move(&game, -1);
    EXPECT(game.route_sel == IceRouteStart);

    /* Without CONTINUE the cursor skips it both ways. */
    ice_game_init(&game);
    ice_game_load_raw(&game, map, W, H, 60);
    EXPECT(game.route_sel == IceRouteStart);
    EXPECT(!ice_game_route_available(&game, IceRouteContinue));
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteQuit);
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteStart);
    ice_game_route_move(&game, -1);
    EXPECT(game.route_sel == IceRouteQuit);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseTitle);

    /* The cursor only moves on the route screen. */
    game.route_sel = IceRouteStart;
    ice_game_route_move(&game, 1);
    EXPECT(game.route_sel == IceRouteStart);
}

static void test_probe_order(void) {
    uint8_t map[W * H];
    IceGame game;

    /* The box straddles rows 0 and 1; tile (4, 0) is a block and (4, 1) water.
     * The step into them kills whatever corner is looked at first. */
    floor_map(map);
    map[4] = 0x80;
    map[4 + W] = 0x90;
    load(&game, map, 60);
    game.py = 4;
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 10);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeWater);
}

static void test_events(void) {
    uint8_t map[W * H];
    IceGame game;
    uint16_t seen = 0;
    int hurry = 0;

    floor_map(map);
    map[4] = ICE_TILE_PAWN;
    map[5] = ICE_TILE_DYNAMITE;
    map[7] = ICE_TILE_PAWN;
    map[8] = ICE_TILE_EXIT_L; /* opens in the tick the last pawn is taken */
    map[9] = ICE_TILE_EXIT_R;
    load(&game, map, 60);
    ice_game_take_events(&game);
    ice_game_set_velocity(&game, 3, 0);
    for(int i = 0; i < 40 && game.phase == IcePhasePlay; i++) {
        ice_game_tick(&game);
        seen |= ice_game_take_events(&game);
        if(game.phase == IcePhasePlay) {
            EXPECT(game.vx == 3); /* the exit never stops the cube */
        }
    }
    EXPECT(game.outcome == IceOutcomeCleared);
    EXPECT(seen & IceEventPawn);
    EXPECT(seen & IceEventDynamite);
    EXPECT(seen & IceEventExitOpen);
    EXPECT(seen & IceEventCleared);
    EXPECT((seen & IceEventDeath) == 0);

    /* Hurry fires once, when the clock crosses ICE_HURRY_SEC. */
    floor_map(map);
    load(&game, map, ICE_HURRY_SEC + 2);
    ice_game_take_events(&game);
    for(int i = 0; i < 4 * ICE_TICKS_PER_SEC && game.phase == IcePhasePlay; i++) {
        ice_game_tick(&game);
        hurry += (ice_game_take_events(&game) & IceEventHurry) != 0;
    }
    EXPECT(hurry == 1);

    floor_map(map);
    map[4] = 0x90;
    load(&game, map, 60);
    game.dynamite = 1;
    ice_game_detonate(&game);
    EXPECT(ice_game_take_events(&game) & IceEventBlast);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 10);
    EXPECT(ice_game_take_events(&game) & IceEventDeath);

    floor_map(map);
    map[4] = ICE_TILE_PAWN;
    map[6] = ICE_TILE_PAWN;
    load(&game, map, 60);
    game.score = 950;
    ice_game_take_events(&game);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 4);
    EXPECT(ice_game_take_events(&game) & IceEventExtraLife);
    EXPECT(ice_game_take_events(&game) == 0);
}

static void test_gray_phases(void) {
    EXPECT(ice_phase_uses_gray(IcePhaseTitle));
    EXPECT(ice_phase_uses_gray(IcePhaseRoute));
    EXPECT(ice_phase_uses_gray(IcePhaseComic));
    EXPECT(!ice_phase_uses_gray(IcePhasePlay));
    EXPECT(!ice_phase_uses_gray(IcePhasePause));
    EXPECT(!ice_phase_uses_gray(IcePhaseGameOver));
    EXPECT(!ice_phase_uses_gray(IcePhaseWin));
}

static void test_preview(void) {
    FILE* file = fopen("../levels/preview_stage01.png", "rb");
    unsigned char sig[8];
    size_t n;
    EXPECT(file != NULL);
    if(file == NULL) {
        return;
    }
    n = fread(sig, 1, 8, file);
    fclose(file);
    EXPECT(n == 8);
    EXPECT(sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G');
}

int main(void) {
    test_kinds();
    test_stages();
    test_slide();
    test_bounce();
    test_pawn_and_exit();
    test_blast();
    test_death();
    test_switch_and_push();
    test_tap();
    test_camera();
    test_camera_lead();
    test_hud_corner();
    test_continue();
    test_route_menu();
    test_probe_order();
    test_events();
    test_gray_phases();
    test_preview();
    if(failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
