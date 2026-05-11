.DEFAULT_GOAL := help

BUILD_DIR := build
SRC_DIRS := src include tests
CXX_FILES := $(shell find $(SRC_DIRS) -type f \( -name '*.cpp' -o -name '*.hpp' \))

.PHONY: help deps format build test clean

help: ## Show this help
	@awk 'BEGIN {FS = ":.*?## "; printf "Usage: make <target>\n\nTargets:\n"} \
		/^[a-zA-Z_-]+:.*?## / {printf "  %-12s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

deps: ## Install build dependencies (Debian/Ubuntu)
	sudo apt update
	sudo apt install -y \
		build-essential \
		cmake \
		pkg-config \
		clang-format \
		libgtest-dev \
		libgstreamer1.0-dev \
		libgstreamer-plugins-base1.0-dev \
		libgstrtspserver-1.0-dev

format: ## Format C++ sources in-place with clang-format
	@clang-format -i $(CXX_FILES)

build: ## Configure and build the library (and tests when BUILD_TESTING=ON)
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j

test: build ## Build then run unit tests via CTest
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean: ## Remove the build directory
	rm -rf $(BUILD_DIR)
