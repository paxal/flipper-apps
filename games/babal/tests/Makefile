CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -I..
LDFLAGS ?= -lm

SRCS_MATH = ../render_math.c ../level.c ../levels/levels_data.c ../game.c
OBJS_MATH = $(SRCS_MATH:.c=.o)

.PHONY: all test clean record_golden

all: test

test_perspective: test_perspective.c ../render_math.c
	$(CC) $(CFLAGS) -o $@ test_perspective.c ../render_math.c $(LDFLAGS)

test_lut: test_lut.c ../render_math.c
	$(CC) $(CFLAGS) -o $@ test_lut.c ../render_math.c $(LDFLAGS)

test_span_fill: test_span_fill.c ../render_math.c ../level.c ../levels/levels_data.c ../game.c
	$(CC) $(CFLAGS) -o $@ test_span_fill.c ../render_math.c ../level.c ../levels/levels_data.c ../game.c $(LDFLAGS)

test_invariants: test_invariants.c ../render_math.c ../level.c ../levels/levels_data.c ../game.c
	$(CC) $(CFLAGS) -o $@ test_invariants.c ../render_math.c ../level.c ../levels/levels_data.c ../game.c $(LDFLAGS)

test: test_perspective test_lut test_span_fill test_invariants
	./test_perspective
	./test_lut
	./test_span_fill
	./test_invariants
	@echo "All Babal render tests passed."

record_golden: test_span_fill
	RECORD_GOLDEN=1 ./test_span_fill

clean:
	rm -f test_perspective test_lut test_span_fill test_invariants
	rm -f ../*.o ../levels/*.o
