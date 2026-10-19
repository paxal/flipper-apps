#pragma once

#include "csc_parse.h"

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#define CSC_WORKER_MAX_DEVICES 24
#define CSC_WORKER_NAME_LEN    20

typedef enum {
    CscWorkerEventScanUpdated,
    CscWorkerEventConnecting,
    CscWorkerEventConnected,
    CscWorkerEventMeasurement,
    CscWorkerEventError,
    CscWorkerEventDisconnected,
} CscWorkerEventType;

typedef struct {
    uint8_t address[6];
    uint8_t address_type;
    int8_t rssi;
    char name[CSC_WORKER_NAME_LEN];
    bool has_csc_uuid;
} CscDevice;

typedef struct {
    CscWorkerEventType type;
    CscDerived derived;
    CscMeasurement measurement;
    uint8_t raw[20];
    uint8_t raw_len;
    uint8_t feature;
    uint8_t sensor_location;
    bool has_feature;
    bool has_location;
    const char* error_msg;
} CscWorkerEvent;

typedef void (*CscWorkerCallback)(CscWorkerEvent* event, void* context);

typedef struct CscWorker CscWorker;

CscWorker* csc_worker_alloc(void);
void csc_worker_free(CscWorker* worker);

void csc_worker_set_callback(CscWorker* worker, CscWorkerCallback callback, void* context);
void csc_worker_set_wheel_circumference_mm(CscWorker* worker, uint32_t mm);

/** Returns false if BLE Full stack is not loaded. */
bool csc_worker_is_supported(void);

bool csc_worker_start_scan(CscWorker* worker);
void csc_worker_stop(CscWorker* worker);

size_t csc_worker_snapshot_devices(CscWorker* worker, CscDevice* out, size_t max);
bool csc_worker_connect(CscWorker* worker, const CscDevice* device);

void csc_worker_get_last_event(CscWorker* worker, CscWorkerEvent* out);
