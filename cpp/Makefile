CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
AR=i686-w64-mingw32-ar

CXXFLAGS += -I../src -DFAUDIOCPP_EXPORTS
LDFLAGS += -Wl,--enable-stdcall-fixup 

# Source lists
XAPO_SRC = XAPOBase.cpp

XAUDIO2_SRC = 	com_utils.cpp \
		xaudio2.cpp \
		XAudio2fx.cpp

X3DAUDIO_SRC = 	X3DAudio.cpp

# Object code lists
XAPO_TARGET = XAPOBase.lib
XAPO_OBJ = $(XAPO_SRC:%.cpp=%.o)

XAUDIO20_TARGET = xaudio2_0.dll
XAUDIO20_OBJ = $(XAUDIO2_SRC:%.cpp=%_x20.o) xaudiodll_x27.o

XAUDIO21_TARGET = xaudio2_1.dll
XAUDIO21_OBJ = $(XAUDIO2_SRC:%.cpp=%_x21.o) xaudiodll_x27.o

XAUDIO22_TARGET = xaudio2_2.dll
XAUDIO22_OBJ = $(XAUDIO2_SRC:%.cpp=%_x22.o) xaudiodll_x27.o

XAUDIO23_TARGET = xaudio2_3.dll
XAUDIO23_OBJ = $(XAUDIO2_SRC:%.cpp=%_x23.o) xaudiodll_x27.o

XAUDIO24_TARGET = xaudio2_4.dll
XAUDIO24_OBJ = $(XAUDIO2_SRC:%.cpp=%_x24.o) xaudiodll_x27.o

XAUDIO25_TARGET = xaudio2_5.dll
XAUDIO25_OBJ = $(XAUDIO2_SRC:%.cpp=%_x25.o) xaudiodll_x27.o

XAUDIO26_TARGET = xaudio2_6.dll
XAUDIO26_OBJ = $(XAUDIO2_SRC:%.cpp=%_x26.o) xaudiodll_x27.o

XAUDIO27_TARGET = xaudio2_7.dll
XAUDIO27_OBJ = $(XAUDIO2_SRC:%.cpp=%_x27.o) xaudiodll_x27.o

XAUDIO28_TARGET = xaudio2_8.dll
XAUDIO28_OBJ = $(XAUDIO2_SRC:%.cpp=%_x28.o) xaudiodll_x29.o

XAUDIO29_TARGET = xaudio2_9.dll
XAUDIO29_OBJ = $(XAUDIO2_SRC:%.cpp=%_x29.o) xaudiodll_x29.o

X3DAUDIO_TARGET = x3daudio.dll
X3DAUDIO_OBJ = $(X3DAUDIO_SRC:%.cpp=%.o)


# targets
%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%_x20.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=0 -c -o $@ $<

%_x21.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=1 -c -o $@ $<

%_x22.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=2 -c -o $@ $<

%_x23.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=3 -c -o $@ $<

%_x24.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=4 -c -o $@ $<

%_x25.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=5 -c -o $@ $<

%_x26.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=6 -c -o $@ $<

%_x27.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=7 -c -o $@ $<

%_x28.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=8 -c -o $@ $<

%_x29.o:%.cpp
	$(CXX) $(CXXFLAGS) -DXAUDIO2_VERSION=9 -c -o $@ $<

all: $(XAUDIO20_TARGET) $(XAUDIO21_TARGET) $(XAUDIO22_TARGET) $(XAUDIO23_TARGET) \
     $(XAUDIO24_TARGET) $(XAUDIO25_TARGET) $(XAUDIO26_TARGET) $(XAUDIO27_TARGET) \
     $(XAUDIO28_TARGET) $(XAUDIO29_TARGET) $(X3DAUDIO_TARGET) $(XAPO_TARGET)

$(XAPO_TARGET): $(XAPO_OBJ)
	$(AR) rcs $@ $<

$(XAUDIO20_TARGET) : $(XAUDIO20_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO20_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO21_TARGET) : $(XAUDIO21_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO21_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO22_TARGET) : $(XAUDIO22_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO22_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO23_TARGET) : $(XAUDIO23_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO23_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO24_TARGET) : $(XAUDIO24_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO24_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO25_TARGET) : $(XAUDIO25_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO25_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO26_TARGET) : $(XAUDIO26_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO26_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO27_TARGET) : $(XAUDIO27_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO27_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO28_TARGET) : $(XAUDIO28_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO28_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(XAUDIO29_TARGET) : $(XAUDIO29_OBJ) $(XAPO_TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(XAUDIO29_OBJ) $(XAPO_TARGET) ../libFAudio.a

$(X3DAUDIO_TARGET) : $(X3DAUDIO_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $(X3DAUDIO_OBJ) ../libFAudio.a

xaudiodll_x27.o: xaudio2_7.def
	i686-w64-mingw32-dlltool -d $< -e $@

xaudiodll_x29.o: xaudio2_9.def
	i686-w64-mingw32-dlltool -d $< -e $@

clean:
	rm -f $(XAUDIO20_TARGET) $(XAUDIO21_TARGET) $(XAUDIO22_TARGET) $(XAUDIO23_TARGET) \
	      $(XAUDIO24_TARGET) $(XAUDIO25_TARGET) $(XAUDIO26_TARGET) $(XAUDIO27_TARGET) \
	      $(XAUDIO28_TARGET) $(XAUDIO29_TARGET) $(XAPO_TARGET) *.o
