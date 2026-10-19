#include "../csc_debug.h"

#include <stdio.h>

typedef enum {
    CscStartItemScan,
    CscStartItemCircumference,
    CscStartItemAbout,
} CscStartItem;

static void csc_debug_scene_start_submenu_callback(void* context, uint32_t index) {
    CscDebugApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void csc_debug_circ_change(VariableItem* item) {
    CscDebugApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    /* 1900–2300 mm in 25 mm steps (17 values) */
    uint32_t mm = 1900u + (uint32_t)idx * 25u;
    app->wheel_circumference_mm = mm;
    csc_worker_set_wheel_circumference_mm(app->worker, mm);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu mm", (unsigned long)mm);
    variable_item_set_current_value_text(item, buf);
}

static uint32_t csc_debug_varlist_previous(void* context) {
    UNUSED(context);
    return CscDebugViewSubmenu;
}

void csc_debug_scene_start_on_enter(void* context) {
    CscDebugApp* app = context;

    if(app->unsupported) {
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget,
            0,
            0,
            128,
            64,
            "BLE Full stack required.\n\n"
            "Stock Flipper firmware is\n"
            "peripheral-only and cannot\n"
            "scan or connect to CSC\n"
            "sensors.\n\n"
            "Install Moon Firmware\n"
            "(or Blue++) then rebuild\n"
            "this FAP against that SDK.");
        view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewWidget);
        return;
    }

    variable_item_list_reset(app->var_list);
    variable_item_list_set_enter_callback(app->var_list, NULL, NULL);
    view_set_previous_callback(
        variable_item_list_get_view(app->var_list), csc_debug_varlist_previous);

    VariableItem* item =
        variable_item_list_add(app->var_list, "Wheel circ.", 17, csc_debug_circ_change, app);
    uint8_t idx = 8; /* 2100 mm */
    if(app->wheel_circumference_mm >= 1900 && app->wheel_circumference_mm <= 2300) {
        idx = (uint8_t)((app->wheel_circumference_mm - 1900) / 25);
    }
    variable_item_set_current_value_index(item, idx);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu mm", (unsigned long)app->wheel_circumference_mm);
    variable_item_set_current_value_text(item, buf);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CSC Debug");
    submenu_add_item(
        app->submenu, "Scan sensors", CscStartItemScan, csc_debug_scene_start_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "Wheel size",
        CscStartItemCircumference,
        csc_debug_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", CscStartItemAbout, csc_debug_scene_start_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewSubmenu);
}

bool csc_debug_scene_start_on_event(void* context, SceneManagerEvent event) {
    CscDebugApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CscStartItemScan:
            scene_manager_next_scene(app->scene_manager, CscDebugSceneScan);
            consumed = true;
            break;
        case CscStartItemAbout:
            scene_manager_next_scene(app->scene_manager, CscDebugSceneAbout);
            consumed = true;
            break;
        case CscStartItemCircumference:
            view_dispatcher_switch_to_view(app->view_dispatcher, CscDebugViewVarList);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void csc_debug_scene_start_on_exit(void* context) {
    CscDebugApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
