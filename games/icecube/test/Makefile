CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O1
ROOT := ..
INCLUDES := -I$(ROOT)
SRCS := $(ROOT)/game.c $(ROOT)/level.c $(ROOT)/render.c $(ROOT)/helpers/gray2bit.c \
        $(ROOT)/levels/levels_data.c $(ROOT)/levels/gfx_data.c

.PHONY: check frames clean

check: test_game
	./test_game

frames: render_frames
	mkdir -p frames && ./render_frames frames

test_game: test_game.c $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

render_frames: render_frames.c $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f test_game render_frames
	rm -rf frames
