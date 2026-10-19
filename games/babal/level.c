#include "level.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool babal_level_cell_is_platform(char c) {
    return c != ' ' && c != '\0' && c != '.' && c != '\n' && c != '\r';
}

BabalCellKind babal_level_cell_at(const BabalLevelDef* level, int32_t row, int8_t lane) {
    if(!level || row < 0 || (uint16_t)row >= level->length || lane < 0 ||
       lane >= BABAL_LEVEL_WIDTH) {
        return BabalCellHole;
    }
    return babal_level_cell_is_platform(level->rows[row][lane]) ? BabalCellPlatform :
                                                                  BabalCellHole;
}

bool babal_level_is_safe(const BabalLevelDef* level, float progress, float lane_pos) {
    if(!level) {
        return false;
    }
    int32_t row = (int32_t)progress;
    if(row < 0) {
        row = 0;
    }
    /* lane_pos: 0..8 continuous, center of ball. Map to lane index. */
    int8_t lane = (int8_t)lane_pos;
    if(lane < 0) {
        lane = 0;
    }
    if(lane >= BABAL_LEVEL_WIDTH) {
        lane = BABAL_LEVEL_WIDTH - 1;
    }
    if((uint16_t)row >= level->length) {
        return true; /* past end = finished, treat as safe */
    }
    return babal_level_cell_at(level, row, lane) == BabalCellPlatform;
}

static void skip_ws(const char** p) {
    while(**p == ' ' || **p == '\t' || **p == '\r') {
        (*p)++;
    }
}

bool babal_level_parse_text(
    const char* text,
    char* name_out,
    size_t name_cap,
    char* storage,
    size_t storage_cap,
    const char** row_ptrs,
    uint16_t max_rows,
    uint16_t* row_count) {
    if(!text || !name_out || !storage || !row_ptrs || !row_count || name_cap == 0) {
        return false;
    }
    name_out[0] = '\0';
    *row_count = 0;
    size_t stor_used = 0;
    const char* p = text;

    skip_ws(&p);
    if(strncmp(p, "NAME", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
        p += 4;
        skip_ws(&p);
        size_t ni = 0;
        while(*p && *p != '\n' && *p != '\r' && ni + 1 < name_cap) {
            name_out[ni++] = *p++;
        }
        name_out[ni] = '\0';
        while(*p && *p != '\n') {
            p++;
        }
        if(*p == '\n') {
            p++;
        }
    } else {
        snprintf(name_out, name_cap, "Custom");
    }

    while(*p) {
        if(*p == '\n' || *p == '\r') {
            p++;
            continue;
        }
        if(*p == '#') {
            while(*p && *p != '\n') {
                p++;
            }
            continue;
        }
        char line[16];
        size_t li = 0;
        while(*p && *p != '\n' && *p != '\r' && li + 1 < sizeof(line)) {
            line[li++] = *p++;
        }
        line[li] = '\0';
        while(*p && *p != '\n') {
            p++;
        }
        if(*p == '\n') {
            p++;
        }

        /* Trim trailing spaces for length check — require at least 8 chars of content area. */
        if(li < BABAL_LEVEL_WIDTH) {
            continue;
        }
        if(*row_count >= max_rows) {
            return false;
        }
        if(stor_used + BABAL_LEVEL_WIDTH + 1 > storage_cap) {
            return false;
        }
        char* dest = storage + stor_used;
        memcpy(dest, line, BABAL_LEVEL_WIDTH);
        dest[BABAL_LEVEL_WIDTH] = '\0';
        row_ptrs[*row_count] = dest;
        (*row_count)++;
        stor_used += BABAL_LEVEL_WIDTH + 1;
    }

    return *row_count > 0;
}
