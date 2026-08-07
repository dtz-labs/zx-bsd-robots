ROOT := $(CURDIR)
BUILD_DIR := build
DIST_DIR := dist

.DEFAULT_GOAL := help

Z88DK_HOME ?= /Volumes/SSD/Programowanie/z88dk
Z88DK_ROOT ?= $(Z88DK_HOME)
ZCC ?= $(Z88DK_ROOT)/bin/zcc
ZCCCFG ?= $(Z88DK_ROOT)/lib/config
ZESARUX ?= /Applications/ZEsarUX.app/Contents/MacOS/zesarux
PYTHON ?= python3
HOST_CC ?= cc

TARGET := $(BUILD_DIR)/robots-zx
TAP := $(TARGET).tap
MAP := $(TARGET).map
DIST_TAP := $(DIST_DIR)/robots-zx-48k.tap
DIST_LICENSE := $(DIST_DIR)/LICENSE.txt
SCREENSHOT ?= $(BUILD_DIR)/robots-zx-smoke.ppm

SOURCES := src/main.c src/game.c src/font4x8.c
HEADERS := include/robots_game.h include/font4x8.h
TEST_GAME_BIN := $(BUILD_DIR)/test-game
TEST_FONT_BIN := $(BUILD_DIR)/test-font

ZCC_FLAGS ?= +zx -vn -SO3 -clib=sdcc_iy -startup=5
ZCC_FLAGS += -I$(ROOT)/include \
	-pragma-define:REGISTER_SP=65535 \
	-pragma-define:CRT_STACK_SIZE=2048 \
	-pragma-redirect:CRT_OTERM_FONT_4X8=_robots_font4x8

.PHONY: help all spectrum spectrum-dist test test-host check-z88dk \
	check-layout check-tap verify smoke-spectrum run-spectrum clean

help:
	@printf '%s\n' \
		'Robots ZX targets:' \
		'' \
		'  make spectrum        Build the ZX Spectrum 48K TAP' \
		'  make spectrum-dist   Copy the release TAP to dist/' \
		'  make test            Run host logic tests and TAP/layout checks' \
		'  make smoke-spectrum  Exercise title/help/gameplay in headless ZEsarUX' \
		'  make run-spectrum    Launch the 48K TAP in ZEsarUX' \
		'  make verify          Run every automated check, including emulator smoke' \
		'  make clean           Remove generated build files' \
		'' \
		'Overrides: Z88DK_HOME=/path/to/z88dk ZESARUX=/path/to/zesarux'

all: spectrum-dist

$(BUILD_DIR):
	mkdir -p "$@"

$(DIST_DIR):
	mkdir -p "$@"

check-z88dk:
	@test -x "$(ZCC)" || { \
		echo "zcc not found at $(ZCC)" >&2; \
		echo "Set Z88DK_HOME, Z88DK_ROOT, ZCC, and ZCCCFG." >&2; \
		exit 1; \
	}
	@test -d "$(ZCCCFG)" || { \
		echo "z88dk config directory not found at $(ZCCCFG)" >&2; \
		exit 1; \
	}

$(TAP): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(MAKE) --no-print-directory check-z88dk
	env PATH="$(Z88DK_ROOT)/bin:$(PATH)" ZCCCFG="$(ZCCCFG)" \
		"$(ZCC)" $(ZCC_FLAGS) $(SOURCES) \
		-o "$(TARGET)" -create-app -m
	@test -f "$@" || { echo "z88dk did not create $@" >&2; exit 1; }

$(DIST_TAP): $(TAP) | $(DIST_DIR)
	cp "$(TAP)" "$@"

$(DIST_LICENSE): LICENSE | $(DIST_DIR)
	cp "LICENSE" "$@"

spectrum: $(TAP)

spectrum-dist: $(DIST_TAP) $(DIST_LICENSE)

$(TEST_GAME_BIN): tests/test_game.c src/game.c include/robots_game.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_game.c src/game.c -o "$@"

$(TEST_FONT_BIN): tests/test_font.c src/font4x8.c include/font4x8.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_font.c src/font4x8.c -o "$@"

test-host: $(TEST_GAME_BIN) $(TEST_FONT_BIN)
	"$(TEST_GAME_BIN)"
	"$(TEST_FONT_BIN)"

check-layout: $(TAP)
	$(PYTHON) tools/check_48k_layout.py "$(MAP)"

check-tap: $(DIST_TAP)
	$(PYTHON) tools/check_tap.py "$(DIST_TAP)"

test: test-host check-layout check-tap

smoke-spectrum: $(DIST_TAP)
	ZESARUX="$(ZESARUX)" $(PYTHON) tools/zesarux_smoke.py \
		"$(DIST_TAP)" --map "$(MAP)" --screenshot "$(SCREENSHOT)"

verify: test smoke-spectrum

run-spectrum: $(DIST_TAP)
	@test -x "$(ZESARUX)" || { \
		echo "ZEsarUX not found at $(ZESARUX)" >&2; \
		exit 1; \
	}
	"$(ZESARUX)" --noconfigfile --machine 48k \
		--tape "$(abspath $(DIST_TAP))" --fastautoload

clean:
	rm -rf "$(BUILD_DIR)"
