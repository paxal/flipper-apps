#include "falldown_game.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TAG "Falldown"
#define HIGHSCORE_PATH APP_DATA_PATH("highscore")
#define TICK_HZ 25

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} FalldownEvent;

typedef struct {
    FalldownState game;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    bool left_down;
    bool right_down;
} FalldownApp;

static uint32_t falldown_load_high_score(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    uint32_t score = 0;
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, HIGHSCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[16] = {0};
        size_t n = storage_file_read(file, buf, sizeof(buf) - 1);
        if(n > 0) {
            score = (uint32_t)strtoul(buf, NULL, 10);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return score;
}

static void falldown_save_high_score(uint32_t score) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, HIGHSCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%lu", (unsigned long)score);
        if(len > 0) {
            storage_file_write(file, buf, (size_t)len);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void falldown_update_move_dir(FalldownApp* app) {
    int8_t dir = 0;
    if(app->left_down && !app->right_down) {
        dir = -1;
    } else if(app->right_down && !app->left_down) {
        dir = 1;
    }
    falldown_game_set_move_dir(&app->game, dir);
}

static void falldown_draw_platforms(Canvas* canvas, const FalldownState* game) {
    for(uint8_t i = 0; i < FALLDOWN_PLATFORM_COUNT; i++) {
        const FalldownPlatform* p = &game->platforms[i];
        int32_t y = (int32_t)p->y;
        if(y >= FALLDOWN_SCREEN_H || y + FALLDOWN_PLATFORM_H < 0) {
            continue;
        }

        if(p->gap_x > 0) {
            canvas_draw_box(canvas, 0, y, p->gap_x, FALLDOWN_PLATFORM_H);
        }
        int32_t right_x = p->gap_x + p->gap_w;
        if(right_x < FALLDOWN_SCREEN_W) {
            canvas_draw_box(
                canvas, right_x, y, FALLDOWN_SCREEN_W - right_x, FALLDOWN_PLATFORM_H);
        }
    }
}

static void falldown_render_callback(Canvas* canvas, void* ctx) {
    FalldownApp* app = ctx;
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

    const FalldownState* game = &app->game;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(game->phase == FalldownPhaseTitle) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignBottom, "Falldown");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignBottom, "Guide the ball down");
        char best[24];
        snprintf(best, sizeof(best), "Best: %lu", (unsigned long)game->high_score);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, best);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "OK start  Back exit");
    } else {
        falldown_draw_platforms(canvas, game);
        canvas_draw_disc(
            canvas, (int32_t)game->ball_x, (int32_t)game->ball_y, FALLDOWN_BALL_R);

        canvas_set_font(canvas, FontSecondary);
        char score_buf[20];
        snprintf(score_buf, sizeof(score_buf), "%lu", (unsigned long)game->score);
        canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, score_buf);

        if(game->phase == FalldownPhaseGameOver) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 28, 16, 72, 34);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 28, 16, 72, 34);

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, "Game Over");
            canvas_set_font(canvas, FontSecondary);
            char line[28];
            snprintf(
                line,
                sizeof(line),
                "Score %lu  Best %lu",
                (unsigned long)game->score,
                (unsigned long)game->high_score);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, line);
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "OK retry");
        }
    }

    furi_mutex_release(app->mutex);
}

static void falldown_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    FalldownEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void falldown_timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    FalldownEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void falldown_handle_key(FalldownApp* app, const InputEvent* input, bool* processing) {
    if(input->key == InputKeyLeft) {
        if(input->type == InputTypePress || input->type == InputTypeRepeat) {
            app->left_down = true;
        } else if(input->type == InputTypeRelease) {
            app->left_down = false;
        }
        falldown_update_move_dir(app);
        return;
    }

    if(input->key == InputKeyRight) {
        if(input->type == InputTypePress || input->type == InputTypeRepeat) {
            app->right_down = true;
        } else if(input->type == InputTypeRelease) {
            app->right_down = false;
        }
        falldown_update_move_dir(app);
        return;
    }

    if(input->type != InputTypeShort) {
        return;
    }

    if(input->key == InputKeyOk) {
        if(app->game.phase == FalldownPhaseTitle ||
           app->game.phase == FalldownPhaseGameOver) {
            if(app->game.phase == FalldownPhaseGameOver) {
                falldown_save_high_score(app->game.high_score);
            }
            app->left_down = false;
            app->right_down = false;
            falldown_game_start(&app->game);
        }
    } else if(input->key == InputKeyBack) {
        if(app->game.phase == FalldownPhasePlaying) {
            app->game.phase = FalldownPhaseTitle;
            app->left_down = false;
            app->right_down = false;
            falldown_game_set_move_dir(&app->game, 0);
        } else if(app->game.phase == FalldownPhaseGameOver) {
            falldown_save_high_score(app->game.high_score);
            app->game.phase = FalldownPhaseTitle;
        } else {
            *processing = false;
        }
    }
}

int32_t falldown_app(void* p) {
    UNUSED(p);
    srand(furi_get_tick());

    FalldownApp* app = malloc(sizeof(FalldownApp));
    furi_check(app);
    memset(app, 0, sizeof(FalldownApp));

    uint32_t high = falldown_load_high_score();
    falldown_game_init(&app->game, high);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(FalldownEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, falldown_render_callback, app);
    view_port_input_callback_set(app->view_port, falldown_input_callback, app->event_queue);

    app->timer = furi_timer_alloc(falldown_timer_callback, FuriTimerTypePeriodic, app->event_queue);
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / TICK_HZ);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    FalldownEvent event;
    for(bool processing = true; processing;) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

            if(event.type == EventTypeKey) {
                falldown_handle_key(app, &event.input, &processing);
            } else if(event.type == EventTypeTick) {
                uint32_t prev_best = app->game.high_score;
                FalldownPhase prev_phase = app->game.phase;
                falldown_game_tick(&app->game);
                if(prev_phase == FalldownPhasePlaying &&
                   app->game.phase == FalldownPhaseGameOver &&
                   app->game.high_score != prev_best) {
                    falldown_save_high_score(app->game.high_score);
                }
            }

            furi_mutex_release(app->mutex);
        }

        view_port_update(app->view_port);
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);

    falldown_save_high_score(app->game.high_score);
    free(app);

    return 0;
}
