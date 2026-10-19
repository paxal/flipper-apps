#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NWD_UNIT_SIZE_MAX 16
#define NWD_CHANGES_MAX   64

typedef struct {
    uint16_t index;
    uint8_t size;
    uint8_t old_data[NWD_UNIT_SIZE_MAX];
    uint8_t new_data[NWD_UNIT_SIZE_MAX];
} NwdChange;

/** Diff fixed-size units (blocks or pages). Returns number of changes written to out. */
size_t nwd_diff_units(
    const uint8_t* before,
    const uint8_t* after,
    size_t unit_count,
    size_t unit_size,
    NwdChange* out,
    size_t out_cap);

/** Hex-encode bytes into buf (no trailing spaces). Returns chars written excluding NUL. */
size_t nwd_format_hex(char* buf, size_t buflen, const uint8_t* data, size_t len);

/**
 * Format one write log entry into buf.
 * Example:
 * +001250ms WRITE block 4
 *   old: 00 11 ...
 *   new: AA BB ...
 */
size_t nwd_format_change_entry(
    char* buf,
    size_t buflen,
    uint32_t elapsed_ms,
    const char* unit_name,
    const NwdChange* change);

/** Build timestamped log basename: YYYYMMDD_HHMMSS_<safe_name>.log */
size_t nwd_format_log_basename(
    char* buf,
    size_t buflen,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second,
    const char* card_name);

/** Sanitize card name for filenames (alnum/_/- only). */
void nwd_sanitize_name(char* out, size_t out_len, const char* in);

#ifdef __cplusplus
}
#endif
