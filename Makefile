# Makefile for XYZ Monitor (Modular Version) - Adapted for src/ structure
# Supports both MinGW and MSVC compilers

# Cross-compiler for Windows (MinGW-w64)
CXX = x86_64-w64-mingw32-g++
RC = x86_64-w64-mingw32-windres

# Target executable name
TARGET = xyz_monitor.exe

# Source files (now in src directory)
SOURCES = src/main.cpp src/core.cpp src/logger.cpp src/config.cpp src/converter.cpp

# Object files (put in build directory)
OBJECTS = $(SOURCES:src/%.cpp=build/%.o)

# Resource files
RESOURCE_RC = resources/xyz_monitor.rc
RESOURCE_OBJ = build/xyz_monitor_res.o

# Include directories
INCLUDES = -Isrc

# Compiler flags for MinGW/GCC
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -static-libgcc -static-libstdc++ $(INCLUDES)
LDFLAGS = -static -mwindows
LIBS = -luser32 -lkernel32 -lshell32 -lgdi32

# Debug flags
DEBUG_CXXFLAGS = -std=c++17 -Wall -Wextra -g -DDEBUG $(INCLUDES)
DEBUG_LDFLAGS = -static
DEBUG_LIBS = $(LIBS)

# Default target
all: setup $(TARGET)

# Setup build directory
setup:
	@mkdir -p build logs temp

# Main target with resources
$(TARGET): $(OBJECTS) $(RESOURCE_OBJ)
	$(CXX) $(OBJECTS) $(RESOURCE_OBJ) -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Build completed: $(TARGET)"

# Rule for object files
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile resource file
$(RESOURCE_OBJ): $(RESOURCE_RC)
	@mkdir -p build
	@if [ ! -f "resources/gview.ico" ]; then \
		echo "Warning: gview.ico not found! Using default icon."; \
	fi
	$(RC) -o $@ $<
	@echo "Resource compiled: $(RESOURCE_OBJ)"

# Debug build
debug: CXXFLAGS = $(DEBUG_CXXFLAGS)
debug: LDFLAGS = $(DEBUG_LDFLAGS)
debug: LIBS = $(DEBUG_LIBS)
debug: $(TARGET)

# Build without resources (if .rc file missing)
no-res: $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Build completed without resources: $(TARGET)"

# Clean build artifacts
clean:
	rm -rf build $(TARGET)
	@echo "Cleaned build files"

# Create config file template
config:
	@echo "Creating config.ini template..."
	@echo "hotkey=CTRL+ALT+C" > config.ini
	@echo "hotkey_reverse=CTRL+ALT+G" >> config.ini
	@echo "gview_path=gview.exe" >> config.ini
	@echo "gaussian_clipboard_path=Clipboard.frg" >> config.ini
	@echo "temp_dir=temp" >> config.ini
	@echo "log_file=logs/xyz_monitor.log" >> config.ini
	@echo "log_level=INFO" >> config.ini
	@echo "log_to_console=true" >> config.ini
	@echo "log_to_file=true" >> config.ini
	@echo "wait_seconds=5" >> config.ini
	@echo "max_memory_mb=500" >> config.ini
	@echo "max_clipboard_chars=0" >> config.ini
	@echo "Config template created: config.ini"

# Install (copy to a bin directory)
install: $(TARGET)
	@mkdir -p bin
	cp $(TARGET) bin/
	@if [ -f "config.ini" ]; then cp config.ini bin/; fi
	@if [ -f "resources/gview.ico" ]; then cp resources/gview.ico bin/; fi
	@mkdir -p bin/logs bin/temp
	@echo "Installed to bin directory"

# Full rebuild
rebuild: clean all

# Check for required files
check:
	@echo "Checking required files..."
	@for file in src/main.cpp src/core.cpp src/logger.cpp src/config.cpp src/converter.cpp; do \
		if [ -f "$$file" ]; then echo "✓ $$file found"; else echo "✗ $$file missing!"; fi; \
	done
	@for file in src/core.h src/logger.h src/config.h src/converter.h; do \
		if [ -f "$$file" ]; then echo "✓ $$file found"; else echo "✗ $$file missing!"; fi; \
	done
	@if [ -f "$(RESOURCE_RC)" ]; then echo "✓ $(RESOURCE_RC) found"; else echo "⚠ $(RESOURCE_RC) missing - use 'make no-res'"; fi
	@if [ -f "resources/gview.ico" ]; then echo "✓ gview.ico found"; else echo "⚠ gview.ico missing - using default icon"; fi

# Dependencies
build/main.o: src/main.cpp src/core.h src/logger.h src/config.h src/converter.h
build/core.o: src/core.cpp src/core.h
build/logger.o: src/logger.cpp src/logger.h  
build/config.o: src/config.cpp src/config.h src/logger.h src/core.h
build/converter.o: src/converter.cpp src/converter.h src/logger.h src/core.h

# Mark targets that don't create files
.PHONY: all no-res debug clean install setup config rebuild check help