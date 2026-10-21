#include "helpers/gray2bit.h"
#include "helpers/image_flipper.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <string.h>

#define TAG "GrayPoc"

/** ~60 Hz ticks: TI 2:1 plane schedule repeats at ~20 Hz. */
#define FRC_TICK_MS 16

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} GrayEvent;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    NotificationApp* notifications;

    GrayMode mode;
    uint8_t phase;
    bool show_hud;

    uint8_t ti_dark[GRAY2BIT_XBM_SIZE];
    uint8_t ti_light[GRAY2BIT_XBM_SIZE];
    uint8_t xbm[GRAY2BIT_XBM_SIZE];
} GrayApp;

static void gray_rebuild_frame(GrayApp* app) {
    gray2bit_render_xbm(
        image_flipper_2bit,
        GRAY2BIT_SCREEN_W,
        GRAY2BIT_SCREEN_H,
        app->mode,
        app->phase,
        app->ti_dark,
        app->ti_light,
        app->xbm);
}

static void gray_draw_callback(Canvas* canvas, void* ctx) {
    GrayApp* app = ctx;
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H, app->xbm);

    if(app->show_hud) {
        char line[40];
        snprintf(line, sizeof(line), "%s  <>mode OK/Back", gray2bit_mode_name(app->mode));
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 62, line);
    }

    furi_mutex_release(app->mutex);
}

static void gray_input_callback(InputEvent* input, void* ctx) {
    GrayApp* app = ctx;
    GrayEvent ev = {.type = EventTypeKey, .input = *input};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void gray_timer_callback(void* ctx) {
    GrayApp* app = ctx;
    GrayEvent ev = {.type = EventTypeTick};
    furi_message_queue_put(app->queue, &ev, 0);
}

int32_t grayscale_poc_app(void* p) {
    UNUSED(p);

    GrayApp* app = malloc(sizeof(GrayApp));
    memset(app, 0, sizeof(GrayApp));
    app->mode = GrayModeStagger;
    app->show_hud = true;
    app->phase = 0;

    /* Pre-bake TI planes once — Flipper analogue of HW1 LCD address swap. */
    gray2bit_pack_to_ti_planes(
        image_flipper_2bit, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H, app->ti_dark, app->ti_light);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(GrayEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, gray_draw_callback, app);
    view_port_input_callback_set(app->view_port, gray_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    gray_rebuild_frame(app);

    app->timer = furi_timer_alloc(gray_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, FRC_TICK_MS);

    bool running = true;
    GrayEvent event;
    while(running) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

        if(event.type == EventTypeTick) {
            if(gray2bit_mode_needs_frc(app->mode)) {
                app->phase = (uint8_t)((app->phase + 1) % GRAY2BIT_FRC_PERIOD);
                gray_rebuild_frame(app);
            }
        } else if(event.input.type == InputTypeShort) {
            switch(event.input.key) {
            case InputKeyLeft:
                app->mode = (GrayMode)((app->mode + GrayModeCount - 1) % GrayModeCount);
                app->phase = 0;
                gray_rebuild_frame(app);
                break;
            case InputKeyRight:
            case InputKeyOk:
                app->mode = (GrayMode)((app->mode + 1) % GrayModeCount);
                app->phase = 0;
                gray_rebuild_frame(app);
                break;
            case InputKeyUp:
            case InputKeyDown:
                app->show_hud = !app->show_hud;
                break;
            case InputKeyBack:
                running = false;
                break;
            default:
                break;
            }
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
