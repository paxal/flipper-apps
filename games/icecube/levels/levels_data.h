#pragma once

#include <stdint.h>

#define ICE_STAGE_COUNT 12
#define ICE_MAX_MAP_W   65
#define ICE_MAX_MAP_H   32
#define ICE_MAX_CELLS   (ICE_MAX_MAP_W * ICE_MAX_MAP_H)

typedef struct {
    const char* name;
    const uint8_t* cells;
    uint8_t width;
    uint8_t height;
    uint8_t pawns;
    uint16_t time_sec;
} IceStageDef;

extern const IceStageDef ice_stages[ICE_STAGE_COUNT];

/* Sum of per-stage pawn tiles: 300, as the manual says. */
extern const uint16_t ice_pawn_total;
