#include "csc_debug.h"

#include <stdlib.h>
#include <string.h>

#define TAG "CscDebug"

static bool csc_debug_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    CscDebugApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool csc_debug_back_event_callback(void* context) {
    furi_assert(context);
    CscDebugApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static CscDebugApp* csc_debug_app_alloc(void) {
    CscDebugApp* app = malloc(sizeof(CscDebugApp));
    memset(app, 0, sizeof(CscDebugApp));

    app->wheel_circumference_mm = CSC_DEFAULT_WHEEL_CIRCUMFERENCE_MM;
    app->selected_device = -1;
    app->worker = csc_worker_alloc();
    app->unsupported = !csc_worker_is_supported();

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&csc_debug_scene_handlers, app);

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, csc_debug_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, csc_debug_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CscDebugViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CscDebugViewWidget, widget_get_view(app->widget));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CscDebugViewVarList, variable_item_list_get_view(app->var_list));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void csc_debug_app_free(CscDebugApp* app) {
    csc_worker_stop(app->worker);
    csc_worker_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, CscDebugViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, CscDebugViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, CscDebugViewVarList);

    submenu_free(app->submenu);
    widget_free(app->widget);
    variable_item_list_free(app->var_list);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t csc_debug_app(void* p) {
    UNUSED(p);
    CscDebugApp* app = csc_debug_app_alloc();
    scene_manager_next_scene(app->scene_manager, CscDebugSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    csc_debug_app_free(app);
    return 0;
}
