CXXFLAGS += -I../src -DFAUDIOCPP_EXPORTS -D_WIN32_WINNT=0x0600 `sdl2-config --cflags`
LDFLAGS += -Wl,--enable-stdcall-fixup `sdl2-config --libs` -static-libgcc -static-libstdc++ -L.. -lFAudio

DLLTOOL ?= dlltool

TARGET ?= $(shell $(CC) -v 2>&1 | grep -i Target:)

ifneq (,$(findstring x86_64,$(TARGET)))
	BUILD_DIR = build_win64
else
	BUILD_DIR = build_win32
endif

# Source lists
XAPO_SRC = XAPOBase.cpp

XAUDIO2_SRC_x20 = \
	com_utils.cpp \
	xaudio2.cpp \
	XAudio2fx.cpp \
	win_registry.cpp

XAUDIO2_SRC_x28 = \
	com_utils.cpp \
	xaudio2.cpp \
	XAudio2fx.cpp \
	win_device.cpp \
	X3DAudio.cpp

X3DAUDIO_SRC = 	X3DAudio.cpp

# Object code lists
XAPO_TARGET = $(BUILD_DIR)/XAPOBase.lib
XAPO_OBJ = $(XAPO_SRC:%.cpp=$(BUILD_DIR)/%.o)

XAUDIO20_TARGET = $(BUILD_DIR)/xaudio2_0.dll
XAUDIO20_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x20.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO21_TARGET = $(BUILD_DIR)/xaudio2_1.dll
XAUDIO21_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x21.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO22_TARGET = $(BUILD_DIR)/xaudio2_2.dll
XAUDIO22_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x22.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO23_TARGET = $(BUILD_DIR)/xaudio2_3.dll
XAUDIO23_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x23.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO24_TARGET = $(BUILD_DIR)/xaudio2_4.dll
XAUDIO24_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x24.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO25_TARGET = $(BUILD_DIR)/xaudio2_5.dll
XAUDIO25_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x25.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO26_TARGET = $(BUILD_DIR)/xaudio2_6.dll
XAUDIO26_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x26.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO27_TARGET = $(BUILD_DIR)/xaudio2_7.dll
XAUDIO27_OBJ = $(XAUDIO2_SRC_x20:%.cpp=$(BUILD_DIR)/%_x27.o) $(BUILD_DIR)/xaudiodll_x20.o

XAUDIO28_TARGET = $(BUILD_DIR)/xaudio2_8.dll
XAUDIO28_OBJ = $(XAUDIO2_SRC_x28:%.cpp=$(BUILD_DIR)/%_x28.o) $(BUILD_DIR)/xaudiodll_x28.o

XAUDIO29_TARGET = $(BUILD_DIR)/xaudio2_9.dll
XAUDIO29_OBJ = $(XAUDIO2_SRC_x28:%.cpp=$(BUILD_DIR)/%_x29.o) $(BUILD_DIR)/xaudiodll_x28.o

X3DAUDIO_TARGET = $(BUILD_DIR)/x3daudio1_7.dll
X3DAUDIO_OBJ = $(X3DAUDIO_SRC:%.cpp=$(BUILD_DIR)/%.o)


# Targets
$(BUILD_DIR)/%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%_x20.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=0 -c -o $@ $<

$(BUILD_DIR)/%_x21.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=1 -c -o $@ $<

$(BUILD_DIR)/%_x22.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=2 -c -o $@ $<

$(BUILD_DIR)/%_x23.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=3 -c -o $@ $<

$(BUILD_DIR)/%_x24.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=4 -c -o $@ $<

$(BUILD_DIR)/%_x25.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=5 -c -o $@ $<

$(BUILD_DIR)/%_x26.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=6 -c -o $@ $<

$(BUILD_DIR)/%_x27.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=7 -c -o $@ $<

$(BUILD_DIR)/%_x28.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=8 -DUNICODE -c -o $@ $<

$(BUILD_DIR)/%_x29.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=9 -DUNICODE -c -o $@ $<

all: $(XAUDIO20_TARGET) $(XAUDIO21_TARGET) $(XAUDIO22_TARGET) $(XAUDIO23_TARGET) \
     $(XAUDIO24_TARGET) $(XAUDIO25_TARGET) $(XAUDIO26_TARGET) $(XAUDIO27_TARGET) \
     $(XAUDIO28_TARGET) $(XAUDIO29_TARGET) $(X3DAUDIO_TARGET) $(XAPO_TARGET)

$(XAPO_TARGET): directories $(XAPO_OBJ)
	$(AR) rcs $@ $(XAPO_OBJ)

$(XAUDIO20_TARGET) : directories $(XAUDIO20_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO20_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO21_TARGET) : directories $(XAUDIO21_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO21_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO22_TARGET) : directories $(XAUDIO22_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO22_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO23_TARGET) : directories $(XAUDIO23_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO23_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO24_TARGET) : directories $(XAUDIO24_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO24_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO25_TARGET) : directories $(XAUDIO25_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO25_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO26_TARGET) : directories $(XAUDIO26_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO26_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO27_TARGET) : directories $(XAUDIO27_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO27_OBJ) $(XAPO_TARGET) $(LDFLAGS)

$(XAUDIO28_TARGET) : directories $(XAUDIO28_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO28_OBJ) $(XAPO_TARGET) $(LDFLAGS) -lsetupapi

$(XAUDIO29_TARGET) : directories $(XAUDIO29_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(XAUDIO29_OBJ) $(XAPO_TARGET) $(LDFLAGS) -lsetupapi

$(X3DAUDIO_TARGET) : directories $(X3DAUDIO_OBJ)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(X3DAUDIO_OBJ) $(LDFLAGS)
	cp $(X3DAUDIO_TARGET) $(BUILD_DIR)/x3daudio1_3.dll
	cp $(X3DAUDIO_TARGET) $(BUILD_DIR)/x3daudio1_4.dll
	cp $(X3DAUDIO_TARGET) $(BUILD_DIR)/x3daudio1_5.dll
	cp $(X3DAUDIO_TARGET) $(BUILD_DIR)/x3daudio1_6.dll

$(BUILD_DIR)/xaudiodll_x20.o: xaudio2_7.def
	$(DLLTOOL) -d $< -e $@

$(BUILD_DIR)/xaudiodll_x28.o: xaudio2_9.def
	$(DLLTOOL) -d $< -e $@

.PHONY: directories clean

directories:
	mkdir -p $(BUILD_DIR)
	cp ../FAudio.dll $(BUILD_DIR)
	cp `sdl2-config --prefix`/bin/SDL2.dll $(BUILD_DIR)

clean:
	rm -f $(XAUDIO20_TARGET) $(XAUDIO21_TARGET) $(XAUDIO22_TARGET) $(XAUDIO23_TARGET) \
	      $(XAUDIO24_TARGET) $(XAUDIO25_TARGET) $(XAUDIO26_TARGET) $(XAUDIO27_TARGET) \
	      $(XAUDIO28_TARGET) $(XAUDIO29_TARGET) $(X3DAUDIO_TARGET) $(XAPO_TARGET) \
	      $(BUILD_DIR)/*.o $(BUILD_DIR)/*.dll
	rmdir $(BUILD_DIR)
