CXX      := g++
CXXFLAGS := -std=c++17 -Wall -O3 -march=armv7-a -mfpu=neon -g
LDFLAGS  := -lfftw3f -lm

# Windows-specific flags
WIN_CXXFLAGS := -std=c++17 -Wall -O3 -march=native -g -I./fftw_win
WIN_LDFLAGS  := -L./fftw_win -lfftw3f-3 -lm

TARGET      := spec_gen
WIN_TARGET  := spec_gen.exe
SRC         := src/main.cpp
ASM_OUT     := $(SRC:.cpp=.s)

OUTPUT      := output.hmsb
OUT_HMSP    := output.hmsp
INPUT       := test_files/48k_test.wav

all: $(TARGET)

win: $(WIN_TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(WIN_TARGET): $(SRC)
	$(CXX) $(WIN_CXXFLAGS) $(SRC) -o $(WIN_TARGET) $(WIN_LDFLAGS)

clean:
	rm -f $(TARGET) $(WIN_TARGET)

run: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all  ./$(TARGET) $(INPUT) $(OUTPUT) $(OUT_HMSP) 2> erra.log

nout: $(TARGET)
	./$(TARGET) $(INPUT) $(OUTPUT) $(OUT_HMSP) 2> errn.log

massif: $(TARGET)
	valgrind --tool=massif  ./$(TARGET) $(INPUT) $(OUTPUT) $(OUT_HMSP) 2> errb.log

.PHONY: run all win clean
