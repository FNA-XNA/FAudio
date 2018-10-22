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
ifneq (,$(findstring w64-mingw32,$(CC))) # mingw-w64 on Linux
	WINDOWS_TARGET=1
endif

# Compiler
ifeq ($(WINDOWS_TARGET),1)
	TARGET_PREFIX = 
	TARGET_SUFFIX = dll
	UTIL_SUFFIX = .exe
	LDFLAGS += -static-libgcc
else ifeq ($(UNAME), Darwin)
	CFLAGS += -mmacosx-version-min=10.6 -fpic -fPIC
	TARGET_PREFIX = lib
	TARGET_SUFFIX = dylib
	UTIL_SUFFIX = 
else
	CFLAGS += -fpic -fPIC
	TARGET_PREFIX = lib
	TARGET_SUFFIX = so
	UTIL_SUFFIX = 
endif

CFLAGS += -g -Wall -pedantic

# Source lists
FAUDIOSRC = \
	src/F3DAudio.c \
	src/FAudio.c \
	src/FAudio_internal.c \
	src/FAudio_internal_simd.c \
	src/FAudioFX_reverb.c \
	src/FAudioFX_volumemeter.c \
	src/FACT.c \
	src/FACT3D.c \
	src/FACT_internal.c \
	src/FAPOBase.c \
	src/FAPOFX.c \
	src/FAPOFX_eq.c \
	src/FAPOFX_masteringlimiter.c \
	src/FAPOFX_reverb.c \
	src/FAPOFX_echo.c \
	src/FAudio_platform_sdl2.c
ifneq ($(COM_WRAPPER), 1)
	FAUDIOSRC += src/XNA_Song.c
endif

# FFmpeg for WMA decoding
ifeq ($(FAUDIO_FFMPEG), 1)
	ifdef FAUDIO_FFMPEG_PREFIX
		FFMPEG_CFLAGS = -I$(FAUDIO_FFMPEG_PREFIX)/include 
		FFMPEG_LDFLAGS = -L$(FAUDIO_FFMPEG_PREFIX)/lib -lavcodec -lavutil
	else
		FFMPEG_CFLAGS = `pkg-config libavcodec --cflags` `pkg-config libavutil --cflags`
		FFMPEG_LDFLAGS = `pkg-config libavcodec --libs` `pkg-config libavutil --libs`
	endif

	FAUDIOSRC += src/FAudio_ffmpeg.c
	FFMPEG_CFLAGS += -DHAVE_FFMPEG=1

	CFLAGS += $(FFMPEG_CFLAGS)
	LDFLAGS += $(FFMPEG_LDFLAGS)
endif


# Object code lists
FAUDIOOBJ = $(FAUDIOSRC:%.c=%.o)

# Targets

all: $(FAUDIOOBJ)
	$(CC) $(CFLAGS) -shared -o $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX) $(FAUDIOOBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< `sdl2-config --cflags`

clean:
	rm -f $(FAUDIOOBJ) $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX) testparse$(UTIL_SUFFIX) facttool$(UTIL_SUFFIX) testreverb$(UTIL_SUFFIX) testvolumemeter$(UTIL_SUFFIX) testfilter$(UTIL_SUFFIX)

.PHONY: testparse facttool testreverb testvolumemeter testfilter testxwma

testparse:
	$(CC) -g -Wall -pedantic -o testparse$(UTIL_SUFFIX) \
		utils/testparse/testparse.c \
		src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

facttool:
	$(CXX) -g -Wall $(FFMPEG_CFLAGS) -o facttool$(UTIL_SUFFIX) \
		utils/facttool/facttool.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs` $(FFMPEG_LDFLAGS)

testreverb:
	$(CXX) -g -Wall -o testreverb$(UTIL_SUFFIX) \
		utils/testreverb/*.cpp \
		utils/wavcommon/wavs.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

testvolumemeter:
	$(CXX) -g -Wall -o testvolumemeter$(UTIL_SUFFIX) \
		utils/testvolumemeter/*.cpp \
		utils/wavcommon/wavs.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

testxwma:
	$(CXX) -g -Wall $(FFMPEG_CFLAGS) -o testxwma$(UTIL_SUFFIX) \
		utils/testxwma/*.cpp \
		src/F*.c \
		-Isrc `sdl2-config --cflags --libs` $(FFMPEG_LDFLAGS)

testfilter:
	$(CXX) -g -Wall -o testfilter$(UTIL_SUFFIX) \
		utils/testfilter/*.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`
