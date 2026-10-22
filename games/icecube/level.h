#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Map byte classes, from the ICE.EXE play loop (ice1_1/disasm/code/code_00041.asm,
 * tile tests around 0x0083E-0x00A4F and the blast at 0x013F0). The map byte is
 * also the index into the 192-tile sprite bank. */
#define ICE_TILE_SPAWN      0x00
#define ICE_TILE_TIME       0x03 /* "ME" half of the TIME label; digits follow */
#define ICE_TILE_BLASTED    0x04
#define ICE_TILE_DIGIT0     0x0C
#define ICE_TILE_PAWN       0x30
#define ICE_TILE_EXIT_L     0x31
#define ICE_TILE_EXIT_R     0x32
#define ICE_TILE_MINE       0x33
#define ICE_TILE_PUSH_UP    0x34
#define ICE_TILE_PUSH_DOWN  0x35
#define ICE_TILE_PUSH_LEFT  0x36
#define ICE_TILE_PUSH_RIGHT 0x37
#define ICE_TILE_SWITCH1    0x38 /* 0x38..0x3A set key 1..3 */
#define ICE_TILE_SLOT0      0x3B /* 0x3B..0x3E: with key 0..3 the cell to the right opens */
#define ICE_TILE_DYNAMITE   0x3F
#define ICE_TILE_FLOOR      0x01
#define ICE_TILE_WALL       0x40 /* 0x40..0x7F bounce the cube */
#define ICE_TILE_BREAK_END  0x70 /* blast clears bit-6 tiles below this */
#define ICE_TILE_BLOCK      0x80 /* 0x80..0x89 stop the cube */
#define ICE_TILE_WORM       0x8A /* 0x8A..0x8F deadly; bit-7 tiles animate their low nibble */
#define ICE_TILE_WATER      0x90 /* 0x90.. deadly */

typedef enum {
    IceTileFloor = 0,
    IceTilePawn,
    IceTileExit,
    IceTileMine,
    IceTilePush,
    IceTileSwitch,
    IceTileSlot,
    IceTileDynamite,
    IceTileWall,
    IceTileBlock,
    IceTileWorm,
    IceTileWater,
} IceTileKind;

/* How a stage ended; selects the end-of-level comic panel. */
typedef enum {
    IceOutcomeCleared = 0,
    IceOutcomeTimeOut = 1,
    IceOutcomeWater = 2,
    IceOutcomeWorm = 3,
    IceOutcomeBoom = 4,
    IceOutcomeQuit = 5,
} IceOutcome;

IceTileKind ice_tile_kind(uint8_t raw);
bool ice_tile_deadly(uint8_t raw);
bool ice_tile_breakable(uint8_t raw);
IceOutcome ice_tile_outcome(uint8_t raw);
