#pragma once

#include "helpers/csc_worker.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>

typedef enum {
    CscDebugViewSubmenu,
    CscDebugViewWidget,
    CscDebugViewVarList,
} CscDebugView;

typedef enum {
    CscDebugSceneStart,
    CscDebugSceneScan,
    CscDebugSceneLive,
    CscDebugSceneAbout,
    CscDebugSceneCount,
} CscDebugScene;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    VariableItemList* var_list;

    CscWorker* worker;
    CscDevice devices[CSC_WORKER_MAX_DEVICES];
    size_t device_count;
    int selected_device;

    uint32_t wheel_circumference_mm;
    CscWorkerEvent last_event;
    bool unsupported;
    bool keep_ble_session;
} CscDebugApp;

extern const SceneManagerHandlers csc_debug_scene_handlers;

void csc_debug_scene_start_on_enter(void* context);
bool csc_debug_scene_start_on_event(void* context, SceneManagerEvent event);
void csc_debug_scene_start_on_exit(void* context);

void csc_debug_scene_scan_on_enter(void* context);
bool csc_debug_scene_scan_on_event(void* context, SceneManagerEvent event);
void csc_debug_scene_scan_on_exit(void* context);

void csc_debug_scene_live_on_enter(void* context);
bool csc_debug_scene_live_on_event(void* context, SceneManagerEvent event);
void csc_debug_scene_live_on_exit(void* context);

void csc_debug_scene_about_on_enter(void* context);
bool csc_debug_scene_about_on_event(void* context, SceneManagerEvent event);
void csc_debug_scene_about_on_exit(void* context);
