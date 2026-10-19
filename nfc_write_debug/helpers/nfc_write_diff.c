#include "nfc_write_diff.h"

#include <stdio.h>
#include <string.h>

size_t nwd_diff_units(
    const uint8_t* before,
    const uint8_t* after,
    size_t unit_count,
    size_t unit_size,
    NwdChange* out,
    size_t out_cap) {
    if(!before || !after || !out || unit_size == 0 || unit_size > NWD_UNIT_SIZE_MAX) {
        return 0;
    }

    size_t found = 0;
    for(size_t i = 0; i < unit_count && found < out_cap; i++) {
        const uint8_t* b = before + i * unit_size;
        const uint8_t* a = after + i * unit_size;
        if(memcmp(b, a, unit_size) != 0) {
            out[found].index = (uint16_t)i;
            out[found].size = (uint8_t)unit_size;
            memcpy(out[found].old_data, b, unit_size);
            memcpy(out[found].new_data, a, unit_size);
            found++;
        }
    }
    return found;
}

size_t nwd_format_hex(char* buf, size_t buflen, const uint8_t* data, size_t len) {
    if(!buf || buflen == 0) {
        return 0;
    }
    buf[0] = '\0';
    if(!data || len == 0) {
        return 0;
    }

    size_t pos = 0;
    for(size_t i = 0; i < len; i++) {
        int need = (i == 0) ? 2 : 3;
        if(pos + (size_t)need + 1 > buflen) {
            break;
        }
        int n = snprintf(buf + pos, buflen - pos, (i == 0) ? "%02X" : " %02X", (unsigned)data[i]);
        if(n < 0) {
            break;
        }
        pos += (size_t)n;
    }
    return pos;
}

size_t nwd_format_change_entry(
    char* buf,
    size_t buflen,
    uint32_t elapsed_ms,
    const char* unit_name,
    const NwdChange* change) {
    if(!buf || buflen == 0 || !change || !unit_name) {
        return 0;
    }

    char old_hex[3 * NWD_UNIT_SIZE_MAX];
    char new_hex[3 * NWD_UNIT_SIZE_MAX];
    nwd_format_hex(old_hex, sizeof(old_hex), change->old_data, change->size);
    nwd_format_hex(new_hex, sizeof(new_hex), change->new_data, change->size);

    int n = snprintf(
        buf,
        buflen,
        "+%06lums WRITE %s %u\n  old: %s\n  new: %s\n",
        (unsigned long)elapsed_ms,
        unit_name,
        (unsigned)change->index,
        old_hex,
        new_hex);
    if(n < 0) {
        buf[0] = '\0';
        return 0;
    }
    if((size_t)n >= buflen) {
        buf[buflen - 1] = '\0';
        return buflen - 1;
    }
    return (size_t)n;
}

void nwd_sanitize_name(char* out, size_t out_len, const char* in) {
    if(!out || out_len == 0) {
        return;
    }
    if(!in) {
        out[0] = '\0';
        return;
    }

    size_t j = 0;
    for(size_t i = 0; in[i] != '\0' && j + 1 < out_len; i++) {
        char c = in[i];
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-') {
            out[j++] = c;
        } else if(c == ' ' || c == '.') {
            out[j++] = '_';
        }
    }
    if(j == 0) {
        snprintf(out, out_len, "card");
        return;
    }
    out[j] = '\0';
}

size_t nwd_format_log_basename(
    char* buf,
    size_t buflen,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    const char* card_name) {
    if(!buf || buflen == 0) {
        return 0;
    }

    char safe[32];
    nwd_sanitize_name(safe, sizeof(safe), card_name);
    if(safe[0] == '\0') {
        snprintf(safe, sizeof(safe), "card");
    }

    int n = snprintf(
        buf,
        buflen,
        "%04u%02u%02u_%02u%02u%02u_%s.log",
        (unsigned)year,
        (unsigned)month,
        (unsigned)day,
        (unsigned)hour,
        (unsigned)minute,
        (unsigned)second,
        safe);
    if(n < 0) {
        buf[0] = '\0';
        return 0;
    }
    if((size_t)n >= buflen) {
        buf[buflen - 1] = '\0';
        return buflen - 1;
    }
    return (size_t)n;
}
