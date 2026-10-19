#include "game.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG       "Babal"
#define BEST_PATH APP_DATA_PATH("best_ms")
#define TICK_MS   40

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} BabalEvent;

typedef struct {
    BabalGame game;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    bool left_down;
    bool right_down;
    bool up_down;
    bool down_down;
} BabalApp;

static uint32_t babal_load_best(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    uint32_t best = 0;
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, BEST_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[16] = {0};
        if(storage_file_read(file, buf, sizeof(buf) - 1) > 0) {
            best = (uint32_t)strtoul(buf, NULL, 10);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return best;
}

static void babal_save_best(uint32_t best) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, BEST_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[16];
        int n = snprintf(buf, sizeof(buf), "%lu", (unsigned long)best);
        if(n > 0) {
            storage_file_write(file, buf, (size_t)n);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void babal_blit_fb(Canvas* canvas, const BabalSoftFb* fb, bool invert) {
    /* Soft FB bit=1 is "ink" (platforms / ball outline). Default: black void, white ink. */
    canvas_set_color(canvas, invert ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, 0, 0, BABAL_SCREEN_W, BABAL_SCREEN_H);
    canvas_set_color(canvas, invert ? ColorBlack : ColorWhite);
    for(int16_t y = 0; y < BABAL_SCREEN_H; y++) {
        for(int16_t x = 0; x < BABAL_SCREEN_W; x++) {
            if(babal_fb_get(fb, x, y)) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

static void babal_format_time(char* buf, size_t cap, uint32_t ms) {
    uint32_t s = ms / 1000;
    uint32_t cs = (ms % 1000) / 10;
    snprintf(buf, cap, "%lu.%02lu", (unsigned long)s, (unsigned long)cs);
}

static void babal_draw_callback(Canvas* canvas, void* ctx) {
    BabalApp* app = ctx;
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

    BabalGame* g = &app->game;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(g->phase == BabalPhaseTitle) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Babal");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, g->level->name);
        char best[28];
        if(g->best_ms) {
            char t[12];
            babal_format_time(t, sizeof(t), g->best_ms);
            snprintf(best, sizeof(best), "Best %s", t);
        } else {
            snprintf(best, sizeof(best), "Best --");
        }
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, best);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "OK play  <> level");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Up setup  Down about");
    } else if(g->phase == BabalPhaseAbout) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "About");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, "HP-48 Babal by U-Foot");
        canvas_draw_str_aligned(
            canvas, 64, 36, AlignCenter, AlignBottom, "Public domain original");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "Flipper port — paxal");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Back");
    } else if(g->phase == BabalPhaseSetup) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Setup");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            64,
            28,
            AlignCenter,
            AlignBottom,
            g->invert ? "Invert: ON  (OK)" : "Invert: OFF (OK)");
        canvas_draw_str_aligned(
            canvas,
            64,
            40,
            AlignCenter,
            AlignBottom,
            g->bounce ? "Bounce: ON  (Up)" : "Bounce: OFF (Up)");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "Back");
    } else {
        babal_game_render_soft(g);
        babal_blit_fb(canvas, &g->frame, g->invert);

        canvas_set_font(canvas, FontSecondary);
        char hud[24];
        babal_format_time(hud, sizeof(hud), g->elapsed_ms);
        canvas_set_color(canvas, g->invert ? ColorBlack : ColorWhite);
        canvas_draw_str(canvas, 2, 8, hud);

        if(g->phase == BabalPhaseFinished) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 22, 14, 84, 36);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 22, 14, 84, 36);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, "Finish!");
            canvas_set_font(canvas, FontSecondary);
            char line[32];
            babal_format_time(hud, sizeof(hud), g->elapsed_ms);
            snprintf(line, sizeof(line), "Time %s", hud);
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, line);
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "OK again  Back");
        }
    }

    furi_mutex_release(app->mutex);
}

static void babal_input_callback(InputEvent* input_event, void* ctx) {
    BabalApp* app = ctx;
    BabalEvent ev = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void babal_timer_callback(void* ctx) {
    BabalApp* app = ctx;
    BabalEvent ev = {.type = EventTypeTick};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void babal_sync_keys(BabalApp* app) {
    babal_game_set_keys(&app->game, app->left_down, app->right_down, app->up_down, app->down_down);
}

int32_t babal_app(void* p) {
    UNUSED(p);

    BabalApp* app = malloc(sizeof(BabalApp));
    memset(app, 0, sizeof(*app));
    babal_game_init(&app->game);
    app->game.best_ms = babal_load_best();
    app->game.session_best_ms = app->game.best_ms;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(BabalEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, babal_draw_callback, app);
    view_port_input_callback_set(app->view_port, babal_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->timer = furi_timer_alloc(babal_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, TICK_MS);

    bool running = true;
    BabalEvent event;
    while(running) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);

        if(event.type == EventTypeTick) {
            if(app->game.phase == BabalPhasePlaying) {
                babal_game_tick(&app->game, TICK_MS);
                if(app->game.phase == BabalPhaseFinished && app->game.best_ms &&
                   app->game.elapsed_ms == app->game.best_ms) {
                    babal_save_best(app->game.best_ms);
                }
            }
        } else if(event.type == EventTypeKey) {
            InputEvent* in = &event.input;
            bool pressed = (in->type == InputTypePress || in->type == InputTypeRepeat);
            bool released = (in->type == InputTypeRelease);

            if(in->key == InputKeyLeft) {
                if(pressed) app->left_down = true;
                if(released) app->left_down = false;
            } else if(in->key == InputKeyRight) {
                if(pressed) app->right_down = true;
                if(released) app->right_down = false;
            } else if(in->key == InputKeyUp) {
                if(pressed) app->up_down = true;
                if(released) app->up_down = false;
            } else if(in->key == InputKeyDown) {
                if(pressed) app->down_down = true;
                if(released) app->down_down = false;
            }

            babal_sync_keys(app);

            if(in->type == InputTypePress || in->type == InputTypeShort) {
                BabalPhase ph = app->game.phase;
                if(in->key == InputKeyBack) {
                    if(ph == BabalPhaseTitle) {
                        running = false;
                    } else if(
                        ph == BabalPhaseAbout || ph == BabalPhaseSetup ||
                        ph == BabalPhaseFinished || ph == BabalPhasePlaying) {
                        app->game.phase = BabalPhaseTitle;
                        app->left_down = app->right_down = app->up_down = app->down_down = false;
                        babal_sync_keys(app);
                    }
                } else if(in->key == InputKeyOk) {
                    if(ph == BabalPhaseTitle || ph == BabalPhaseFinished) {
                        babal_game_start(&app->game, app->game.level_index);
                    } else if(ph == BabalPhasePlaying) {
                        babal_game_jump(&app->game);
                    } else if(ph == BabalPhaseSetup) {
                        babal_game_toggle_invert(&app->game);
                    }
                } else if(
                    in->key == InputKeyLeft && ph == BabalPhaseTitle &&
                    in->type == InputTypeShort) {
                    babal_game_prev_level(&app->game);
                } else if(
                    in->key == InputKeyRight && ph == BabalPhaseTitle &&
                    in->type == InputTypeShort) {
                    babal_game_next_level(&app->game);
                } else if(
                    in->key == InputKeyUp && ph == BabalPhaseTitle && in->type == InputTypeShort) {
                    app->game.phase = BabalPhaseSetup;
                } else if(
                    in->key == InputKeyDown && ph == BabalPhaseTitle &&
                    in->type == InputTypeShort) {
                    app->game.phase = BabalPhaseAbout;
                } else if(
                    in->key == InputKeyUp && ph == BabalPhaseSetup && in->type == InputTypeShort) {
                    babal_game_toggle_bounce(&app->game);
                }
            }
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
