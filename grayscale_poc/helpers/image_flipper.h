#pragma once

#include <stdint.h>
#include "gray2bit.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Full-screen 2-bit Flipper demo image (packed, see gray2bit_get). */
extern const uint8_t image_flipper_2bit[GRAY2BIT_PACKED_SIZE];

#ifdef __cplusplus
}
#endif
