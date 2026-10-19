#include "helpers/nfc_write_diff.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "NfcWriteDebug"

#define NWD_NFC_FOLDER EXT_PATH("nfc")
#define NWD_LOG_FOLDER APP_DATA_PATH("logs")
#define NWD_POLL_MS    100
#define NWD_LINE_BUF   512
#define NWD_PATH_MAX   256

typedef enum {
    NwdKindUnsupported = 0,
    NwdKindMfClassic,
    NwdKindMfUltralight,
} NwdCardKind;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} NwdEvent;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    NotificationApp* notifications;

    Nfc* nfc;
    NfcDevice* device;
    NfcListener* listener;
    NfcProtocol protocol;
    NwdCardKind kind;

    Storage* storage;
    File* log_file;
    char log_path[NWD_PATH_MAX];
    char card_path[NWD_PATH_MAX];
    char card_name[64];

    uint8_t* snapshot;
    uint8_t* current;
    size_t unit_count;
    size_t unit_size;
    const char* unit_name;

    uint32_t started_at_ms;
    uint32_t write_events;
    uint32_t units_written;
    bool running;
    bool dirty;

    char status[128];
    char last_line[40];
} NwdApp;

static const char* nwd_path_basename(const char* path) {
    const char* base = path;
    for(const char* p = path; *p; p++) {
        if(*p == '/') {
            base = p + 1;
        }
    }
    return base;
}

static void nwd_strip_ext(char* name) {
    char* dot = strrchr(name, '.');
    if(dot) {
        *dot = '\0';
    }
}

static bool nwd_log_write(NwdApp* app, const char* text) {
    if(!app->log_file || !text) {
        return false;
    }
    size_t len = strlen(text);
    return storage_file_write(app->log_file, text, len) == len;
}

static bool nwd_open_log(NwdApp* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char basename[96];
    nwd_format_log_basename(
        basename,
        sizeof(basename),
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        app->card_name);

    storage_simply_mkdir(app->storage, STORAGE_APP_DATA_PATH_PREFIX);
    storage_simply_mkdir(app->storage, NWD_LOG_FOLDER);

    snprintf(app->log_path, sizeof(app->log_path), "%s/%s", NWD_LOG_FOLDER, basename);

    app->log_file = storage_file_alloc(app->storage);
    if(!storage_file_open(app->log_file, app->log_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(app->log_file);
        app->log_file = NULL;
        return false;
    }

    char line[96];
    nwd_log_write(app, "# nfc_write_debug\n# card: ");
    nwd_log_write(app, app->card_path);
    nwd_log_write(app, "\n# protocol: ");
    nwd_log_write(app, nfc_device_get_protocol_name(app->protocol));
    nwd_log_write(app, "\n");
    snprintf(
        line,
        sizeof(line),
        "# started: %04u-%02u-%02u %02u:%02u:%02u\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);
    nwd_log_write(app, line);
    nwd_log_write(app, "# note: RF ACK/NACK is handled by the protocol listener.\n");
    nwd_log_write(app, "#       Emulated memory updates in-place (no restart needed).\n#\n");
    return true;
}

static void nwd_close_log(NwdApp* app) {
    if(!app->log_file) {
        return;
    }
    char footer[96];
    uint32_t elapsed = furi_get_tick() - app->started_at_ms;
    snprintf(
        footer,
        sizeof(footer),
        "+%06lums SESSION_END write_events=%lu units=%lu dirty=%d\n",
        (unsigned long)elapsed,
        (unsigned long)app->write_events,
        (unsigned long)app->units_written,
        app->dirty ? 1 : 0);
    nwd_log_write(app, footer);
    storage_file_sync(app->log_file);
    storage_file_close(app->log_file);
    storage_file_free(app->log_file);
    app->log_file = NULL;
}

static bool nwd_prepare_buffers(NwdApp* app) {
    if(app->kind == NwdKindMfClassic) {
        const MfClassicData* data =
            (const MfClassicData*)nfc_device_get_data(app->device, NfcProtocolMfClassic);
        app->unit_count = mf_classic_get_total_block_num(data->type);
        app->unit_size = MF_CLASSIC_BLOCK_SIZE;
        app->unit_name = "block";
    } else if(app->kind == NwdKindMfUltralight) {
        const MfUltralightData* data =
            (const MfUltralightData*)nfc_device_get_data(app->device, NfcProtocolMfUltralight);
        app->unit_count = data->pages_total ? data->pages_total : data->pages_read;
        if(app->unit_count == 0) {
            app->unit_count = mf_ultralight_get_pages_total(data->type);
        }
        app->unit_size = MF_ULTRALIGHT_PAGE_SIZE;
        app->unit_name = "page";
    } else {
        return false;
    }

    size_t bytes = app->unit_count * app->unit_size;
    app->snapshot = malloc(bytes);
    app->current = malloc(bytes);
    if(!app->snapshot || !app->current) {
        free(app->snapshot);
        free(app->current);
        app->snapshot = NULL;
        app->current = NULL;
        return false;
    }
    memset(app->snapshot, 0, bytes);
    memset(app->current, 0, bytes);
    return true;
}

static void nwd_copy_units_from_classic(uint8_t* dst, const MfClassicData* data, size_t count) {
    for(size_t i = 0; i < count; i++) {
        memcpy(dst + i * MF_CLASSIC_BLOCK_SIZE, data->block[i].data, MF_CLASSIC_BLOCK_SIZE);
    }
}

static void
    nwd_copy_units_from_ultralight(uint8_t* dst, const MfUltralightData* data, size_t count) {
    for(size_t i = 0; i < count; i++) {
        memcpy(dst + i * MF_ULTRALIGHT_PAGE_SIZE, data->page[i].data, MF_ULTRALIGHT_PAGE_SIZE);
    }
}

static void nwd_load_snapshot_from_device(NwdApp* app) {
    if(app->kind == NwdKindMfClassic) {
        const MfClassicData* data =
            (const MfClassicData*)nfc_device_get_data(app->device, NfcProtocolMfClassic);
        nwd_copy_units_from_classic(app->snapshot, data, app->unit_count);
    } else if(app->kind == NwdKindMfUltralight) {
        const MfUltralightData* data =
            (const MfUltralightData*)nfc_device_get_data(app->device, NfcProtocolMfUltralight);
        nwd_copy_units_from_ultralight(app->snapshot, data, app->unit_count);
    }
    memcpy(app->current, app->snapshot, app->unit_count * app->unit_size);
}

static void nwd_load_current_from_listener(NwdApp* app) {
    const NfcDeviceData* live = nfc_listener_get_data(app->listener, app->protocol);
    if(app->kind == NwdKindMfClassic) {
        nwd_copy_units_from_classic(app->current, (const MfClassicData*)live, app->unit_count);
    } else if(app->kind == NwdKindMfUltralight) {
        nwd_copy_units_from_ultralight(
            app->current, (const MfUltralightData*)live, app->unit_count);
    }
}

static void nwd_apply_listener_to_device(NwdApp* app) {
    const NfcDeviceData* live = nfc_listener_get_data(app->listener, app->protocol);
    nfc_device_set_data(app->device, app->protocol, live);
}

static void nwd_poll_writes(NwdApp* app) {
    if(!app->running || !app->listener) {
        return;
    }

    nwd_load_current_from_listener(app);

    NwdChange changes[NWD_CHANGES_MAX];
    size_t n = nwd_diff_units(
        app->snapshot, app->current, app->unit_count, app->unit_size, changes, NWD_CHANGES_MAX);

    if(n == 0) {
        return;
    }

    uint32_t elapsed = furi_get_tick() - app->started_at_ms;
    char entry[NWD_LINE_BUF];

    for(size_t i = 0; i < n; i++) {
        size_t len =
            nwd_format_change_entry(entry, sizeof(entry), elapsed, app->unit_name, &changes[i]);
        if(len > 0) {
            nwd_log_write(app, entry);
        }
        snprintf(
            app->last_line,
            sizeof(app->last_line),
            "%s %u",
            app->unit_name,
            (unsigned)changes[i].index);
    }

    memcpy(app->snapshot, app->current, app->unit_count * app->unit_size);
    app->write_events += 1;
    app->units_written += (uint32_t)n;
    app->dirty = true;

    /* Keep NfcDevice in sync with live emulated memory (no listener restart). */
    nwd_apply_listener_to_device(app);

    notification_message(app->notifications, &sequence_semi_success);
    view_port_update(app->view_port);
}

static void nwd_draw_callback(Canvas* canvas, void* ctx) {
    NwdApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "NFC Write Debug");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, app->card_name);

    char line[48];
    snprintf(
        line,
        sizeof(line),
        "Writes:%lu Units:%lu",
        (unsigned long)app->write_events,
        (unsigned long)app->units_written);
    canvas_draw_str(canvas, 2, 34, line);

    if(app->last_line[0]) {
        canvas_draw_str(canvas, 2, 46, app->last_line);
    } else {
        canvas_draw_str(canvas, 2, 46, "Waiting for reader writes");
    }

    canvas_draw_str(canvas, 2, 58, "Back=stop  log in apps_data");
}

static void nwd_input_callback(InputEvent* event, void* ctx) {
    NwdApp* app = ctx;
    NwdEvent ev = {.type = EventTypeKey, .input = *event};
    furi_message_queue_put(app->event_queue, &ev, 0);
}

static void nwd_timer_callback(void* ctx) {
    NwdApp* app = ctx;
    NwdEvent ev = {.type = EventTypeTick};
    furi_message_queue_put(app->event_queue, &ev, 0);
}

static NwdCardKind nwd_kind_from_protocol(NfcProtocol protocol) {
    if(protocol == NfcProtocolMfClassic) {
        return NwdKindMfClassic;
    }
    if(protocol == NfcProtocolMfUltralight) {
        return NwdKindMfUltralight;
    }
    return NwdKindUnsupported;
}

static bool nwd_pick_and_load(NwdApp* app) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    FuriString* path = furi_string_alloc_set(NWD_NFC_FOLDER);

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".nfc", NULL);
    options.base_path = NWD_NFC_FOLDER;
    options.hide_ext = false;

    bool ok = false;
    if(dialog_file_browser_show(dialogs, path, path, &options)) {
        size_t path_len = furi_string_size(path);
        if(path_len >= sizeof(app->card_path)) {
            path_len = sizeof(app->card_path) - 1;
        }
        memcpy(app->card_path, furi_string_get_cstr(path), path_len);
        app->card_path[path_len] = '\0';
        const char* base = nwd_path_basename(app->card_path);
        char raw_name[64];
        size_t base_len = strlen(base);
        if(base_len >= sizeof(raw_name)) {
            base_len = sizeof(raw_name) - 1;
        }
        memcpy(raw_name, base, base_len);
        raw_name[base_len] = '\0';
        nwd_strip_ext(raw_name);
        nwd_sanitize_name(app->card_name, sizeof(app->card_name), raw_name);
        if(app->card_name[0] == '\0') {
            memcpy(app->card_name, "card", 5);
        }

        if(nfc_device_load(app->device, app->card_path)) {
            app->protocol = nfc_device_get_protocol(app->device);
            app->kind = nwd_kind_from_protocol(app->protocol);
            ok = (app->kind != NwdKindUnsupported);
            if(!ok) {
                snprintf(
                    app->status,
                    sizeof(app->status),
                    "Need Classic/UL, got %s",
                    nfc_device_get_protocol_name(app->protocol));
            }
        } else {
            snprintf(app->status, sizeof(app->status), "Failed to load .nfc");
        }
    } else {
        snprintf(app->status, sizeof(app->status), "Cancelled");
    }

    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);
    return ok;
}

static void nwd_show_message(const char* text) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* msg = dialog_message_alloc();
    dialog_message_set_header(msg, "NFC Write Debug", 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(msg, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(dialogs, msg);
    dialog_message_free(msg);
    furi_record_close(RECORD_DIALOGS);
}

static bool nwd_start_emulate(NwdApp* app) {
    if(!nwd_prepare_buffers(app)) {
        snprintf(app->status, sizeof(app->status), "Out of memory");
        return false;
    }
    if(!nwd_open_log(app)) {
        snprintf(app->status, sizeof(app->status), "Cannot open log");
        return false;
    }

    nwd_load_snapshot_from_device(app);

    char start[128];
    snprintf(
        start,
        sizeof(start),
        "+000000ms SESSION_START %s_count=%u unit=%u\n",
        app->unit_name,
        (unsigned)app->unit_count,
        (unsigned)app->unit_size);
    nwd_log_write(app, start);

    const NfcDeviceData* data = nfc_device_get_data(app->device, app->protocol);
    app->listener = nfc_listener_alloc(app->nfc, app->protocol, data);
    nfc_listener_start(app->listener, NULL, NULL);

    app->started_at_ms = furi_get_tick();
    app->running = true;
    app->write_events = 0;
    app->units_written = 0;
    app->dirty = false;
    app->last_line[0] = '\0';

    notification_message(app->notifications, &sequence_blink_start_cyan);
    return true;
}

static void nwd_stop_emulate(NwdApp* app) {
    if(!app->running) {
        return;
    }

    if(app->listener) {
        /* Final poll while still marked running, then tear down. */
        nwd_poll_writes(app);
        app->running = false;
        nfc_listener_stop(app->listener);
        nwd_apply_listener_to_device(app);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    } else {
        app->running = false;
    }

    notification_message(app->notifications, &sequence_blink_stop);
    nwd_close_log(app);

    /* Persist mutated card under app data for inspection. */
    if(app->dirty) {
        char save_path[NWD_PATH_MAX];
        snprintf(
            save_path, sizeof(save_path), "%s/%s_mutated.nfc", NWD_LOG_FOLDER, app->card_name);
        if(nfc_device_save(app->device, save_path)) {
            FURI_LOG_I(TAG, "Saved mutated card to %s", save_path);
        }
    }
}

static void nwd_free_buffers(NwdApp* app) {
    free(app->snapshot);
    free(app->current);
    app->snapshot = NULL;
    app->current = NULL;
}

int32_t nfc_write_debug_app(void* p) {
    UNUSED(p);

    NwdApp* app = malloc(sizeof(NwdApp));
    furi_check(app);
    memset(app, 0, sizeof(NwdApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(NwdEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, nwd_draw_callback, app);
    view_port_input_callback_set(app->view_port, nwd_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->nfc = nfc_alloc();
    app->device = nfc_device_alloc();

    if(!nwd_pick_and_load(app)) {
        if(app->status[0]) {
            nwd_show_message(app->status);
        }
        goto cleanup;
    }

    if(!nwd_start_emulate(app)) {
        nwd_show_message(app->status);
        goto cleanup;
    }

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->timer = furi_timer_alloc(nwd_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, NWD_POLL_MS);

    bool exit = false;
    while(!exit) {
        NwdEvent event;
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        if(event.type == EventTypeTick) {
            nwd_poll_writes(app);
        } else if(event.type == EventTypeKey) {
            if(event.input.type == InputTypeShort && event.input.key == InputKeyBack) {
                exit = true;
            }
        }
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
    gui_remove_view_port(app->gui, app->view_port);

    nwd_stop_emulate(app);

    char done[160];
    snprintf(
        done,
        sizeof(done),
        "Writes: %lu\nSaved log under\napps_data/nfc_write_debug/logs",
        (unsigned long)app->write_events);
    nwd_show_message(done);

cleanup:
    nwd_free_buffers(app);
    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
    }
    nfc_device_free(app->device);
    nfc_free(app->nfc);

    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
