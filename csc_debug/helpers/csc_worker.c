#include "csc_worker.h"

#include <furi_hal_bt.h>
#include <furi_ble/gatt_client.h>
#include <gap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "CscWorker"
#define CSC_TICK_MS 100

typedef enum {
    CscStateIdle,
    CscStateScanning,
    CscStateConnecting,
    CscStateDiscovering,
    CscStateSubscribing,
    CscStateLive,
    CscStateError,
} CscState;

struct CscWorker {
    FuriMutex* mutex;
    FuriTimer* tick_timer;

    CscWorkerCallback callback;
    void* callback_ctx;

    CscState state;
    uint16_t phase_ticks;
    uint32_t wheel_circumference_mm;

    CscDevice devices[CSC_WORKER_MAX_DEVICES];
    size_t device_count;

    uint8_t target_addr[6];
    uint8_t target_addr_type;
    uint16_t connection_handle;

    BleGattService services[BLE_GATT_CLIENT_MAX_SERVICES];
    uint8_t service_count;
    BleGattCharacteristic chars[BLE_GATT_CLIENT_MAX_CHARS];
    uint8_t char_count;

    uint16_t csc_svc_start;
    uint16_t csc_svc_end;
    uint16_t measurement_handle;
    uint16_t feature_handle;
    uint16_t location_handle;

    bool services_ready;
    bool chars_ready;
    bool subscribed;
    bool gatt_error;

    CscTracker tracker;
    CscWorkerEvent last_event;
};

static void csc_worker_emit(CscWorker* worker, CscWorkerEvent* event) {
    worker->last_event = *event;
    if(worker->callback) {
        worker->callback(event, worker->callback_ctx);
    }
}

static void csc_worker_emit_error(CscWorker* worker, const char* msg) {
    CscWorkerEvent ev = {
        .type = CscWorkerEventError,
        .error_msg = msg,
    };
    csc_worker_emit(worker, &ev);
}

static void csc_worker_parse_adv_name(const uint8_t* adv, uint8_t len, char* name, size_t name_len) {
    name[0] = '\0';
    uint8_t i = 0;
    while(i + 1 < len) {
        uint8_t field_len = adv[i];
        if(field_len == 0 || (uint16_t)i + 1u + field_len > len) {
            break;
        }
        uint8_t type = adv[i + 1];
        if((type == 0x08 || type == 0x09) && field_len > 1) {
            size_t copy = field_len - 1;
            if(copy >= name_len) {
                copy = name_len - 1;
            }
            memcpy(name, &adv[i + 2], copy);
            name[copy] = '\0';
            return;
        }
        i = (uint8_t)(i + 1 + field_len);
    }
}

static int csc_worker_find_device(CscWorker* worker, const uint8_t* addr) {
    for(size_t i = 0; i < worker->device_count; i++) {
        if(memcmp(worker->devices[i].address, addr, 6) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void csc_worker_scan_callback(GapScanResultData* result, void* context) {
    CscWorker* worker = context;
    if(!worker || !result) {
        return;
    }

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    if(worker->state != CscStateScanning) {
        furi_mutex_release(worker->mutex);
        return;
    }

    bool has_csc = csc_adv_has_service_uuid(result->data, result->data_len, CSC_SERVICE_UUID);
    char name[CSC_WORKER_NAME_LEN];
    csc_worker_parse_adv_name(result->data, result->data_len, name, sizeof(name));

    int idx = csc_worker_find_device(worker, result->address);
    if(idx < 0) {
        if(worker->device_count >= CSC_WORKER_MAX_DEVICES) {
            furi_mutex_release(worker->mutex);
            return;
        }
        /* Prefer devices that advertise CSC; still keep unknowns for manual pick. */
        idx = (int)worker->device_count++;
        memset(&worker->devices[idx], 0, sizeof(CscDevice));
        memcpy(worker->devices[idx].address, result->address, 6);
        worker->devices[idx].address_type = result->address_type;
    }

    worker->devices[idx].rssi = result->rssi;
    worker->devices[idx].address_type = result->address_type;
    if(has_csc) {
        worker->devices[idx].has_csc_uuid = true;
    }
    if(name[0] != '\0') {
        strncpy(worker->devices[idx].name, name, CSC_WORKER_NAME_LEN - 1);
    }

    furi_mutex_release(worker->mutex);

    CscWorkerEvent ev = {.type = CscWorkerEventScanUpdated};
    csc_worker_emit(worker, &ev);
}

static void csc_worker_gatt_callback(BleGattClientEvent* event, void* context) {
    CscWorker* worker = context;
    if(!worker || !event) {
        return;
    }

    furi_mutex_acquire(worker->mutex, FuriWaitForever);

    switch(event->type) {
    case BleGattClientEventDiscoverComplete:
        worker->service_count = event->discover.count;
        if(worker->service_count > BLE_GATT_CLIENT_MAX_SERVICES) {
            worker->service_count = BLE_GATT_CLIENT_MAX_SERVICES;
        }
        if(event->discover.services && worker->service_count) {
            memcpy(
                worker->services,
                event->discover.services,
                sizeof(BleGattService) * worker->service_count);
        }
        worker->services_ready = true;
        break;

    case BleGattClientEventCharDiscoverComplete:
        worker->char_count = event->char_discover.count;
        if(worker->char_count > BLE_GATT_CLIENT_MAX_CHARS) {
            worker->char_count = BLE_GATT_CLIENT_MAX_CHARS;
        }
        if(event->char_discover.chars && worker->char_count) {
            memcpy(
                worker->chars,
                event->char_discover.chars,
                sizeof(BleGattCharacteristic) * worker->char_count);
        }
        worker->chars_ready = true;
        break;

    case BleGattClientEventReadComplete:
        if(event->read.value_handle == worker->feature_handle && event->read.data_len >= 1) {
            worker->last_event.has_feature = true;
            worker->last_event.feature = event->read.data[0];
        } else if(event->read.value_handle == worker->location_handle && event->read.data_len >= 1) {
            worker->last_event.has_location = true;
            worker->last_event.sensor_location = event->read.data[0];
        }
        break;

    case BleGattClientEventNotification:
        if(event->notification.value_handle == worker->measurement_handle &&
           event->notification.data && event->notification.data_len > 0) {
            CscMeasurement meas;
            if(csc_measurement_parse(
                   event->notification.data, event->notification.data_len, &meas)) {
                CscDerived derived;
                csc_tracker_update(&worker->tracker, &meas, &derived);

                CscWorkerEvent ev = {
                    .type = CscWorkerEventMeasurement,
                    .derived = derived,
                    .measurement = meas,
                    .raw_len = event->notification.data_len > sizeof(ev.raw) ?
                                   sizeof(ev.raw) :
                                   (uint8_t)event->notification.data_len,
                    .has_feature = worker->last_event.has_feature,
                    .feature = worker->last_event.feature,
                    .has_location = worker->last_event.has_location,
                    .sensor_location = worker->last_event.sensor_location,
                };
                memcpy(ev.raw, event->notification.data, ev.raw_len);
                worker->last_event = ev;
                furi_mutex_release(worker->mutex);
                csc_worker_emit(worker, &ev);
                return;
            }
        }
        break;

    case BleGattClientEventError:
        worker->gatt_error = true;
        break;

    default:
        break;
    }

    furi_mutex_release(worker->mutex);
}

static BleGattService* csc_worker_find_csc_service(CscWorker* worker) {
    for(uint8_t i = 0; i < worker->service_count; i++) {
        if(worker->services[i].uuid_type == 1 &&
           worker->services[i].uuid_16 == CSC_SERVICE_UUID) {
            return &worker->services[i];
        }
    }
    return NULL;
}

static void csc_worker_map_chars(CscWorker* worker) {
    worker->measurement_handle = 0;
    worker->feature_handle = 0;
    worker->location_handle = 0;
    for(uint8_t i = 0; i < worker->char_count; i++) {
        if(worker->chars[i].uuid_type != 1) {
            continue;
        }
        switch(worker->chars[i].uuid_16) {
        case CSC_MEASUREMENT_UUID:
            worker->measurement_handle = worker->chars[i].value_handle;
            break;
        case CSC_FEATURE_UUID:
            worker->feature_handle = worker->chars[i].value_handle;
            break;
        case CSC_SENSOR_LOCATION_UUID:
            worker->location_handle = worker->chars[i].value_handle;
            break;
        default:
            break;
        }
    }
}

static void csc_worker_transition(CscWorker* worker, CscState next) {
    worker->state = next;
    worker->phase_ticks = 0;
}

static void csc_worker_tick(void* ctx) {
    CscWorker* worker = ctx;
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->phase_ticks++;

    switch(worker->state) {
    case CscStateConnecting: {
        uint16_t handle = gap_get_connection_handle_by_role(true);
        if(handle) {
            worker->connection_handle = handle;
            ble_gatt_client_set_callback(handle, csc_worker_gatt_callback, worker);
            ble_gatt_client_exchange_mtu(handle);
            worker->services_ready = false;
            worker->chars_ready = false;
            worker->gatt_error = false;
            csc_tracker_reset(&worker->tracker, worker->wheel_circumference_mm);
            csc_worker_transition(worker, CscStateDiscovering);
            furi_mutex_release(worker->mutex);

            CscWorkerEvent ev = {.type = CscWorkerEventConnected};
            csc_worker_emit(worker, &ev);

            ble_gatt_client_discover_services(handle);
            return;
        }
        if(worker->phase_ticks > 50) { /* ~5 s */
            csc_worker_transition(worker, CscStateError);
            furi_mutex_release(worker->mutex);
            csc_worker_emit_error(worker, "Connect timeout");
            return;
        }
        break;
    }

    case CscStateDiscovering:
        if(worker->gatt_error) {
            csc_worker_transition(worker, CscStateError);
            furi_mutex_release(worker->mutex);
            csc_worker_emit_error(worker, "GATT error");
            return;
        }
        if(worker->services_ready && !worker->chars_ready) {
            BleGattService* svc = csc_worker_find_csc_service(worker);
            if(!svc) {
                csc_worker_transition(worker, CscStateError);
                furi_mutex_release(worker->mutex);
                csc_worker_emit_error(worker, "No CSC service");
                return;
            }
            worker->csc_svc_start = svc->start_handle;
            worker->csc_svc_end = svc->end_handle;
            worker->services_ready = false; /* consume */
            uint16_t handle = worker->connection_handle;
            furi_mutex_release(worker->mutex);
            ble_gatt_client_discover_characteristics(handle, svc);
            return;
        }
        if(worker->chars_ready) {
            csc_worker_map_chars(worker);
            if(!worker->measurement_handle) {
                csc_worker_transition(worker, CscStateError);
                furi_mutex_release(worker->mutex);
                csc_worker_emit_error(worker, "No CSC Measurement");
                return;
            }
            uint16_t handle = worker->connection_handle;
            uint16_t meas = worker->measurement_handle;
            uint16_t feat = worker->feature_handle;
            uint16_t loc = worker->location_handle;
            csc_worker_transition(worker, CscStateSubscribing);
            furi_mutex_release(worker->mutex);
            if(feat) {
                ble_gatt_client_read(handle, feat);
            }
            if(loc) {
                ble_gatt_client_read(handle, loc);
            }
            ble_gatt_client_subscribe_notifications(handle, meas, true);
            return;
        }
        if(worker->phase_ticks > 100) { /* ~10 s */
            csc_worker_transition(worker, CscStateError);
            furi_mutex_release(worker->mutex);
            csc_worker_emit_error(worker, "Discovery timeout");
            return;
        }
        break;

    case CscStateSubscribing:
        /* Subscription is fire-and-forget; move to live after a short delay. */
        if(worker->phase_ticks >= 5) {
            worker->subscribed = true;
            csc_worker_transition(worker, CscStateLive);
        }
        break;

    case CscStateLive: {
        uint16_t handle = gap_get_connection_handle_by_role(true);
        if(!handle) {
            worker->connection_handle = 0;
            csc_worker_transition(worker, CscStateIdle);
            furi_mutex_release(worker->mutex);
            CscWorkerEvent ev = {.type = CscWorkerEventDisconnected};
            csc_worker_emit(worker, &ev);
            return;
        }
        break;
    }

    default:
        break;
    }

    furi_mutex_release(worker->mutex);
}

CscWorker* csc_worker_alloc(void) {
    CscWorker* worker = malloc(sizeof(CscWorker));
    memset(worker, 0, sizeof(*worker));
    worker->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->tick_timer = furi_timer_alloc(csc_worker_tick, FuriTimerTypePeriodic, worker);
    worker->wheel_circumference_mm = CSC_DEFAULT_WHEEL_CIRCUMFERENCE_MM;
    worker->state = CscStateIdle;
    return worker;
}

void csc_worker_free(CscWorker* worker) {
    if(!worker) {
        return;
    }
    csc_worker_stop(worker);
    furi_timer_free(worker->tick_timer);
    furi_mutex_free(worker->mutex);
    free(worker);
}

void csc_worker_set_callback(CscWorker* worker, CscWorkerCallback callback, void* context) {
    furi_check(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->callback = callback;
    worker->callback_ctx = context;
    furi_mutex_release(worker->mutex);
}

void csc_worker_set_wheel_circumference_mm(CscWorker* worker, uint32_t mm) {
    furi_check(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->wheel_circumference_mm = mm ? mm : CSC_DEFAULT_WHEEL_CIRCUMFERENCE_MM;
    worker->tracker.wheel_circumference_mm = worker->wheel_circumference_mm;
    furi_mutex_release(worker->mutex);
}

bool csc_worker_is_supported(void) {
    return furi_hal_bt_get_radio_stack() == FuriHalBtStackFull;
}

bool csc_worker_start_scan(CscWorker* worker) {
    furi_check(worker);
    if(!csc_worker_is_supported()) {
        csc_worker_emit_error(worker, "Need BLE Full stack (Moon/Blue++)");
        return false;
    }

    csc_worker_stop(worker);

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    worker->device_count = 0;
    memset(worker->devices, 0, sizeof(worker->devices));
    worker->services_ready = false;
    worker->chars_ready = false;
    worker->gatt_error = false;
    worker->subscribed = false;
    furi_mutex_release(worker->mutex);

    ble_gatt_client_init();
    gap_set_no_pairing();
    furi_hal_bt_set_scan_callback(csc_worker_scan_callback, worker);

    GapScanParams params = {
        .interval = 0x0050,
        .window = 0x0030,
        .active = true,
        .timeout_ms = 0,
    };

    if(!furi_hal_bt_start_scanning(&params)) {
        furi_hal_bt_set_scan_callback(NULL, NULL);
        csc_worker_emit_error(worker, "Scan start failed");
        return false;
    }

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    csc_worker_transition(worker, CscStateScanning);
    furi_mutex_release(worker->mutex);
    furi_timer_start(worker->tick_timer, furi_ms_to_ticks(CSC_TICK_MS));
    return true;
}

void csc_worker_stop(CscWorker* worker) {
    furi_check(worker);
    furi_timer_stop(worker->tick_timer);

    if(gap_is_activity_active(GapActivityScanning)) {
        furi_hal_bt_stop_scanning();
    }
    furi_hal_bt_set_scan_callback(NULL, NULL);

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    uint16_t handle = worker->connection_handle;
    worker->connection_handle = 0;
    csc_worker_transition(worker, CscStateIdle);
    furi_mutex_release(worker->mutex);

    if(handle) {
        ble_gatt_client_set_callback(handle, NULL, NULL);
        furi_hal_bt_disconnect(handle);
    }
    ble_gatt_client_deinit();
}

size_t csc_worker_snapshot_devices(CscWorker* worker, CscDevice* out, size_t max) {
    furi_check(worker);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);

    /* Sort CSC advertisers first, then by RSSI descending (simple insert). */
    CscDevice tmp[CSC_WORKER_MAX_DEVICES];
    size_t n = worker->device_count;
    memcpy(tmp, worker->devices, sizeof(CscDevice) * n);
    for(size_t i = 1; i < n; i++) {
        CscDevice key = tmp[i];
        int j = (int)i - 1;
        while(j >= 0) {
            bool swap = false;
            if(tmp[j].has_csc_uuid != key.has_csc_uuid) {
                swap = !tmp[j].has_csc_uuid && key.has_csc_uuid;
            } else {
                swap = tmp[j].rssi < key.rssi;
            }
            if(!swap) {
                break;
            }
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }

    size_t copy = n < max ? n : max;
    if(out && copy) {
        memcpy(out, tmp, sizeof(CscDevice) * copy);
    }
    furi_mutex_release(worker->mutex);
    return copy;
}

bool csc_worker_connect(CscWorker* worker, const CscDevice* device) {
    furi_check(worker);
    furi_check(device);

    if(gap_is_activity_active(GapActivityScanning)) {
        furi_hal_bt_stop_scanning();
    }

    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    memcpy(worker->target_addr, device->address, 6);
    worker->target_addr_type = device->address_type;
    worker->services_ready = false;
    worker->chars_ready = false;
    worker->gatt_error = false;
    worker->measurement_handle = 0;
    memset(&worker->last_event, 0, sizeof(worker->last_event));
    csc_worker_transition(worker, CscStateConnecting);
    furi_mutex_release(worker->mutex);

    CscWorkerEvent ev = {.type = CscWorkerEventConnecting};
    csc_worker_emit(worker, &ev);

    if(!furi_hal_bt_connect(device->address_type, device->address)) {
        csc_worker_emit_error(worker, "Connect failed");
        return false;
    }

    furi_timer_start(worker->tick_timer, furi_ms_to_ticks(CSC_TICK_MS));
    return true;
}

void csc_worker_get_last_event(CscWorker* worker, CscWorkerEvent* out) {
    furi_check(worker);
    furi_check(out);
    furi_mutex_acquire(worker->mutex, FuriWaitForever);
    *out = worker->last_event;
    furi_mutex_release(worker->mutex);
}
