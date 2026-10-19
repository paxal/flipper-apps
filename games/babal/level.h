#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "render_math.h"

#define BABAL_MAX_LEVEL_ROWS 512
#define BABAL_MAX_LEVELS     8

typedef struct {
    const char* name;
    const char* const* rows;
    uint16_t length;
} BabalLevelDef;

typedef enum {
    BabalCellHole = 0,
    BabalCellPlatform = 1,
} BabalCellKind;

bool babal_level_cell_is_platform(char c);
BabalCellKind babal_level_cell_at(const BabalLevelDef* level, int32_t row, int8_t lane);

/* Sample under ball: lane in [0,7], progress = row + frac. */
bool babal_level_is_safe(const BabalLevelDef* level, float progress, float lane_pos);

const BabalLevelDef* babal_level_get(uint8_t index);
uint8_t babal_level_count(void);

/*
 * Parse a simple custom level text:
 *   NAME <name>
 *   then lines of exactly 8 chars using ' ' or '*' (or '#').
 * Returns false on error. On success fills out_* (rows point into storage).
 */
bool babal_level_parse_text(
    const char* text,
    char* name_out,
    size_t name_cap,
    char* storage,
    size_t storage_cap,
    const char** row_ptrs,
    uint16_t max_rows,
    uint16_t* row_count);

extern const BabalLevelDef babal_level_tutorial;
extern const BabalLevelDef babal_level_holes;
extern const BabalLevelDef babal_level_jumps;
extern const BabalLevelDef babal_level_maze;
