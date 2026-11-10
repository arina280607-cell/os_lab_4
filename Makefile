CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

COMMON = src/shared.c src/sync.c src/text.c src/input.c
HEADERS = include/shared.h include/sync.h include/text.h include/input.h

.PHONY: all clean

all: build/parent build/child1 build/child2

build:
	mkdir -p build

build/parent: src/parent.c $(COMMON) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/parent.c $(COMMON) -o build/parent

build/child1: src/child1.c $(COMMON) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child1.c $(COMMON) -o build/child1

build/child2: src/child2.c $(COMMON) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child2.c $(COMMON) -o build/child2

build/mmap_demo: tests/mmap_demo.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/mmap_demo.c -o build/mmap_demo

clean:
	rm -rf build