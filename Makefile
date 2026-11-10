CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

COMMON_SRC = src/shared.c src/sync.c
COMMON_HEADERS = include/shared.h include/sync.h

.PHONY: all clean

all: build/parent build/child1 build/child2

build:
	mkdir -p build

build/parent: src/parent.c src/input.c include/input.h $(COMMON_SRC) $(COMMON_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/parent.c src/input.c $(COMMON_SRC) -o build/parent

build/child1: src/child1.c src/text.c include/text.h $(COMMON_SRC) $(COMMON_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child1.c src/text.c $(COMMON_SRC) -o build/child1

build/child2: src/child2.c src/text.c include/text.h $(COMMON_SRC) $(COMMON_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child2.c src/text.c $(COMMON_SRC) -o build/child2

clean:
	rm -rf build
