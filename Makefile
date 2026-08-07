ROOT := $(CURDIR)
BUILD_DIR := build

.DEFAULT_GOAL := help

Z88DK_HOME ?= /Volumes/SSD/Programowanie/z88dk
Z88DK_ROOT ?= $(Z88DK_HOME)
ZCC ?= $(Z88DK_ROOT)/bin/zcc
ZCCCFG ?= $(Z88DK_ROOT)/lib/config
ZESARUX ?= /Applications/ZEsarUX.app/Contents/MacOS/zesarux
PYTHON ?= python3
HOST_CC ?= cc

SPECTRUM_TARGET := $(BUILD_DIR)/zx-bsd-robots-48k
SPECTRUM_TAP := $(SPECTRUM_TARGET).tap
SPECTRUM_MAP := $(SPECTRUM_TARGET).map
TIMEX_TARGET := $(BUILD_DIR)/zx-bsd-robots-timex-512
TIMEX_TAP := $(TIMEX_TARGET).tap
TIMEX_MAP := $(TIMEX_TARGET).map

SPECTRUM_SCREENSHOT ?= $(BUILD_DIR)/zx-bsd-robots-48k-smoke.ppm
TIMEX_SCREENSHOT ?= $(BUILD_DIR)/zx-bsd-robots-timex-512-smoke.ppm

COMMON_SOURCES := src/main.c src/game.c src/controls.c
SPECTRUM_SOURCES := $(COMMON_SOURCES) src/font4x8.c
TIMEX_SOURCES := $(COMMON_SOURCES) src/font8x8.c src/timex_themes.c
COMMON_HEADERS := include/robots_game.h include/robots_controls.h
SPECTRUM_HEADERS := $(COMMON_HEADERS) include/font4x8.h
TIMEX_HEADERS := $(COMMON_HEADERS) include/font8x8.h include/timex_themes.h

TEST_GAME_BIN := $(BUILD_DIR)/test-game
TEST_CONTROLS_BIN := $(BUILD_DIR)/test-controls
TEST_FONT4_BIN := $(BUILD_DIR)/test-font4x8
TEST_FONT8_BIN := $(BUILD_DIR)/test-font8x8
TEST_TIMEX_THEMES_BIN := $(BUILD_DIR)/test-timex-themes

ZCC_FLAGS ?= +zx -vn -SO3 -clib=sdcc_iy -startup=31
ZCC_FLAGS += -I$(ROOT)/include \
	-pragma-define:REGISTER_SP=65535 \
	-pragma-define:CRT_STACK_SIZE=2048

.PHONY: help all spectrum timex test test-host \
	check-z88dk check-layout check-layout-spectrum check-layout-timex \
	check-tap check-tap-spectrum check-tap-timex verify \
	smoke-spectrum smoke-timex run run-spectrum run-timex clean

help:
	@printf '%s\n' \
		'ZX BSD Robots targets:' \
		'' \
		'  make spectrum        Build the ZX Spectrum 48K TAP' \
		'  make timex           Build the Timex 512x192 TAP' \
		'  make all             Build both TAP files in build/' \
		'  make test            Run host, TAP, and memory-layout checks' \
		'  make smoke-spectrum  Exercise the 48K UI in headless ZEsarUX' \
		'  make smoke-timex     Exercise the Timex hi-res UI in ZEsarUX' \
		'  make run             Launch the Spectrum 48K edition' \
		'  make run-timex       Launch the Timex TC2048 edition' \
		'  make verify          Run every automated check and both smokes' \
		'  make clean           Remove generated build files' \
		'' \
		'Overrides: Z88DK_HOME=/path/to/z88dk ZESARUX=/path/to/zesarux'

all: spectrum timex

$(BUILD_DIR):
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

$(SPECTRUM_TAP): $(SPECTRUM_SOURCES) $(SPECTRUM_HEADERS) | $(BUILD_DIR)
	$(MAKE) --no-print-directory check-z88dk
	env PATH="$(Z88DK_ROOT)/bin:$(PATH)" ZCCCFG="$(ZCCCFG)" \
		"$(ZCC)" $(ZCC_FLAGS) $(SPECTRUM_SOURCES) \
		-o "$(SPECTRUM_TARGET)" -create-app -m
	@test -f "$@" || { echo "z88dk did not create $@" >&2; exit 1; }

$(TIMEX_TAP): $(TIMEX_SOURCES) $(TIMEX_HEADERS) | $(BUILD_DIR)
	$(MAKE) --no-print-directory check-z88dk
	env PATH="$(Z88DK_ROOT)/bin:$(PATH)" ZCCCFG="$(ZCCCFG)" \
		"$(ZCC)" $(ZCC_FLAGS) -DROBOTS_TIMEX_HIRES=1 $(TIMEX_SOURCES) \
		-o "$(TIMEX_TARGET)" -create-app -m
	@test -f "$@" || { echo "z88dk did not create $@" >&2; exit 1; }

spectrum: $(SPECTRUM_TAP)

timex: $(TIMEX_TAP)

$(TEST_GAME_BIN): tests/test_game.c src/game.c include/robots_game.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_game.c src/game.c -o "$@"

$(TEST_CONTROLS_BIN): tests/test_controls.c src/controls.c \
		include/robots_controls.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_controls.c src/controls.c -o "$@"

$(TEST_FONT4_BIN): tests/test_font.c src/font4x8.c include/font4x8.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_font.c src/font4x8.c -o "$@"

$(TEST_FONT8_BIN): tests/test_font8x8.c src/font8x8.c include/font8x8.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_font8x8.c src/font8x8.c -o "$@"

$(TEST_TIMEX_THEMES_BIN): tests/test_timex_themes.c src/timex_themes.c \
		src/font8x8.c include/timex_themes.h include/font8x8.h | $(BUILD_DIR)
	$(HOST_CC) -std=c89 -Wall -Wextra -Werror -pedantic \
		-Iinclude tests/test_timex_themes.c src/timex_themes.c \
		src/font8x8.c -o "$@"

test-host: $(TEST_GAME_BIN) $(TEST_CONTROLS_BIN) $(TEST_FONT4_BIN) \
		$(TEST_FONT8_BIN) \
		$(TEST_TIMEX_THEMES_BIN)
	"$(TEST_GAME_BIN)"
	"$(TEST_CONTROLS_BIN)"
	"$(TEST_FONT4_BIN)"
	"$(TEST_FONT8_BIN)"
	"$(TEST_TIMEX_THEMES_BIN)"

check-layout-spectrum: $(SPECTRUM_TAP)
	$(PYTHON) tools/check_48k_layout.py "$(SPECTRUM_MAP)"

check-layout-timex: $(TIMEX_TAP)
	$(PYTHON) tools/check_48k_layout.py "$(TIMEX_MAP)"

check-layout: check-layout-spectrum check-layout-timex

check-tap-spectrum: $(SPECTRUM_TAP)
	$(PYTHON) tools/check_tap.py "$(SPECTRUM_TAP)"

check-tap-timex: $(TIMEX_TAP)
	$(PYTHON) tools/check_tap.py "$(TIMEX_TAP)"

check-tap: check-tap-spectrum check-tap-timex

test: test-host check-layout check-tap

smoke-spectrum: $(SPECTRUM_TAP)
	ZESARUX="$(ZESARUX)" $(PYTHON) tools/zesarux_smoke.py \
		"$(SPECTRUM_TAP)" --map "$(SPECTRUM_MAP)" \
		--screenshot "$(SPECTRUM_SCREENSHOT)"

smoke-timex: $(TIMEX_TAP)
	ZESARUX="$(ZESARUX)" $(PYTHON) tools/zesarux_timex_smoke.py \
		"$(TIMEX_TAP)" --map "$(TIMEX_MAP)" \
		--screenshot "$(TIMEX_SCREENSHOT)"

verify: test smoke-spectrum smoke-timex

run: run-spectrum

run-spectrum: $(SPECTRUM_TAP)
	@test -x "$(ZESARUX)" || { \
		echo "ZEsarUX not found at $(ZESARUX)" >&2; \
		exit 1; \
	}
	"$(ZESARUX)" --noconfigfile --machine 48k \
		--tape "$(abspath $(SPECTRUM_TAP))" --fastautoload

run-timex: $(TIMEX_TAP)
	@test -x "$(ZESARUX)" || { \
		echo "ZEsarUX not found at $(ZESARUX)" >&2; \
		exit 1; \
	}
	"$(ZESARUX)" --noconfigfile --machine TC2048 \
		--tape "$(abspath $(TIMEX_TAP))" --fastautoload

clean:
	rm -rf "$(BUILD_DIR)"
