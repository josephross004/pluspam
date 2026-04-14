SRC		:= src/main.cpp
TARGET 		:= spec_gen
WIN_TARGET	:= spec_gen.exe
MAC_TARGET	:= spec_gen_mac

CXX		:= arm-linux-gnueabihf-g++
CXXFLAGS	:= -std=c++17 -Wall -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard
LDFLAGS 	:= -static -lfftw3f -lm -lpthread

LIB_DIR		:= ./libs
ARMHF_LIBS	:= $(LIB_DIR)/linux_armhf
WIN_LIBS	:= $(LIB_DIR)/win_x64
MAC_LIBS	:= $(LIB_DIR)/mac_arm64

ARM_CXX		:= arm-linux-gnueabihf-g++
ARM_CXXFLAGS	:= -std=c++17 -Wall -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard -I./libs/linux_armhf
ARM_LDFLAGS 	:= -L$(ARMHF_LIBS) -static -lfftw3f -lm -lpthread

# Windows-speciific flags
WIN_CXX		:= x86_64-w64-mingw32-g++
WIN_CXXFLAGS 	:= -std=c++17 -Wall -O3 -march=native -g -I./libs/win_x64
WIN_LDFLAGS  	:= -L$(WIN_LIBS) -lfftw3f -static-libgcc -static-libstdc++  -lm

# mac flags
MAC_CXX		:= aarch64-apple-darwin23.5-clang++
MAC_CXXFLAGS	:= -std=c++17 -Wall -O3 -arch arm64 -mmacosx-version-min=11.0 -g -I./libs/mac_arm64
MAC_LDFLAGS 	:= -L$(MAC_LIBS) -lfftw3f -lm -lpthread

ASM_OUT     	:= $(SRC:.cpp=.s)

OUTPUT      	:= output.hmsb
OUT_HMSP    	:= output.hmsp
INPUT       	:= test_files/48k_test.wav

all: $(TARGET)

win: $(WIN_TARGET)

mac: $(MAC_TARGET)

$(TARGET): $(SRC)
	$(ARM_CXX) $(ARM_CXXFLAGS) $(SRC) -o $(TARGET) $(ARM_LDFLAGS)

$(WIN_TARGET): $(SRC)
	$(WIN_CXX) $(WIN_CXXFLAGS) $(SRC) -o $(WIN_TARGET) $(WIN_LDFLAGS)

$(MAC_TARGET): $(SRC)
	$(MAC_CXX) $(MAC_CXXFLAGS) $(SRC) -o $(MAC_TARGET) $(MAC_LDFLAGS)

clean:
	rm -f $(TARGET) $(WIN_TARGET) $(MAC_TARGET)


.PHONY: run all win mac clean



# Windows pyinstaller: 
# pyinstaller --onefile --noconsole `
    #--add-data "spec_gen.exe;." `
    #--add-data "libs/win_x64/libfftw3f.a;." `
    #src/viewer/gui.py