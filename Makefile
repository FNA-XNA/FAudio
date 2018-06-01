# Makefile for FAudio
# Written by Ethan "flibitijibibo" Lee

# System information
UNAME = $(shell uname)
ARCH = $(shell uname -m)

LDFLAGS += `sdl2-config --libs`

# Detect Windows target
WINDOWS_TARGET=0
ifeq ($(OS), Windows_NT) # cygwin/msys2
	WINDOWS_TARGET=1
endif
ifneq (,$(findstring w64-mingw32,$(CC)))  # mingw-w64 on Linux
	WINDOWS_TARGET=1
endif

# Compiler
ifeq ($(WINDOWS_TARGET),1)
	TARGET_PREFIX = 
	TARGET_SUFFIX = dll
	LDFLAGS += -static-libgcc
else ifeq ($(UNAME), Darwin)
	CC = cc -arch i386 -arch x86_64 -mmacosx-version-min=10.6
	CFLAGS += -fpic -fPIC
	TARGET_PREFIX = lib
	TARGET_SUFFIX = dylib
else
	CFLAGS += -fpic -fPIC
	TARGET_PREFIX = lib
	TARGET_SUFFIX = so
endif

CFLAGS += -g -Wall -pedantic 

# Source lists
FAUDIOSRC = \
	src/F3DAudio.c \
	src/FACT3D.c \
	src/FAPOBase.c \
	src/FAudio.c \
	src/FAudioFX.c \
	src/FAudio_internal.c \
	src/FACT.c \
	src/FACT_internal.c \
	src/XNA_Song.c \
	src/FAudio_platform_sdl2.c \
	src/FAudioFX_internal.c

# Object code lists
FAUDIOOBJ = $(FAUDIOSRC:%.c=%.o)

# Targets

all: $(FAUDIOOBJ)
	$(CC) $(CFLAGS) -shared -o $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX) $(FAUDIOOBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< `sdl2-config --cflags`

clean:
	rm -f $(FAUDIOOBJ) $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX)

.PHONY: test tool

test:
	$(CC) -g -Wall -pedantic test/testparse.c src/F*.c -Isrc `sdl2-config --cflags --libs`

tool:
	$(CXX) -g -Wall tool/*.c* src/*.c -Isrc `sdl2-config --cflags --libs`
