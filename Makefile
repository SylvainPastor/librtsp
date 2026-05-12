.DEFAULT_GOAL := help

BUILD_DIR := build
SRC_DIRS := src include tests
CXX_FILES := $(shell find $(SRC_DIRS) -type f \( -name '*.cpp' -o -name '*.hpp' \))

# Variables consumed by the run-* targets.
PORT           ?= 8554
URL            ?= rtsp://127.0.0.1:8554/test
GST_LEVEL      ?= 2
VALGRIND       ?=
INSTALL_PREFIX ?= /usr/local
GLIB_SUPP      := /usr/share/glib-2.0/valgrind/glib.supp
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=definite \
                  --track-origins=yes --error-exitcode=1 \
                  $(if $(wildcard $(GLIB_SUPP)),--suppressions=$(GLIB_SUPP))

DOCS_DIR          := docs
UML_OUTPUT_FORMAT ?= svg
PUPPETEER_CONFIG  := .puppeteer.json

.PHONY: help deps format build test clean install run-test-pattern-server run-test-pattern-client docs-uml

help: ## Show this help
	@awk 'BEGIN {FS = ":.*?## "; printf "Usage: make <target>\n\nTargets:\n"} \
		/^[a-zA-Z_-]+:.*?## / {printf "  %-25s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

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
		libgstrtspserver-1.0-dev \
		gstreamer1.0-tools \
		gstreamer1.0-plugins-base \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad \
		gstreamer1.0-plugins-ugly \
		gstreamer1.0-libav \
		nodejs \
		npm

format: ## Format C++ sources in-place with clang-format
	@clang-format -i $(CXX_FILES)

build: ## Configure and build the library (and tests when BUILD_TESTING=ON)
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j

test: build ## Build then run unit tests via CTest
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean: ## Remove the build directory
	rm -rf $(BUILD_DIR)

install: build ## Install library + headers (var: INSTALL_PREFIX=<dir>, default /usr/local)
	cmake --install $(BUILD_DIR) --prefix $(INSTALL_PREFIX)

run-test-pattern-server: build ## Run test_pattern_server (vars: PORT, GST_LEVEL, VALGRIND=1)
	GST_DEBUG=$(GST_LEVEL) $(if $(VALGRIND),valgrind $(VALGRIND_FLAGS) )./$(BUILD_DIR)/examples/test_pattern_server $(PORT)

run-test-pattern-client: build ## Run test_pattern_client (vars: URL, GST_LEVEL, VALGRIND=1)
	GST_DEBUG=$(GST_LEVEL) $(if $(VALGRIND),valgrind $(VALGRIND_FLAGS) )./$(BUILD_DIR)/examples/test_pattern_client $(URL)

docs-uml: ## Render every docs/*_uml.md mermaid diagram (needs npx; var: UML_OUTPUT_FORMAT=svg|png)
	@for src in $(DOCS_DIR)/*_uml.md; do \
		stem=$$(basename $$src .md); \
		echo ">>> Rendering $$src -> $(DOCS_DIR)/$$stem.$(UML_OUTPUT_FORMAT)"; \
		awk '/^```mermaid$$/{f=1; next} /^```$$/{f=0} f' $$src \
			> $(DOCS_DIR)/$$stem.mmd.tmp ; \
		npx --yes -p @mermaid-js/mermaid-cli mmdc \
			$(if $(wildcard $(PUPPETEER_CONFIG)),-p $(PUPPETEER_CONFIG)) \
			-i $(DOCS_DIR)/$$stem.mmd.tmp \
			-o $(DOCS_DIR)/$$stem.$(UML_OUTPUT_FORMAT) ; \
		rm -f $(DOCS_DIR)/$$stem.mmd.tmp ; \
	done
