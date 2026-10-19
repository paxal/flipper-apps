#include "../csc_debug.h"

void csc_debug_scene_about_on_enter(void* context) {
    CscDebugApp* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "CSC Debug\n"
        "BLE Cycling Speed &\n"
        "Cadence debugger\n\n"
        "Service 0x1816\n"
        "Meas 0x2A5B\n\n"
        "Requires BLE Full stack\n"
        "(Moon Firmware / Blue++)\n\n"
        "* in scan = advertises\n"
        "CSC UUID. Speed uses\n"
        "wheel circumference.");
    view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewWidget);
}

bool csc_debug_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void csc_debug_scene_about_on_exit(void* context) {
    CscDebugApp* app = context;
    widget_reset(app->widget);
}
