# Makefile for MKP Solver
# Requires GLPK (GNU Linear Programming Kit)
#
# Install GLPK:
#   Ubuntu/Debian: sudo apt-get install libglpk-dev
#   macOS:         brew install glpk
#   Fedora:        sudo dnf install glpk-devel

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lglpk -lm

TARGET  = mkp
SOURCES = main.c mkp.c
HEADERS = mkp.h
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) example_input.txt

clean:
	rm -f $(OBJECTS) $(TARGET)
