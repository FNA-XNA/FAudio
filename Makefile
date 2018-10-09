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
	ifeq ($(COM_WRAPPER), 1)
		CFLAGS += -DFAUDIO_COM_WRAPPER
		LDFLAGS += -lole32
	endif
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
	src/FAPOFX_eq.c \
	src/FAPOFX_masteringlimiter.c \
	src/FAPOFX_reverb.c \
	src/FAPOFX_echo.c \
	src/FAudio_platform_sdl2.c
ifneq ($(COM_WRAPPER), 1)
	FAUDIOSRC += src/XNA_Song.c
endif

# Object code lists
FAUDIOOBJ = $(FAUDIOSRC:%.c=%.o)

# Targets

all: $(FAUDIOOBJ)
	$(CC) $(CFLAGS) -shared -o $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX) $(FAUDIOOBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< `sdl2-config --cflags`

clean:
	rm -f $(FAUDIOOBJ) $(TARGET_PREFIX)FAudio.$(TARGET_SUFFIX) testparse$(UTIL_SUFFIX) facttool$(UTIL_SUFFIX) testreverb$(UTIL_SUFFIX) testfilter$(UTIL_SUFFIX)

.PHONY: testparse facttool testreverb testfilter

testparse:
	$(CC) -g -Wall -pedantic -o testparse$(UTIL_SUFFIX) \
		utils/testparse/testparse.c \
		src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

facttool:
	$(CXX) -g -Wall -o facttool$(UTIL_SUFFIX) \
		utils/facttool/facttool.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

testreverb:
	$(CXX) -g -Wall -o testreverb$(UTIL_SUFFIX) \
		utils/testreverb/*.cpp \
		utils/wavcommon/wavs.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`

testfilter:
	$(CXX) -g -Wall -o testfilter$(UTIL_SUFFIX) \
		utils/testfilter/*.cpp \
		utils/uicommon/*.cpp src/F*.c \
		-Isrc `sdl2-config --cflags --libs`
