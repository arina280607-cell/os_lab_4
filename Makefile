CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all clean

all: build/parent build/child1 build/child2

build:
	mkdir -p build

build/parent: src/parent.c src/shared.c include/shared.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/parent.c src/shared.c -o build/parent

build/child1: src/child1.c src/shared.c include/shared.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child1.c src/shared.c -o build/child1

build/child2: src/child2.c src/shared.c include/shared.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/child2.c src/shared.c -o build/child2

build/mmap_demo: tests/mmap_demo.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/mmap_demo.c -o build/mmap_demo

clean:
	rm -rf build