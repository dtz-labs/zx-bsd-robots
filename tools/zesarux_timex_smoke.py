#!/usr/bin/env python3
"""Exercise the Timex 512x192 TAP through ZEsarUX's remote protocol."""

from __future__ import annotations

import argparse
import os
import re
import socket
import subprocess
import time
from collections.abc import Callable
from pathlib import Path


DEFAULT_ZESARUX = Path("/Applications/ZEsarUX.app/Contents/MacOS/zesarux")

MACHINE = "TC2048"
DISPLAY_FILE_0 = 0x4000
DISPLAY_FILE_1 = 0x6000
DISPLAY_FILE_BYTES = 6144
SCLD_PORT = 0xFF
SCLD_HIRES_WHITE_ON_BLACK = 0x3E
SCREEN_COLUMNS = 64
SCREEN_ROWS = 24
SCREEN_WIDTH = 512
SCREEN_HEIGHT = 192
FONT_SYMBOL = "_robots_font8x8"
FONT_FIRST_CHARACTER = 32
FONT_GLYPHS = 96
FONT_SCANLINES = 8
FONT_BYTES = FONT_GLYPHS * FONT_SCANLINES

TITLE_FRAGMENT = "ROBOTS"
TITLE_TIMEX_FRAGMENT = "TIMEX HI-RES 512X192"
TITLE_LICENSE_FRAGMENT = "L: BSD LICENSE"
TITLE_THEME_ORIGINAL_FRAGMENT = "G: THEME [ORIGINAL +]"
TITLE_THEME_ROBOT_FRAGMENT = "G: THEME [PIXEL ROBOT]"
TITLE_THEME_ATARI_FRAGMENT = "G: THEME [ATARI]"
TITLE_THEME_C64_FRAGMENT = "G: THEME [C64]"
TITLE_ORIGINAL_KEYS_FRAGMENT = "Y K U"
TITLE_NUMERIC_KEYS_FRAGMENT = "7 8 9"
TITLE_TELEPORT_FRAGMENT = "T TELEPORT"
LICENSE_PAGE_1_FRAGMENT = "BSD LICENSE 1/2"
LICENSE_PAGE_2_FRAGMENT = "BSD LICENSE 2/2"
BOARD_HEADER_FRAGMENT = "ROBOTS"
HELP_FRAGMENT = "HOW TO PLAY"
QUIT_FRAGMENT = "QUIT"

REMOTE_TIMEOUT_SECONDS = 2.0
STARTUP_TIMEOUT_SECONDS = 30.0
SCREEN_TIMEOUT_SECONDS = 12.0
KEY_HOLD_SECONDS = 0.25
KEY_RELEASE_SECONDS = 0.1
SCREEN_STABLE_SECONDS = 0.75
UNKNOWN_GLYPH = "?"

SYMBOL_RE = re.compile(r"^\s*(\S+)\s*=\s*\$([0-9A-Fa-f]+)\b")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Smoke-test ZX BSD Robots Timex 512x192 in headless ZEsarUX."
    )
    parser.add_argument("tap", type=Path, help="Timex TAP image to load")
    parser.add_argument(
        "--map",
        dest="map_path",
        required=True,
        type=Path,
        help="z88dk link map containing _robots_font8x8",
    )
    parser.add_argument(
        "--screenshot",
        type=Path,
        help="optional destination for a final 512x192 monochrome PPM screenshot",
    )
    return parser.parse_args()


def parse_symbol(map_path: Path, symbol: str) -> int:
    for line in map_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = SYMBOL_RE.match(line)
        if match is not None and match.group(1) == symbol:
            return int(match.group(2), 16)
    raise ValueError(f"map does not define required symbol {symbol}")


def reserve_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def receive(connection: socket.socket) -> str:
    connection.settimeout(REMOTE_TIMEOUT_SECONDS)
    data = bytearray()
    try:
        while not data.endswith(b"command> "):
            chunk = connection.recv(65536)
            if not chunk:
                break
            data.extend(chunk)
    except socket.timeout:
        pass
    return data.decode("latin-1", "replace")


def command(connection: socket.socket, text: str) -> str:
    connection.sendall((text + "\n").encode("ascii"))
    return receive(connection)


def connect(port: int) -> socket.socket:
    deadline = time.monotonic() + STARTUP_TIMEOUT_SECONDS
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        try:
            connection = socket.create_connection(
                ("127.0.0.1", port), timeout=1.0
            )
            receive(connection)
            return connection
        except OSError as error:
            last_error = error
            time.sleep(0.1)
    raise RuntimeError(f"ZEsarUX remote protocol did not start: {last_error}")


def read_memory(connection: socket.socket, address: int, length: int) -> bytes:
    response = command(connection, f"read-memory {address} {length}")
    for line in response.splitlines():
        candidate = line.rsplit("command> ", 1)[-1].strip()
        if (
            len(candidate) == length * 2
            and re.fullmatch(r"[0-9A-Fa-f]+", candidate) is not None
        ):
            return bytes.fromhex(candidate)
    raise RuntimeError(
        f"cannot parse read-memory response for {address}:{length}: {response!r}"
    )


def read_port(connection: socket.socket, port: int) -> int:
    response = command(connection, f"evaluate IN({port})")
    for line in response.splitlines():
        candidate = line.rsplit("command> ", 1)[-1].strip()
        if re.fullmatch(r"[0-9]+", candidate) is not None:
            return int(candidate, 10)
    raise RuntimeError(f"cannot parse port {port} response: {response!r}")


def send_ascii(connection: socket.socket, text: str) -> None:
    for character in text:
        key_code = ord(character)
        command(connection, f"send-keys-event {key_code} 1")
        time.sleep(KEY_HOLD_SECONDS)
        command(connection, f"send-keys-event {key_code} 0")
        time.sleep(KEY_RELEASE_SECONDS)


def bitmap_offset(x_byte: int, y: int) -> int:
    return ((y & 0xC0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | x_byte


def font_lookup(font: bytes) -> dict[tuple[int, ...], str]:
    if len(font) != FONT_BYTES:
        raise ValueError(f"expected {FONT_BYTES} font bytes, got {len(font)}")

    lookup: dict[tuple[int, ...], str] = {}
    for glyph_index in range(FONT_GLYPHS):
        start = glyph_index * FONT_SCANLINES
        glyph = tuple(font[start : start + FONT_SCANLINES])
        # Keep the first code for duplicate glyphs. This makes an all-zero
        # cell decode as SPACE rather than a later blank placeholder.
        lookup.setdefault(glyph, chr(FONT_FIRST_CHARACTER + glyph_index))
    return lookup


def decode_screen(
    display_0: bytes, display_1: bytes, font: bytes
) -> tuple[str, ...]:
    if len(display_0) != DISPLAY_FILE_BYTES:
        raise ValueError(
            f"expected {DISPLAY_FILE_BYTES} bytes in display file 0, "
            f"got {len(display_0)}"
        )
    if len(display_1) != DISPLAY_FILE_BYTES:
        raise ValueError(
            f"expected {DISPLAY_FILE_BYTES} bytes in display file 1, "
            f"got {len(display_1)}"
        )

    lookup = font_lookup(font)
    rows: list[str] = []
    for text_row in range(SCREEN_ROWS):
        characters: list[str] = []
        for column in range(SCREEN_COLUMNS):
            display = display_0 if column % 2 == 0 else display_1
            byte_column = column // 2
            glyph = tuple(
                display[
                    bitmap_offset(
                        byte_column, text_row * FONT_SCANLINES + scanline
                    )
                ]
                for scanline in range(FONT_SCANLINES)
            )
            characters.append(lookup.get(glyph, UNKNOWN_GLYPH))
        rows.append("".join(characters).rstrip())
    return tuple(rows)


def read_screen(
    connection: socket.socket, font_address: int
) -> tuple[tuple[str, ...], bytes, bytes]:
    font = read_memory(connection, font_address, FONT_BYTES)
    display_0 = read_memory(connection, DISPLAY_FILE_0, DISPLAY_FILE_BYTES)
    display_1 = read_memory(connection, DISPLAY_FILE_1, DISPLAY_FILE_BYTES)
    return decode_screen(display_0, display_1, font), display_0, display_1


def visible_text(rows: tuple[str, ...]) -> str:
    return "\n".join(rows)


def has_fragment(rows: tuple[str, ...], fragment: str) -> bool:
    return fragment in visible_text(rows).upper()


def has_board_frame(rows: tuple[str, ...]) -> bool:
    if len(rows) != SCREEN_ROWS:
        return False

    frame_right = 60
    top = rows[0]
    bottom = rows[-1]
    has_horizontal_edges = (
        len(top) > frame_right
        and top[0] == "{"
        and top[frame_right] == "}"
        and len(bottom) > frame_right
        and bottom[0] == "["
        and bottom[frame_right] == "]"
        and bottom[1:frame_right].count("-") >= 55
    )
    vertical_rows = sum(
        len(row) > frame_right and row[0] == "|" and row[frame_right] == "|"
        for row in rows[1:-1]
    )
    return has_horizontal_edges and vertical_rows >= 20


def is_board(rows: tuple[str, ...]) -> bool:
    text = visible_text(rows)
    return (
        has_fragment(rows, BOARD_HEADER_FRAGMENT)
        and has_board_frame(rows)
        and "@" in text
    )


def assert_two_file_picture(
    display_0: bytes, display_1: bytes, description: str
) -> None:
    if not any(display_0):
        raise AssertionError(f"{description}: display file 0 is blank")
    if not any(display_1):
        raise AssertionError(f"{description}: display file 1 is blank")


def format_screen(rows: tuple[str, ...]) -> str:
    return "\n".join(f"{index:02}: {row}" for index, row in enumerate(rows))


def wait_for_screen(
    connection: socket.socket,
    font_address: int,
    description: str,
    predicate: Callable[[tuple[str, ...]], bool],
    *,
    timeout: float = SCREEN_TIMEOUT_SECONDS,
    stable_for: float = 0.0,
) -> tuple[tuple[str, ...], bytes, bytes]:
    deadline = time.monotonic() + timeout
    last_rows: tuple[str, ...] = ()
    last_error: Exception | None = None
    stable_rows: tuple[str, ...] | None = None
    stable_since = 0.0

    while time.monotonic() < deadline:
        try:
            rows, display_0, display_1 = read_screen(connection, font_address)
            last_rows = rows
            if predicate(rows):
                now = time.monotonic()
                if stable_for <= 0.0:
                    return rows, display_0, display_1
                if rows != stable_rows:
                    stable_rows = rows
                    stable_since = now
                elif now - stable_since >= stable_for:
                    return rows, display_0, display_1
            else:
                stable_rows = None
        except (RuntimeError, ValueError) as error:
            last_error = error
        time.sleep(0.1)

    detail = f"\n{format_screen(last_rows)}" if last_rows else ""
    if last_error is not None:
        detail += f"\nlast read error: {last_error}"
    raise AssertionError(f"screen never reached {description}{detail}")


def wait_for_port(
    connection: socket.socket, port: int, expected: int, description: str
) -> None:
    deadline = time.monotonic() + SCREEN_TIMEOUT_SECONDS
    last_value: int | None = None
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            last_value = read_port(connection, port)
            if last_value == expected:
                return
        except RuntimeError as error:
            last_error = error
        time.sleep(0.1)
    detail = f"; last value ${last_value:02X}" if last_value is not None else ""
    if last_error is not None:
        detail += f"; last error: {last_error}"
    raise AssertionError(f"port never reached {description}{detail}")


def save_ppm(destination: Path, display_0: bytes, display_1: bytes) -> None:
    pixels = bytearray()
    for y in range(SCREEN_HEIGHT):
        for x in range(SCREEN_WIDTH):
            byte_index = x // 8
            display = display_0 if byte_index % 2 == 0 else display_1
            value = display[bitmap_offset(byte_index // 2, y)]
            level = 255 if value & (0x80 >> (x & 7)) else 0
            pixels.extend((level, level, level))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(b"P6\n512 192\n255\n" + pixels)


def main() -> int:
    args = parse_args()
    tap = args.tap.resolve()
    map_path = args.map_path.resolve()
    emulator = Path(os.environ.get("ZESARUX", str(DEFAULT_ZESARUX))).resolve()

    if not tap.is_file():
        raise SystemExit(f"TAP not found: {tap}")
    if not map_path.is_file():
        raise SystemExit(f"map not found: {map_path}")
    if not emulator.is_file():
        raise SystemExit(f"ZEsarUX not found: {emulator}")

    try:
        font_address = parse_symbol(map_path, FONT_SYMBOL)
    except ValueError as error:
        raise SystemExit(f"invalid map: {error}") from error
    if not 0x4000 <= font_address <= 0x10000 - FONT_BYTES:
        raise SystemExit(
            f"invalid map: {FONT_SYMBOL} address ${font_address:04X} "
            "cannot hold the complete font in RAM"
        )

    port = reserve_port()
    process = subprocess.Popen(
        [
            str(emulator),
            "--noconfigfile",
            "--machine",
            MACHINE,
            "--tape",
            str(tap),
            "--vo",
            "null",
            "--ao",
            "null",
            "--nosplash",
            "--nowelcomemessage",
            "--enable-remoteprotocol",
            "--remoteprotocol-port",
            str(port),
            "--quickexit",
            "--fastautoload",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )

    connection: socket.socket | None = None
    try:
        connection = connect(port)

        title_rows, title_0, title_1 = wait_for_screen(
            connection,
            font_address,
            f"boot/title containing {TITLE_TIMEX_FRAGMENT!r}",
            lambda rows: (
                has_fragment(rows, TITLE_FRAGMENT)
                and has_fragment(rows, TITLE_TIMEX_FRAGMENT)
                and has_fragment(rows, TITLE_LICENSE_FRAGMENT)
                and has_fragment(rows, TITLE_ORIGINAL_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_NUMERIC_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_TELEPORT_FRAGMENT)
                and has_fragment(rows, TITLE_THEME_ORIGINAL_FRAGMENT)
                and not has_fragment(rows, "JOYSTICK")
                and not has_fragment(rows, "KEMPSTON")
                and not has_fragment(rows, "MATRIX")
                and not has_fragment(rows, "T/0")
            ),
            timeout=STARTUP_TIMEOUT_SECONDS,
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_two_file_picture(title_0, title_1, "title screen")
        scld_mode = read_port(connection, SCLD_PORT)
        if scld_mode != SCLD_HIRES_WHITE_ON_BLACK:
            raise AssertionError(
                "title screen did not select Timex 512x192 white-on-black "
                f"mode: port $FF is ${scld_mode:02X}, expected $3E"
            )

        for expected_theme in (
            TITLE_THEME_ROBOT_FRAGMENT,
            TITLE_THEME_ATARI_FRAGMENT,
            TITLE_THEME_C64_FRAGMENT,
            TITLE_THEME_ORIGINAL_FRAGMENT,
        ):
            send_ascii(connection, "g")
            wait_for_screen(
                connection,
                font_address,
                f"Timex title theme {expected_theme!r}",
                lambda rows, fragment=expected_theme: has_fragment(rows, fragment),
                stable_for=SCREEN_STABLE_SECONDS,
            )

        send_ascii(connection, "l")
        wait_for_screen(
            connection,
            font_address,
            f"license screen containing {LICENSE_PAGE_1_FRAGMENT!r}",
            lambda rows: has_fragment(rows, LICENSE_PAGE_1_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, " ")
        wait_for_screen(
            connection,
            font_address,
            f"license screen containing {LICENSE_PAGE_2_FRAGMENT!r}",
            lambda rows: has_fragment(rows, LICENSE_PAGE_2_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, "p")
        wait_for_screen(
            connection,
            font_address,
            "BSD license page 1 after going back",
            lambda rows: has_fragment(rows, LICENSE_PAGE_1_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, "q")
        returned_title_rows, _, _ = wait_for_screen(
            connection,
            font_address,
            "title after closing the BSD license",
            lambda rows: (
                has_fragment(rows, TITLE_TIMEX_FRAGMENT)
                and has_fragment(rows, TITLE_LICENSE_FRAGMENT)
                and has_fragment(rows, TITLE_ORIGINAL_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_NUMERIC_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_TELEPORT_FRAGMENT)
                and has_fragment(rows, TITLE_THEME_ORIGINAL_FRAGMENT)
                and not has_fragment(rows, "JOYSTICK")
                and not has_fragment(rows, "KEMPSTON")
                and not has_fragment(rows, "MATRIX")
                and not has_fragment(rows, "T/0")
            ),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        send_ascii(connection, " ")
        board_rows, board_0, board_1 = wait_for_screen(
            connection,
            font_address,
            "game board with header, proper frame corners, and player",
            is_board,
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_two_file_picture(board_0, board_1, "game board")

        send_ascii(connection, "i")
        wait_for_screen(
            connection,
            font_address,
            f"help screen containing {HELP_FRAGMENT!r}",
            lambda rows: (
                has_fragment(rows, HELP_FRAGMENT)
                and has_fragment(rows, "T                TELEPORT")
                and not has_fragment(rows, "T OR 0")
                and not has_fragment(rows, "JOYSTICK")
            ),
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, " ")
        wait_for_screen(
            connection,
            font_address,
            "game board after closing help",
            is_board,
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, "q")
        quit_rows, _, _ = wait_for_screen(
            connection,
            font_address,
            f"changed quit modal containing {QUIT_FRAGMENT!r}",
            lambda rows: rows != board_rows and has_fragment(rows, QUIT_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )

        send_ascii(connection, "n")
        final_rows, final_0, final_1 = wait_for_screen(
            connection,
            font_address,
            "game board after declining quit",
            lambda rows: rows != quit_rows and is_board(rows),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_two_file_picture(final_0, final_1, "game board after declining quit")

        if args.screenshot is not None:
            save_ppm(args.screenshot.resolve(), final_0, final_1)

        send_ascii(connection, "q")
        wait_for_screen(
            connection,
            font_address,
            "final quit confirmation",
            lambda rows: rows != final_rows and has_fragment(rows, QUIT_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        send_ascii(connection, "y")
        wait_for_port(
            connection,
            SCLD_PORT,
            0,
            "ULA mode $00 after returning to BASIC",
        )

        print(
            "ZEsarUX Timex smoke OK: SCLD $3E, 512x192 two-file display, "
            "keyboard-only control copy, four theme labels with wraparound, "
            "two-page BSD license, game board, frame corners, help, quit "
            "cancellation, return to play, and clean ULA-mode exit"
        )
        if args.screenshot is not None:
            print(f"screenshot: {args.screenshot.resolve()}")
    finally:
        if connection is not None:
            connection.close()
        process.terminate()
        try:
            process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3.0)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
