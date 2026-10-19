#include "../helpers/nfc_write_diff.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(const char* name, bool cond) {
    if(!cond) {
        printf("FAIL: %s\n", name);
        failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

static void test_diff_blocks(void) {
    uint8_t before[3 * 16];
    uint8_t after[3 * 16];
    memset(before, 0, sizeof(before));
    memcpy(after, before, sizeof(before));
    after[16 + 0] = 0xDE;
    after[16 + 1] = 0xAD;

    NwdChange changes[8];
    size_t n = nwd_diff_units(before, after, 3, 16, changes, 8);
    expect_true("one block changed", n == 1);
    expect_true("block index 1", changes[0].index == 1);
    expect_true("old zero", changes[0].old_data[0] == 0x00);
    expect_true("new dead", changes[0].new_data[0] == 0xDE && changes[0].new_data[1] == 0xAD);
}

static void test_diff_pages(void) {
    uint8_t before[2 * 4] = {0};
    uint8_t after[2 * 4] = {0};
    after[4] = 0x11;
    after[5] = 0x22;
    after[6] = 0x33;
    after[7] = 0x44;

    NwdChange changes[4];
    size_t n = nwd_diff_units(before, after, 2, 4, changes, 4);
    expect_true("one page changed", n == 1);
    expect_true("page index 1", changes[0].index == 1);
    expect_true("page size 4", changes[0].size == 4);
}

static void test_format_hex(void) {
    uint8_t data[4] = {0x0A, 0x0B, 0x0C, 0x0D};
    char buf[32];
    size_t n = nwd_format_hex(buf, sizeof(buf), data, 4);
    expect_true("hex len", n == strlen("0A 0B 0C 0D"));
    expect_true("hex text", strcmp(buf, "0A 0B 0C 0D") == 0);
}

static void test_format_entry(void) {
    NwdChange ch = {
        .index = 7,
        .size = 4,
        .old_data = {0, 0, 0, 0},
        .new_data = {1, 2, 3, 4},
    };
    char buf[256];
    size_t n = nwd_format_change_entry(buf, sizeof(buf), 1250, "page", &ch);
    expect_true("entry non-empty", n > 0);
    expect_true("has timestamp", strstr(buf, "+001250ms") != NULL);
    expect_true("has unit", strstr(buf, "WRITE page 7") != NULL);
    expect_true("has old", strstr(buf, "old: 00 00 00 00") != NULL);
    expect_true("has new", strstr(buf, "new: 01 02 03 04") != NULL);
}

static void test_log_basename(void) {
    char buf[96];
    nwd_format_log_basename(buf, sizeof(buf), 2026, 10, 19, 20, 48, 3, "Door_Card");
    expect_true("basename", strcmp(buf, "20261019_204803_Door_Card.log") == 0);
}

static void test_sanitize(void) {
    char out[32];
    nwd_sanitize_name(out, sizeof(out), "Hi There!.nfc");
    expect_true("sanitize", strcmp(out, "Hi_There_nfc") == 0);
    nwd_sanitize_name(out, sizeof(out), "@@@");
    expect_true("sanitize empty -> card", strcmp(out, "card") == 0);
}

int main(void) {
    test_diff_blocks();
    test_diff_pages();
    test_format_hex();
    test_format_entry();
    test_log_basename();
    test_sanitize();
    if(failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
