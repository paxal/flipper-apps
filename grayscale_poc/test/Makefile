CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O1
ROOT := ..
INCLUDES := -I$(ROOT)/helpers

.PHONY: check clean

check: test_gray2bit
	./test_gray2bit

test_gray2bit: test_gray2bit.c $(ROOT)/helpers/gray2bit.c $(ROOT)/helpers/image_flipper.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f test_gray2bit
