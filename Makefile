# Makefile - простая обёртка для сборки через cmake
BUILD_DIR := build

.PHONY: all clean editor test

all: editor

editor:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && cmake --build . --config Release

test:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && cmake --build . --config Debug --target test1

clean:
	-rm -rf $(BUILD_DIR)
