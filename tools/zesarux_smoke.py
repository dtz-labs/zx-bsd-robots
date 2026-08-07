#!/usr/bin/env python3
"""Exercise the 48K TAP through ZEsarUX's remote control protocol."""

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

MACHINE = "48k"
SCREEN_BITMAP = 16384
SCREEN_ATTRIBUTES = 22528
SCREEN_BITMAP_BYTES = 6144
SCREEN_ATTRIBUTE_BYTES = 768
SCREEN_COLUMNS = 64
SCREEN_ROWS = 24
FONT_SYMBOL = "_robots_font4x8"
FONT_FIRST_CHARACTER = 32
FONT_GLYPHS = 96
FONT_SCANLINES = 8
FONT_BYTES = FONT_GLYPHS * FONT_SCANLINES

# Keep UI wording in one place: these are deliberately cheap to update when
# the native screens change their copy.
TITLE_FRAGMENT = "ROBOTS"
TITLE_OBJECT_FRAGMENT = "ENEMY"
TITLE_PLAYER_HEAP_FRAGMENT = "YOU @   HEAP *"
TITLE_GLYPH_SELECTOR_FRAGMENT = "G: THEME"
TITLE_KEYBOARD_FRAGMENT = "KEYBOARD CONTROLS ONLY"
TITLE_ORIGINAL_KEYS_FRAGMENT = "Y K U"
TITLE_NUMERIC_KEYS_FRAGMENT = "7 8 9"
TITLE_TELEPORT_FRAGMENT = "T TELEPORT"
TITLE_LICENSE_FRAGMENT = "L: BSD LICENSE"
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
        description="Smoke-test the ZX BSD Robots 48K TAP in headless ZEsarUX."
    )
    parser.add_argument("tap", type=Path, help="48K TAP image to load")
    parser.add_argument(
        "--map",
        dest="map_path",
        required=True,
        type=Path,
        help="z88dk link map containing _robots_font4x8",
    )
    parser.add_argument(
        "--screenshot",
        type=Path,
        help="optional destination for a final 256x192 PPM screenshot",
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
        glyph_bytes = font[start : start + FONT_SCANLINES]
        if any((value >> 4) != (value & 0x0F) for value in glyph_bytes):
            code = FONT_FIRST_CHARACTER + glyph_index
            raise ValueError(
                f"font glyph {code} has a row whose nibbles are not duplicated"
            )
        glyph = tuple(value >> 4 for value in glyph_bytes)
        # Keep the first code for duplicate glyphs. In particular, this makes
        # an all-zero cell decode as SPACE rather than a later duplicate.
        lookup.setdefault(glyph, chr(FONT_FIRST_CHARACTER + glyph_index))
    return lookup


def decode_screen(bitmap: bytes, font: bytes) -> tuple[str, ...]:
    if len(bitmap) != SCREEN_BITMAP_BYTES:
        raise ValueError(
            f"expected {SCREEN_BITMAP_BYTES} bitmap bytes, got {len(bitmap)}"
        )
    lookup = font_lookup(font)
    rows: list[str] = []

    for text_row in range(SCREEN_ROWS):
        characters: list[str] = []
        for column in range(SCREEN_COLUMNS):
            glyph_rows: list[int] = []
            for scanline in range(FONT_SCANLINES):
                value = bitmap[
                    bitmap_offset(column // 2, text_row * FONT_SCANLINES + scanline)
                ]
                glyph_rows.append(value >> 4 if column % 2 == 0 else value & 0x0F)
            characters.append(lookup.get(tuple(glyph_rows), UNKNOWN_GLYPH))
        rows.append("".join(characters).rstrip())
    return tuple(rows)


def read_screen(
    connection: socket.socket, font_address: int
) -> tuple[tuple[str, ...], bytes, bytes]:
    font = read_memory(connection, font_address, FONT_BYTES)
    bitmap = read_memory(connection, SCREEN_BITMAP, SCREEN_BITMAP_BYTES)
    attributes = read_memory(
        connection, SCREEN_ATTRIBUTES, SCREEN_ATTRIBUTE_BYTES
    )
    return decode_screen(bitmap, font), bitmap, attributes


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

    # draw_frame() writes the frame before the much slower full field redraw.
    # Requiring almost the complete frame avoids accepting an early, partially
    # rendered board while the game is not yet polling the keyboard.
    return has_horizontal_edges and vertical_rows >= 20


def is_board(rows: tuple[str, ...]) -> bool:
    text = visible_text(rows)
    has_player = "@" in text
    return (
        has_fragment(rows, BOARD_HEADER_FRAGMENT)
        and has_board_frame(rows)
        and has_player
    )


def assert_white_on_black(attributes: bytes, description: str) -> None:
    if len(attributes) != SCREEN_ATTRIBUTE_BYTES:
        raise AssertionError(
            f"{description}: expected {SCREEN_ATTRIBUTE_BYTES} attributes, "
            f"got {len(attributes)}"
        )

    for index, attribute in enumerate(attributes):
        if attribute != 0x47:
            row, column = divmod(index, 32)
            raise AssertionError(
                f"{description}: expected bright-white ink on black paper "
                f"at ({column}, {row}), got attribute 0x{attribute:02x}"
            )


def assert_board_colours(attributes: bytes, description: str) -> None:
    if len(attributes) != SCREEN_ATTRIBUTE_BYTES:
        raise AssertionError(
            f"{description}: expected {SCREEN_ATTRIBUTE_BYTES} attributes, "
            f"got {len(attributes)}"
        )

    red_player_cells = [
        index for index, attribute in enumerate(attributes) if attribute == 0x42
    ]
    if len(red_player_cells) != 1:
        raise AssertionError(
            f"{description}: expected exactly one bright-red player attribute, "
            f"got {len(red_player_cells)}"
        )

    for index, attribute in enumerate(attributes):
        if attribute not in (0x42, 0x47):
            row, column = divmod(index, 32)
            raise AssertionError(
                f"{description}: expected bright-red player or bright-white "
                f"ink on black paper at ({column}, {row}), "
                f"got attribute 0x{attribute:02x}"
            )


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
            rows, bitmap, attributes = read_screen(connection, font_address)
            last_rows = rows
            if predicate(rows):
                now = time.monotonic()
                if stable_for <= 0.0:
                    return rows, bitmap, attributes
                if rows != stable_rows:
                    stable_rows = rows
                    stable_since = now
                elif now - stable_since >= stable_for:
                    return rows, bitmap, attributes
            else:
                stable_rows = None
        except (RuntimeError, ValueError) as error:
            last_error = error
        time.sleep(0.1)

    detail = f"\n{format_screen(last_rows)}" if last_rows else ""
    if last_error is not None:
        detail += f"\nlast read error: {last_error}"
    raise AssertionError(f"screen never reached {description}{detail}")


def spectrum_colour(index: int, bright: bool) -> tuple[int, int, int]:
    level = 255 if bright else 205
    return (
        level if index & 0x02 else 0,
        level if index & 0x04 else 0,
        level if index & 0x01 else 0,
    )


def save_ppm(destination: Path, bitmap: bytes, attributes: bytes) -> None:
    pixels = bytearray()
    for y in range(192):
        for x in range(256):
            attribute = attributes[(y // 8) * 32 + x // 8]
            bright = bool(attribute & 0x40)
            ink = spectrum_colour(attribute & 0x07, bright)
            paper = spectrum_colour((attribute >> 3) & 0x07, bright)
            value = bitmap[bitmap_offset(x // 8, y)]
            pixels.extend(ink if value & (0x80 >> (x & 7)) else paper)

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(b"P6\n256 192\n255\n" + pixels)


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

        _, _, title_attributes = wait_for_screen(
            connection,
            font_address,
            "boot/title with the fixed object legend",
            lambda rows: (
                has_fragment(rows, TITLE_FRAGMENT)
                and has_fragment(rows, TITLE_OBJECT_FRAGMENT)
                and has_fragment(rows, TITLE_PLAYER_HEAP_FRAGMENT)
                and has_fragment(rows, TITLE_LICENSE_FRAGMENT)
                and has_fragment(rows, TITLE_KEYBOARD_FRAGMENT)
                and has_fragment(rows, TITLE_ORIGINAL_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_NUMERIC_KEYS_FRAGMENT)
                and has_fragment(rows, TITLE_TELEPORT_FRAGMENT)
                and not has_fragment(rows, "JOYSTICK")
                and not has_fragment(rows, "KEMPSTON")
                and not has_fragment(rows, "MATRIX")
                and not has_fragment(rows, "T/0")
                and not has_fragment(rows, TITLE_GLYPH_SELECTOR_FRAGMENT)
            ),
            timeout=STARTUP_TIMEOUT_SECONDS,
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(title_attributes, "title screen")

        send_ascii(connection, "l")
        _, _, license_1_attributes = wait_for_screen(
            connection,
            font_address,
            f"license screen containing {LICENSE_PAGE_1_FRAGMENT!r}",
            lambda rows: has_fragment(rows, LICENSE_PAGE_1_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(license_1_attributes, "BSD license page 1")

        send_ascii(connection, " ")
        _, _, license_2_attributes = wait_for_screen(
            connection,
            font_address,
            f"license screen containing {LICENSE_PAGE_2_FRAGMENT!r}",
            lambda rows: has_fragment(rows, LICENSE_PAGE_2_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(license_2_attributes, "BSD license page 2")

        send_ascii(connection, "p")
        _, _, license_1_again_attributes = wait_for_screen(
            connection,
            font_address,
            "BSD license page 1 after going back",
            lambda rows: has_fragment(rows, LICENSE_PAGE_1_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(
            license_1_again_attributes, "BSD license page 1 revisit"
        )

        send_ascii(connection, "q")
        _, _, returned_title_attributes = wait_for_screen(
            connection,
            font_address,
            "title after closing the BSD license",
            lambda rows: (
                has_fragment(rows, TITLE_OBJECT_FRAGMENT)
                and has_fragment(rows, TITLE_PLAYER_HEAP_FRAGMENT)
                and has_fragment(rows, TITLE_LICENSE_FRAGMENT)
                and has_fragment(rows, TITLE_KEYBOARD_FRAGMENT)
                and has_fragment(rows, TITLE_TELEPORT_FRAGMENT)
                and not has_fragment(rows, "JOYSTICK")
                and not has_fragment(rows, "KEMPSTON")
                and not has_fragment(rows, "MATRIX")
                and not has_fragment(rows, "T/0")
                and not has_fragment(rows, TITLE_GLYPH_SELECTOR_FRAGMENT)
            ),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(returned_title_attributes, "returned title screen")

        send_ascii(connection, " ")
        board_rows, _, board_attributes = wait_for_screen(
            connection,
            font_address,
            "game board with header, proper frame corners, and player",
            is_board,
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_board_colours(board_attributes, "game board")

        send_ascii(connection, "i")
        _, _, help_attributes = wait_for_screen(
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
        assert_white_on_black(help_attributes, "help screen")

        send_ascii(connection, " ")
        _, _, returned_board_attributes = wait_for_screen(
            connection,
            font_address,
            "game board after closing help",
            is_board,
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_board_colours(returned_board_attributes, "game board after help")

        send_ascii(connection, "q")
        quit_rows, _, quit_attributes = wait_for_screen(
            connection,
            font_address,
            f"changed quit modal containing {QUIT_FRAGMENT!r}",
            lambda rows: rows != board_rows and has_fragment(rows, QUIT_FRAGMENT),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_white_on_black(quit_attributes, "quit modal")

        send_ascii(connection, "n")
        _, bitmap, attributes = wait_for_screen(
            connection,
            font_address,
            "game board after declining quit",
            lambda rows: rows != quit_rows and is_board(rows),
            stable_for=SCREEN_STABLE_SECONDS,
        )
        assert_board_colours(attributes, "game board after declining quit")

        if args.screenshot is not None:
            save_ppm(args.screenshot.resolve(), bitmap, attributes)

        print(
            "ZEsarUX 48K smoke OK: keyboard-only control copy, fixed sprites, "
            "two-page BSD license, "
            "white-on-black UI, red player, game board, frame corners, help, "
            "quit cancellation, "
            "and return to play"
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
