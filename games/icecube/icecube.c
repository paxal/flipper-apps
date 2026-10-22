#include "game.h"
#include "helpers/gray2bit.h"
#include "render.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include <string.h>

#define FRC_TICK_MS           16
#define DRAW_MUTEX_TIMEOUT_MS 20
#define LOGIC_MS              (1000 / ICE_TICKS_PER_SEC)

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} IceEvent;

typedef struct {
    IceGame game;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    NotificationApp* notifications;
    uint8_t phase;
    uint32_t logic_due; /* furi tick of the next logic step */
    volatile bool tick_pending; /* at most one tick event in the queue */
    uint8_t packed[GRAY2BIT_PACKED_SIZE];
    uint8_t xbm[GRAY2BIT_XBM_SIZE];
} IceApp;

static void bake_frame(IceApp* app) {
    gray2bit_render_xbm(
        app->packed,
        GRAY2BIT_SCREEN_W,
        GRAY2BIT_SCREEN_H,
        GrayModeStagger,
        app->phase,
        NULL,
        NULL,
        app->xbm);
}

static void ice_draw_callback(Canvas* canvas, void* ctx) {
    IceApp* app = ctx;
    /* Never block the GUI thread on the game thread: if the mutex is not free
     * within a frame, skip this redraw. A stuck game thread then still leaves
     * the display and the Back key alive, so a long Back can quit the app. */
    if(furi_mutex_acquire(app->mutex, furi_ms_to_ticks(DRAW_MUTEX_TIMEOUT_MS)) != FuriStatusOk) {
        return;
    }
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H, app->xbm);
    furi_mutex_release(app->mutex);
}

static void ice_input_callback(InputEvent* input, void* ctx) {
    IceApp* app = ctx;
    IceEvent event = {.type = EventTypeKey, .input = *input};
    furi_message_queue_put(app->queue, &event, 0);
}

static void ice_timer_callback(void* ctx) {
    IceApp* app = ctx;
    IceEvent event = {.type = EventTypeTick};
    if(app->tick_pending) {
        return; /* the loop is behind; never crowd key events out of the queue */
    }
    app->tick_pending = true;
    if(furi_message_queue_put(app->queue, &event, 0) != FuriStatusOk) {
        app->tick_pending = false;
    }
}

static void set_dir(IceGame* game, InputKey key, bool down) {
    if(key == InputKeyLeft) {
        ice_game_key(game, IceDirLeft, down);
    } else if(key == InputKeyRight) {
        ice_game_key(game, IceDirRight, down);
    } else if(key == InputKeyUp) {
        ice_game_key(game, IceDirUp, down);
    } else if(key == InputKeyDown) {
        ice_game_key(game, IceDirDown, down);
    }
}

static void handle_key(IceApp* app, const InputEvent* input, bool* running) {
    IceGame* game = &app->game;

    if(input->key == InputKeyLeft || input->key == InputKeyRight || input->key == InputKeyUp ||
       input->key == InputKeyDown) {
        if(input->type == InputTypePress || input->type == InputTypeRepeat) {
            set_dir(game, input->key, true);
        } else if(input->type == InputTypeRelease) {
            set_dir(game, input->key, false);
        }
        return;
    }

    if(input->type == InputTypeLong && input->key == InputKeyBack) {
        *running = false;
        return;
    }
    if(input->key == InputKeyOk && game->phase == IcePhasePlay) {
        if(input->type == InputTypePress) {
            ice_game_detonate(game); /* on press, not on release, for a snappy blast */
        }
        return;
    }
    if(input->type != InputTypeShort) {
        return;
    }

    if(input->key == InputKeyOk) {
        if(game->phase == IcePhasePause) {
            game->phase = IcePhasePlay;
        } else {
            ice_game_set_keys(game, false, false, false, false);
            ice_game_ack(game);
        }
    } else if(input->key == InputKeyBack) {
        ice_game_set_keys(game, false, false, false, false);
        if(game->phase == IcePhasePlay) {
            game->phase = IcePhasePause;
        } else if(game->phase == IcePhasePause) {
            ice_game_quit_stage(game);
        } else if(game->phase == IcePhaseComic) {
            ice_game_ack(game);
        } else if(game->phase == IcePhaseTitle) {
            *running = false;
        } else {
            game->phase = IcePhaseTitle;
        }
    }
}

int32_t icecube_app(void* p) {
    UNUSED(p);

    IceApp* app = malloc(sizeof(IceApp));
    furi_check(app);
    memset(app, 0, sizeof(IceApp));
    ice_game_init(&app->game);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(IceEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ice_draw_callback, app);
    view_port_input_callback_set(app->view_port, ice_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    ice_render(&app->game, app->packed);
    bake_frame(app);
    app->timer = furi_timer_alloc(ice_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, FRC_TICK_MS);

    IceEvent event;
    for(bool running = true; running;) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

        if(event.type == EventTypeTick) {
            uint32_t now = furi_get_tick();
            app->tick_pending = false;
            app->phase = (uint8_t)((app->phase + 1) % GRAY2BIT_FRC_PERIOD);
            if(app->game.phase == IcePhasePlay) {
                /* Logic runs on the wall clock so late or coalesced ticks never slow the game. */
                bool stepped = false;
                for(int n = 0; n < 4 && (int32_t)(now - app->logic_due) >= 0; n++) {
                    ice_game_tick(&app->game);
                    app->logic_due += furi_ms_to_ticks(LOGIC_MS);
                    stepped = true;
                }
                if((int32_t)(now - app->logic_due) >= 0) {
                    app->logic_due = now; /* too far behind: drop the backlog */
                }
                if(stepped) {
                    ice_render(&app->game, app->packed);
                }
            } else {
                app->logic_due = now;
            }
            bake_frame(app);
        } else {
            handle_key(app, &event.input, &running);
            ice_render(&app->game, app->packed);
            bake_frame(app);
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
