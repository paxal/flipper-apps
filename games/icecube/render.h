#pragma once

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

/* Draw the current screen into a 128x64 gray2bit packed buffer. */
void ice_render(const IceGame* g, uint8_t* packed);

/* True for the static picture screens that need 4 gray levels (title, route,
 * comic). Play, pause and the text screens read fine in 1-bit and are shown
 * thresholded, without frame-rate-control flicker. */
bool ice_phase_uses_gray(IcePhase phase);
