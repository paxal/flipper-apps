#include "../csc_debug.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const csc_debug_scene_on_enter_handlers[])(void*) = {
#include "csc_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const csc_debug_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "csc_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const csc_debug_scene_on_exit_handlers[])(void*) = {
#include "csc_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers csc_debug_scene_handlers = {
    .on_enter_handlers = csc_debug_scene_on_enter_handlers,
    .on_event_handlers = csc_debug_scene_on_event_handlers,
    .on_exit_handlers = csc_debug_scene_on_exit_handlers,
    .scene_num = CscDebugSceneCount,
};
