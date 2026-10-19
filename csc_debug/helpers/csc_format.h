#pragma once

#include <stddef.h>
#include <stdio.h>

/** Format a non-negative float with one decimal place into buf (no %f needed). */
static inline void csc_format_float1(char* buf, size_t buflen, float value) {
    if(value < 0) {
        value = 0;
    }
    if(value > 9999.0f) {
        value = 9999.0f;
    }
    unsigned whole = (unsigned)value;
    unsigned frac = (unsigned)((value - (float)whole) * 10.0f + 0.5f);
    if(frac >= 10) {
        whole += 1;
        frac = 0;
    }
    snprintf(buf, buflen, "%u.%u", whole, frac);
}

/** Format a non-negative float as integer (rounded). */
static inline void csc_format_float0(char* buf, size_t buflen, float value) {
    if(value < 0) {
        value = 0;
    }
    if(value > 9999.0f) {
        value = 9999.0f;
    }
    unsigned whole = (unsigned)(value + 0.5f);
    snprintf(buf, buflen, "%u", whole);
}
