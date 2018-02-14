# Makefile for FACT
# Written by Ethan "flibitijibibo" Lee

# System information
UNAME = $(shell uname)
ARCH = $(shell uname -m)

# Compiler
ifeq ($(UNAME), Darwin)
	CC = cc -arch i386 -arch x86_64 -mmacosx-version-min=10.6
	TARGET = dylib
else
	TARGET = so
endif

CFLAGS += -g -Wall -pedantic -fpic -fPIC -shared

# Source lists
FACTSRC = \
	src/F3DAudio.c \
	src/FACT3D.c \
	src/FAudio.c \
	src/FAudio_internal.c \
	src/FACT.c \
	src/FACT_internal.c \
	src/XNA_Song.c \
	src/e_log.c \
	src/e_log10.c \
	src/FAudio_platform_sdl2.c

# Object code lists
FACTOBJ = $(FACTSRC:%.c=%.o)

# Targets

all: $(FACTOBJ)
	$(CC) $(CFLAGS) -o libFACT.$(TARGET) $(FACTOBJ) `sdl2-config --libs`

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< `sdl2-config --cflags`

clean:
	rm -f $(FACTOBJ) libFACT.$(TARGET)

.PHONY: test tool

test:
	$(CC) -g -Wall -pedantic test/testparse.c src/*.c -Isrc `sdl2-config --cflags --libs`

tool:
	$(CXX) -g -Wall tool/*.c* src/*.c -Isrc `sdl2-config --cflags --libs`
