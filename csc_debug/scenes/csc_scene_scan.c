#include "../csc_debug.h"

#include <stdio.h>

typedef enum {
    CscScanCustomRefresh = 0xC500,
    CscScanCustomConnectBase = 0xC600,
} CscScanCustom;

static void csc_debug_scan_submenu_callback(void* context, uint32_t index) {
    CscDebugApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void csc_debug_scan_rebuild(CscDebugApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CSC sensors");

    app->device_count =
        csc_worker_snapshot_devices(app->worker, app->devices, CSC_WORKER_MAX_DEVICES);

    if(app->device_count == 0) {
        submenu_add_item(app->submenu, "Scanning... (* = CSC)", 0, NULL, NULL);
        return;
    }

    for(size_t i = 0; i < app->device_count; i++) {
        CscDevice* d = &app->devices[i];
        char label[48];
        const char* name = d->name[0] ? d->name : "Unknown";
        snprintf(
            label,
            sizeof(label),
            "%s%s %ddBm",
            d->has_csc_uuid ? "*" : " ",
            name,
            (int)d->rssi);
        submenu_add_item(
            app->submenu,
            label,
            CscScanCustomConnectBase + (uint32_t)i,
            csc_debug_scan_submenu_callback,
            app);
    }
}

static void csc_debug_scan_worker_callback(CscWorkerEvent* event, void* context) {
    CscDebugApp* app = context;
    app->last_event = *event;

    if(event->type == CscWorkerEventScanUpdated) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CscScanCustomRefresh);
    } else if(event->type == CscWorkerEventError) {
        notification_message(app->notifications, &sequence_error);
    }
}

void csc_debug_scene_scan_on_enter(void* context) {
    CscDebugApp* app = context;
    app->keep_ble_session = false;

    csc_worker_set_callback(app->worker, csc_debug_scan_worker_callback, app);
    csc_worker_set_wheel_circumference_mm(app->worker, app->wheel_circumference_mm);

    csc_debug_scan_rebuild(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewSubmenu);

    if(!csc_worker_start_scan(app->worker)) {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "CSC sensors");
        submenu_add_item(app->submenu, "Scan failed", 0, NULL, NULL);
    }
}

bool csc_debug_scene_scan_on_event(void* context, SceneManagerEvent event) {
    CscDebugApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CscScanCustomRefresh) {
            csc_debug_scan_rebuild(app);
            consumed = true;
        } else if(
            event.event >= CscScanCustomConnectBase &&
            event.event < CscScanCustomConnectBase + CSC_WORKER_MAX_DEVICES) {
            size_t idx = event.event - CscScanCustomConnectBase;
            if(idx < app->device_count) {
                app->selected_device = (int)idx;
                app->keep_ble_session = true;
                if(csc_worker_connect(app->worker, &app->devices[idx])) {
                    scene_manager_next_scene(app->scene_manager, CscDebugSceneLive);
                } else {
                    app->keep_ble_session = false;
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void csc_debug_scene_scan_on_exit(void* context) {
    CscDebugApp* app = context;
    if(!app->keep_ble_session) {
        csc_worker_stop(app->worker);
    }
    submenu_reset(app->submenu);
}
