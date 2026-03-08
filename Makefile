.PHONY: all

CFLAGS := -Wall -Wextra -std=c99 -pedantic

SRC = $(wildcard src/*.c)

all: plea

plea:
	$(CC) $(CFLAGS) -o plea $(SRC)

