#include "../csc_debug.h"
#include "../helpers/csc_format.h"

#include <stdio.h>

typedef enum {
    CscLiveCustomUpdate = 0xC700,
} CscLiveCustom;

static void csc_debug_live_render(CscDebugApp* app) {
    widget_reset(app->widget);

    CscWorkerEvent* ev = &app->last_event;
    FuriString* text = furi_string_alloc();

    if(ev->type == CscWorkerEventConnecting) {
        furi_string_printf(text, "Connecting...");
    } else if(ev->type == CscWorkerEventError) {
        furi_string_printf(text, "Error:\n%s", ev->error_msg ? ev->error_msg : "?");
    } else if(ev->type == CscWorkerEventDisconnected) {
        furi_string_printf(text, "Disconnected");
    } else {
        const char* name = "sensor";
        if(app->selected_device >= 0 && (size_t)app->selected_device < app->device_count) {
            CscDevice* d = &app->devices[app->selected_device];
            name = d->name[0] ? d->name : "sensor";
        }

        furi_string_printf(text, "%s\n", name);

        if(ev->measurement.wheel_present) {
            if(ev->derived.valid && ev->derived.speed_kmh > 0.05f) {
                char speed_buf[16];
                csc_format_float1(speed_buf, sizeof(speed_buf), ev->derived.speed_kmh);
                furi_string_cat_printf(text, "Speed %s km/h\n", speed_buf);
            } else {
                furi_string_cat_printf(text, "Speed --\n");
            }
            furi_string_cat_printf(
                text, "Wheel %lu\n", (unsigned long)ev->measurement.cumulative_wheel_revs);
        }

        if(ev->measurement.crank_present) {
            if(ev->derived.valid && ev->derived.cadence_rpm > 0.5f) {
                char cad_buf[16];
                csc_format_float0(cad_buf, sizeof(cad_buf), ev->derived.cadence_rpm);
                furi_string_cat_printf(text, "Cadence %s rpm\n", cad_buf);
            } else {
                furi_string_cat_printf(text, "Cadence --\n");
            }
            furi_string_cat_printf(text, "Crank %u\n", ev->measurement.cumulative_crank_revs);
        }

        if(ev->has_location) {
            furi_string_cat_printf(
                text, "Loc %s\n", csc_sensor_location_name(ev->sensor_location));
        }

        if(ev->raw_len > 0) {
            furi_string_cat_printf(text, "Raw ");
            for(uint8_t i = 0; i < ev->raw_len && i < 8; i++) {
                furi_string_cat_printf(text, "%02X", ev->raw[i]);
            }
            if(ev->raw_len > 8) {
                furi_string_cat_printf(text, "..");
            }
        }
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);
}

static void csc_debug_live_worker_callback(CscWorkerEvent* event, void* context) {
    CscDebugApp* app = context;
    app->last_event = *event;
    view_dispatcher_send_custom_event(app->view_dispatcher, CscLiveCustomUpdate);

    if(event->type == CscWorkerEventMeasurement) {
        notification_message(app->notifications, &sequence_blink_cyan_100);
    } else if(event->type == CscWorkerEventError || event->type == CscWorkerEventDisconnected) {
        notification_message(app->notifications, &sequence_error);
    }
}

void csc_debug_scene_live_on_enter(void* context) {
    CscDebugApp* app = context;
    app->keep_ble_session = false;
    csc_worker_set_callback(app->worker, csc_debug_live_worker_callback, app);

    app->last_event.type = CscWorkerEventConnecting;
    csc_debug_live_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewWidget);
}

bool csc_debug_scene_live_on_event(void* context, SceneManagerEvent event) {
    CscDebugApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == CscLiveCustomUpdate) {
        csc_debug_live_render(app);
        consumed = true;
    }

    return consumed;
}

void csc_debug_scene_live_on_exit(void* context) {
    CscDebugApp* app = context;
    csc_worker_stop(app->worker);
    widget_reset(app->widget);
}
