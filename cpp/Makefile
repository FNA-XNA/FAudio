CXXFLAGS += -I../src -DFAUDIOCPP_EXPORTS

# Source lists
XAPO_SRC = XAPOBase.cpp

XAUDIO2_SRC = 	com_utils.cpp \
		xaudio2.cpp \
		XAudio2fx.cpp

# Object code lists
XAPO_TARGET = XAPOBase.lib
XAPO_OBJ = $(XAPO_SRC:%.cpp=%.o)

XAUDIO27_TARGET = xaudio2_7.dll
XAUDIO27_OBJ = $(XAUDIO2_SRC:%.cpp=%.o)

# targets
%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(XAUDIO27_TARGET) $(XAPO_TARGET)

$(XAPO_TARGET): $(XAPO_OBJ)
	$(AR) rcs $@ $<

$(XAUDIO27_TARGET) : $(XAUDIO27_OBJ) $(XAPO_TARGET) xaudio27_defs.o
	$(CXX) $(CXXFLAGS) -Wl,--enable-stdcall-fixup -shared -o $@ xaudio27_defs.o $(XAUDIO27_OBJ) $(XAPO_TARGET) ../libFAudio.a 

xaudio27_defs.o: xaudio2_7.def
	i686-w64-mingw32-dlltool -d $< -e $@

clean:
	rm -f $(XAUDIO27_TARGET) $(XAPO_TARGET) *.o
