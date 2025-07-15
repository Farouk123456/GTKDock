# Makefile

# Compiler and flags

CXX = clang++
FLAGS = -std=gnu++20 -g `pkg-config --cflags --libs freetype2 gtkmm-4.0 gtk4-layer-shell-0 x11` -pthread -ldl -lpthread -Wno-deprecated

# C++ sources
SRC = $(wildcard src/*.cpp)
TARGET = GTKDock

.PHONY: all clean shaders

all: $(TARGET)
	@echo "Build completed."

$(TARGET): $(SRC)
	@start=$$(date +%s); \
	$(CXX) $(SRC) $(FLAGS) -o $@; \
	end=$$(date +%s); \
	runtime=$$((end - start)); \
	echo "Compilation took $$runtime seconds."

clean:
	rm -f $(TARGET)
