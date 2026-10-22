#pragma once

#include <stdint.h>

#define ICE_TILE_COUNT 192
#define ICE_TILE_BYTES 16 /* 8x8 pixels, 2 bits each */
#define ICE_PIC_BYTES  2048 /* 128x64 pixels, 2 bits each */

/* 8x8 tiles from the HP-48 sprite bank, gray2bit packed rows. */
extern const uint8_t ice_tiles[ICE_TILE_COUNT * ICE_TILE_BYTES];

/* Cube sprite: 4 states x 8 rows, each row a mask byte (1 = keep) then a pixel byte (1 = black), LSB = left. */
extern const uint8_t ice_cube_rows[64];

/* Full-screen pictures, gray2bit packed, 128x64. */
extern const uint8_t ice_pic_comic[ICE_PIC_BYTES];
extern const uint8_t ice_pic_route[ICE_PIC_BYTES];
extern const uint8_t ice_pic_pause[ICE_PIC_BYTES];
extern const uint8_t ice_pic_title[ICE_PIC_BYTES];
