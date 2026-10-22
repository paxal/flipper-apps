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
    AppEventTick,
    AppEventKey,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    IceGame game; /* owned by the app thread only */
    FuriMutex* mutex; /* guards `front` against the draw callback */
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    NotificationApp* notifications;
    uint8_t frc_phase;
    uint32_t logic_due; /* furi tick of the next logic step */
    volatile bool tick_pending; /* at most one tick event in the queue */
    bool dirty; /* a key changed the state: render on the next tick */
    bool ok_blast_pending; /* OK pressed in play: swallow the rest of that press */
    uint8_t front; /* xbm buffer shown by the draw callback */
    uint8_t packed[GRAY2BIT_PACKED_SIZE];
    uint8_t xbm[2][GRAY2BIT_XBM_SIZE];
} IceApp;

/* Feedback. The notification service applies the user's volume, vibro and
 * LED settings, so stealth mode silences all of it. */
static const NotificationSequence seq_pawn = {
    &message_note_c7,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_dynamite = {
    &message_blue_255,
    &message_note_e6,
    &message_delay_25,
    &message_sound_off,
    &message_delay_25,
    &message_blue_0,
    NULL,
};

static const NotificationSequence seq_blast = {
    &message_vibro_on,
    &message_note_a4,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_death = {
    &message_red_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_vibro_off,
    &message_sound_off,
    &message_red_0,
    NULL,
};

static const NotificationSequence seq_extra_life = {
    &message_green_255,
    &message_note_c6,
    &message_delay_50,
    &message_note_g6,
    &message_delay_100,
    &message_sound_off,
    &message_green_0,
    NULL,
};

static const NotificationSequence seq_hurry = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

static const NotificationSequence seq_exit_open = {
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_note_e6,
    &message_delay_50,
    &message_sound_off,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

/* One sequence per frame, the most important first, so they never queue up. */
static void play_feedback(IceApp* app) {
    uint16_t ev = ice_game_take_events(&app->game);
    const NotificationSequence* seq = NULL;

    if(ev & IceEventDeath) {
        seq = &seq_death;
    } else if(ev & IceEventCleared) {
        seq = &sequence_success;
    } else if(ev & IceEventExtraLife) {
        seq = &seq_extra_life;
    } else if(ev & IceEventBlast) {
        seq = &seq_blast;
    } else if(ev & IceEventExitOpen) {
        seq = &seq_exit_open;
    } else if(ev & IceEventHurry) {
        seq = &seq_hurry;
    } else if(ev & IceEventDynamite) {
        seq = &seq_dynamite;
    } else if(ev & IceEventPawn) {
        seq = &seq_pawn;
    }
    if(seq != NULL) {
        notification_message(app->notifications, seq);
    }
}

/* Bake the packed frame into the back buffer without any lock, then flip. */
static void present(IceApp* app) {
    uint8_t back = (uint8_t)(app->front ^ 1u);
    GrayMode mode = ice_phase_uses_gray(app->game.phase) ? GrayModeStagger : GrayModeThreshold;

    gray2bit_render_xbm(
        app->packed,
        GRAY2BIT_SCREEN_W,
        GRAY2BIT_SCREEN_H,
        mode,
        app->frc_phase,
        NULL,
        NULL,
        app->xbm[back]);
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    app->front = back;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void ice_draw_callback(Canvas* canvas, void* ctx) {
    IceApp* app = ctx;
    /* Never block the GUI thread on the game thread: if the mutex is not free
     * within a frame, skip this redraw. The game thread only holds it to flip
     * the buffer index, so this practically never happens. */
    if(furi_mutex_acquire(app->mutex, furi_ms_to_ticks(DRAW_MUTEX_TIMEOUT_MS)) != FuriStatusOk) {
        return;
    }
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, GRAY2BIT_SCREEN_W, GRAY2BIT_SCREEN_H, app->xbm[app->front]);
    furi_mutex_release(app->mutex);
}

static void ice_input_callback(InputEvent* input, void* ctx) {
    IceApp* app = ctx;
    AppEvent event = {.type = AppEventKey, .input = *input};
    furi_message_queue_put(app->queue, &event, 0);
}

static void ice_timer_callback(void* ctx) {
    IceApp* app = ctx;
    AppEvent event = {.type = AppEventTick};
    if(app->tick_pending) {
        return; /* the loop is behind; never crowd key events out of the queue */
    }
    app->tick_pending = true;
    if(furi_message_queue_put(app->queue, &event, 0) != FuriStatusOk) {
        app->tick_pending = false;
    }
}

static bool dir_of(InputKey key, IceDir* dir) {
    switch(key) {
    case InputKeyLeft:
        *dir = IceDirLeft;
        return true;
    case InputKeyRight:
        *dir = IceDirRight;
        return true;
    case InputKeyUp:
        *dir = IceDirUp;
        return true;
    case InputKeyDown:
        *dir = IceDirDown;
        return true;
    default:
        return false;
    }
}

static void handle_key(IceApp* app, const InputEvent* input, bool* running) {
    IceGame* game = &app->game;
    IceDir dir;

    if(input->type == InputTypeLong && input->key == InputKeyBack) {
        *running = false;
        return;
    }

    if(input->key == InputKeyOk) {
        if(input->type == InputTypePress && game->phase == IcePhasePlay) {
            ice_game_detonate(game); /* on press, not on release, for a snappy blast */
            app->ok_blast_pending = true;
            return;
        }
        if(app->ok_blast_pending) {
            /* The Short and Release of a blast press must not acknowledge the
             * comic if the stage ended while OK was held. */
            if(input->type == InputTypeRelease) {
                app->ok_blast_pending = false;
            }
            return;
        }
    }

    if(dir_of(input->key, &dir)) {
        if(game->phase == IcePhasePlay) {
            if(input->type == InputTypePress || input->type == InputTypeRepeat) {
                ice_game_key(game, dir, true);
            } else if(input->type == InputTypeRelease) {
                ice_game_key(game, dir, false);
            }
        } else if(
            game->phase == IcePhaseRoute &&
            (input->type == InputTypePress || input->type == InputTypeRepeat)) {
            if(dir == IceDirLeft) {
                ice_game_route_move(game, -1);
            } else if(dir == IceDirRight) {
                ice_game_route_move(game, 1);
            }
        }
        return;
    }

    if(input->type != InputTypeShort) {
        return;
    }

    if(input->key == InputKeyOk) {
        if(game->phase == IcePhasePause) {
            ice_game_resume(game);
        } else {
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
            game->phase = IcePhaseTitle; /* route menu, game over, win */
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
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ice_draw_callback, app);
    view_port_input_callback_set(app->view_port, ice_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    ice_render(&app->game, app->packed);
    present(app);
    app->timer = furi_timer_alloc(ice_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(FRC_TICK_MS));

    AppEvent event;
    for(bool running = true; running;) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        if(event.type == AppEventTick) {
            uint32_t now = furi_get_tick();
            bool stepped = false;
            bool gray;

            app->tick_pending = false;
            if(app->game.phase == IcePhasePlay) {
                /* Logic runs on the wall clock so late or coalesced ticks never slow the game. */
                for(int n = 0; n < 4 && (int32_t)(now - app->logic_due) >= 0; n++) {
                    ice_game_tick(&app->game);
                    app->logic_due += furi_ms_to_ticks(LOGIC_MS);
                    stepped = true;
                }
                if((int32_t)(now - app->logic_due) >= 0) {
                    app->logic_due = now; /* too far behind: drop the backlog */
                }
            } else {
                app->logic_due = now;
            }

            /* Gray screens are static pictures that need a bake every tick for
             * frame-rate control; 1-bit screens only bake when something changed. */
            gray = ice_phase_uses_gray(app->game.phase);
            if(gray) {
                app->frc_phase = (uint8_t)((app->frc_phase + 1) % GRAY2BIT_FRC_PERIOD);
            }
            if(stepped || app->dirty) {
                ice_render(&app->game, app->packed);
                app->dirty = false;
                present(app);
            } else if(gray) {
                present(app);
            }
        } else {
            handle_key(app, &event.input, &running);
            app->dirty = true;
        }
        play_feedback(app);
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
