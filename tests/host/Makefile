CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -I../../include

test: test_telem_format
	./test_telem_format

test_telem_format: test_telem_format.c ../../src/telem_format.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_telem_format
