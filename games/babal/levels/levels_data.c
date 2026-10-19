#include "../level.h"

/* Each row is 8 lanes. '*' = platform, ' ' = hole. */

static const char* const k_tutorial_rows[] = {
    "********", "********", "********", "***  ***", "********", "********", "** *****", "********",
    "**** ***", "********", "*** ****", "********", "**  ****", "********", "****  **", "********",
    "*** *** ", "********", "** *****", "********", "********", "***  ***", "********", "**** ***",
    "********", "** *****", "********", "*** ****", "********", "****  **", "********", "********",
};

static const char* const k_holes_rows[] = {
    "********", "***  ***", "********", "**    **", "********", "* **** *", "********", "***  ***",
    "** *****", "***** **", "***  ***", "********", "*  **  *", "********", "***  ***", "****  **",
    "**  ****", "***  ***", "********", "** *****", "***** **", "*** *** ", " *** ***", "********",
    "**  ** *", "* **  **", "********", "***  ***", "********", "**** ***", "** *****", "********",
    "***  ***", "**    **", "********", "********",
};

static const char* const k_jumps_rows[] = {
    "********", "********", "***  ***", "        ", "***  ***", "********", "**** ***", "        ",
    "**** ***", "********", "** *****", "        ", "** *****", "********", "*** ****", "   **   ",
    "*** ****", "********", "****  **", "        ", "****  **", "********", "** *****", " **  ** ",
    "** *****", "********", "***  ***", "        ", "***  ***", "********", "********", "*** *** ",
    "        ", " *** ***", "********", "********",
};

static const char* const k_maze_rows[] = {
    "********", "* ******", "* *** **", "* *** **", "* *   **", "* ***** ", "* ******", "*    ***",
    "**** ***", "***  ***", "*** ****", "**  ****", "** *****", "*  *****", "* ******", "********",
    "****** *", "**** * *", "**   * *", "** *** *", " * *** *", " ***** *", "****** *", "********",
    "***  ***", "** *****", "** *** *", "*  *** *", "**** * *", "****   *", "********", "********",
};

const BabalLevelDef babal_level_tutorial = {
    .name = "Tutorial",
    .rows = k_tutorial_rows,
    .length = (uint16_t)(sizeof(k_tutorial_rows) / sizeof(k_tutorial_rows[0])),
};

const BabalLevelDef babal_level_holes = {
    .name = "Holes",
    .rows = k_holes_rows,
    .length = (uint16_t)(sizeof(k_holes_rows) / sizeof(k_holes_rows[0])),
};

const BabalLevelDef babal_level_jumps = {
    .name = "Jumps",
    .rows = k_jumps_rows,
    .length = (uint16_t)(sizeof(k_jumps_rows) / sizeof(k_jumps_rows[0])),
};

const BabalLevelDef babal_level_maze = {
    .name = "Maze",
    .rows = k_maze_rows,
    .length = (uint16_t)(sizeof(k_maze_rows) / sizeof(k_maze_rows[0])),
};

static const BabalLevelDef* const k_levels[] = {
    &babal_level_tutorial,
    &babal_level_holes,
    &babal_level_jumps,
    &babal_level_maze,
};

const BabalLevelDef* babal_level_get(uint8_t index) {
    if(index >= babal_level_count()) {
        return &babal_level_tutorial;
    }
    return k_levels[index];
}

uint8_t babal_level_count(void) {
    return (uint8_t)(sizeof(k_levels) / sizeof(k_levels[0]));
}
