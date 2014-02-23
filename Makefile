CFLAGS=-g -O -Wall -Wextra

all:

test-driver: json.c test-driver.c
	$(CC) $(CFLAGS) -o $@ json.c test-driver.c

check: test-driver
	./run-tests.sh

clean:
	rm -f test-driver

distclean: clean

.PHONY: all clean distclean check
