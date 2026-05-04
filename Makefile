# Makefile for MAC0352 EP2 Project
# Builds agent and manager executables

.PHONY: all build agent manager clean help run

# Build directory
BUILD_DIR := build
CMAKE_DIR := build-cmake

# Default target
all: build

help:
	@echo "=== MAC0352 EP2 Makefile ==="
	@echo "Available targets:"
	@echo "  make all       - Build agent and manager (default)"
	@echo "  make build     - Build agent and manager"
	@echo "  make agent     - Build only agent"
	@echo "  make manager   - Build only manager"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make run       - Run the application (python3 run.py)"
	@echo "  make help      - Show this help message"

# Configure CMake if needed
$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..

# Build all targets
build: $(BUILD_DIR)/Makefile
	@echo "Building agent and manager..."
	cd $(BUILD_DIR) && $(MAKE)
	@echo "Build complete!"
	@echo "Executables: $(BUILD_DIR)/agent, $(BUILD_DIR)/manager"

# Build only agent
agent: $(BUILD_DIR)/Makefile
	@echo "Building agent..."
	cd $(BUILD_DIR) && $(MAKE) agent

# Build only manager
manager: $(BUILD_DIR)/Makefile
	@echo "Building manager..."
	cd $(BUILD_DIR) && $(MAKE) manager

# Run the application
run: build
	@python3 run.py

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(CMAKE_DIR)
	rm -f compile_commands.json
	find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
	find . -name "CMakeCache.txt" -delete 2>/dev/null || true
	find . -name "cmake_install.cmake" -delete 2>/dev/null || true
	@echo "Clean complete!"

# Rebuild: clean + build
rebuild: clean build
	@echo "Rebuild complete!"
