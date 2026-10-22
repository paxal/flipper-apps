#include "game.h"
#include "levels/gfx_data.h"

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
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 10);
    EXPECT(game.vx == -2); /* one speed unit lost per bounce */
    EXPECT(game.px < 5 * ICE_TILE);
    EXPECT(game.phase == IcePhasePlay);

    /* A bounce at speed 1 stops the cube instead of drifting away forever. */
    load(&game, map, 60);
    ice_game_set_velocity(&game, 1, 0);
    run(&game, 40);
    EXPECT(game.vx == 0);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);

    /* Pushing into the wall holds the cube still. */
    load(&game, map, 60);
    ice_game_set_keys(&game, false, true, false, false);
    run(&game, 40);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);
    EXPECT(game.vx <= 1);
    ice_game_set_keys(&game, false, false, false, false);
    run(&game, 10);
    EXPECT(game.vx == 0);
    EXPECT(game.px == 5 * ICE_TILE - ICE_CUBE + 1);
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
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseRoute);
    EXPECT(game.px == 2 * ICE_TILE);
    EXPECT(game.dynamite == 0);

    floor_map(map);
    map[3] = ICE_TILE_MINE;
    load(&game, map, 60);
    ice_game_set_velocity(&game, 3, 0);
    run(&game, 8);
    EXPECT(game.outcome == IceOutcomeBoom);

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

    floor_map(map);
    load(&game, map, 1);
    run(&game, ICE_TICKS_PER_SEC);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeTimeOut);
    EXPECT(game.lives == ICE_START_LIVES - 1);

    load(&game, map, 1);
    game.lives = 1;
    run(&game, ICE_TICKS_PER_SEC);
    EXPECT(game.lives == 0);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseGameOver);

    load(&game, map, 60);
    ice_game_quit_stage(&game);
    EXPECT(game.phase == IcePhaseComic);
    EXPECT(game.outcome == IceOutcomeQuit);
    ice_game_ack(&game);
    EXPECT(game.phase == IcePhaseTitle);
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

    ice_game_load_stage(&game, 2); /* 40x9: shorter than the view */
    ice_game_camera(&game, &cx, &cy);
    EXPECT(cy == 0);
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
    test_preview();
    if(failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
