#!/usr/bin/env python3
"""Measure how long the game stays busy after a keypress, in emulated time.

What a player feels is the gap between pressing a key and getting the board
back, so that is what this measures: the game is "busy" whenever the Z80 is
executing anything other than the keyboard-polling loop it idles in.

Two ZEsarUX facilities make this reliable without touching the game:

* the idle loop is discovered empirically -- sampling PC while nothing is
  happening yields exactly the addresses of in_inkey() and its delay, so any
  other address means work is being done;
* get-tstates-partial is a free-running counter of *emulated* t-states, so the
  result is real Spectrum time and does not drift when the host is loaded or
  when polling over the remote protocol slows the emulator down.

ZEsarUX PC breakpoints are deliberately not used: they do not fire for code in
RAM here, and the t-state counter keeps advancing while the CPU is paused, so
bracketing a paused CPU would report the wall clock rather than Z80 work.
"""

from __future__ import annotations

import argparse
import os
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from zesarux_smoke import (  # noqa: E402
    DEFAULT_ZESARUX,
    command,
    connect,
    reserve_port,
)

# The 48K ULA and the TC2048 SCLD both clock the CPU at 3.5 MHz.
CPU_HZ = 3_500_000.0
BOOT_TIMEOUT_SECONDS = 60.0
BUSY_TIMEOUT_SECONDS = 30.0
IDLE_SAMPLE_SECONDS = 2.0
# A turn is over once the game has waited this long without doing anything;
# comfortably longer than the gap between a turn's two render_board() calls.
QUIET_MS = 200.0
# Below this a "turn" is just sampling noise, not a redraw -- which is how a
# keypress swallowed by a modal is recognised.  It has to stay well under a
# real redraw: those are now single-digit milliseconds on the Timex.
MINIMUM_BUSY_MS = 1.0
# The TC2048 keyboard map needs a keypress held about this long before it
# registers; the 48K is happy with less.
KEY_HOLD_SECONDS = 0.25
# A turn must be comfortably over before its addresses could be mistaken for
# waiting: the slowest measured turn is well under a second.
SETTLE_SECONDS = 2.5
MINIMUM_SAMPLES = 3
MAXIMUM_RESTARTS = 6
# Teleport: always accepted, always redraws the board, and -- unlike standing
# still -- does not walk the player into the robots after a handful of turns.
# Letter keys also survive the TC2048 keyboard map, which ignores "5".
TURN_KEY = ord("t")

PC_RE = re.compile(r"\bPC=([0-9A-Fa-f]{4})\b")
DIGITS_RE = re.compile(r"(\d+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Measure ZX BSD Robots board refresh time in ZEsarUX."
    )
    parser.add_argument("tap", type=Path, help="TAP image to benchmark")
    parser.add_argument(
        "--machine", default="48k", help="ZEsarUX machine name (48k, TC2048)"
    )
    parser.add_argument(
        "--samples", type=int, default=8, help="number of turns to time"
    )
    parser.add_argument(
        "--boot-keys", type=int, default=40,
        help="how many keypresses may be spent getting past the title",
    )
    return parser.parse_args()


def sample(connection) -> tuple[int, int]:
    """Return (program counter, emulated t-state counter)."""
    registers = command(connection, "get-registers")
    counter = PC_RE.search(registers)
    partial = command(connection, "get-tstates-partial")
    tstates = DIGITS_RE.search(partial.replace("command> ", ""))
    if counter is None or tstates is None:
        raise AssertionError(f"cannot sample CPU: {registers!r} {partial!r}")
    return int(counter.group(1), 16), int(tstates.group(1))


def reset_clock(connection) -> None:
    """Restart the emulated t-state counter.

    It is only 9 digits wide and reports OVERFLOW after a few minutes of
    emulated time, which a run with several restarts comfortably exceeds.
    Every measurement below is a short window, so zeroing it at each window
    start keeps the counter far away from that limit.
    """
    command(connection, "reset-tstates-partial")


def press_char(connection, character: str) -> None:
    code = ord(character)
    command(connection, f"send-keys-event {code} 1")
    time.sleep(KEY_HOLD_SECONDS)
    command(connection, f"send-keys-event {code} 0")


def press_key(connection) -> None:
    command(connection, f"send-keys-event {TURN_KEY} 1")


def release_key(connection) -> None:
    command(connection, f"send-keys-event {TURN_KEY} 0")


def tap_key(connection) -> None:
    """Press and release the turn key the way a player would.

    The hold matters: a down/up pair delivered inside a single emulated frame
    can be missed entirely, and a lost release leaves read_key() spinning in
    in_wait_nokey() forever.
    """
    press_key(connection)
    time.sleep(KEY_HOLD_SECONDS)
    release_key(connection)


def sample_addresses(connection, seconds: float) -> set[int]:
    addresses: set[int] = set()
    reset_clock(connection)
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        addresses.add(sample(connection)[0])
    if not addresses:
        raise AssertionError("could not sample the CPU")
    return addresses


def learn_waiting_addresses(connection) -> set[int]:
    """Collect every address the game visits while it is *not* redrawing.

    That is two loops, not one: in_inkey() polling for a key, and the
    in_wait_nokey() spin that follows a keypress until the key comes back up.
    Counting the second as work would charge the redraw for how long a finger
    stayed on the key.
    """
    waiting = sample_addresses(connection, IDLE_SAMPLE_SECONDS)
    press_key(connection)
    time.sleep(0.2)  # let read_key() notice the key and enter in_wait_nokey()
    waiting |= sample_addresses(connection, IDLE_SAMPLE_SECONDS)
    release_key(connection)
    time.sleep(SETTLE_SECONDS)  # the release triggers a turn; let it finish
    return waiting | sample_addresses(connection, IDLE_SAMPLE_SECONDS)


def wait_until_idle(connection, waiting: set[int], timeout: float) -> None:
    reset_clock(connection)
    deadline = time.monotonic() + timeout
    idle_ms = 0.0
    previous = None
    while time.monotonic() < deadline:
        counter, tstates = sample(connection)
        if previous is not None:
            elapsed = (tstates - previous) / CPU_HZ * 1000.0
            idle_ms = idle_ms + elapsed if counter in waiting else 0.0
            if idle_ms >= QUIET_MS:
                return
        previous = tstates
    raise AssertionError(f"game stayed busy for more than {timeout:.0f}s")


def time_one_turn(connection, waiting: set[int]) -> float | None:
    """Play one turn and return its redraw cost in ms, or None if ignored.

    The cost is the emulated time the CPU spends outside the waiting loops,
    summed rather than bracketed.  Summing matters: one turn is two separate
    render_board() calls with a hairline gap between them, and the idle loop
    occasionally visits an address that sampling never caught, which would cut
    a bracketed window short or stretch it.  Neither distorts a sum.

    A key is ignored once the game sits in a modal such as GAME OVER, which is
    the natural end of a benchmark run that keeps standing still.
    """
    reset_clock(connection)
    tap_key(connection)
    deadline = time.monotonic() + BUSY_TIMEOUT_SECONDS
    busy_ms = 0.0
    quiet_ms = 0.0
    previous: int | None = None
    while time.monotonic() < deadline:
        counter, tstates = sample(connection)
        if previous is not None:
            elapsed = (tstates - previous) / CPU_HZ * 1000.0
            if counter in waiting:
                quiet_ms += elapsed
                if quiet_ms >= QUIET_MS:
                    return busy_ms if busy_ms >= MINIMUM_BUSY_MS else None
            else:
                busy_ms += elapsed
                quiet_ms = 0.0
        previous = tstates
    raise AssertionError("turn never finished")


def reach_board(connection, boot_keys: int) -> set[int]:
    """Get past the title screen and return the *in-game* idle addresses.

    The title screen idles in its own polling loop at different addresses, so
    the idle set has to be relearned once the board is up -- otherwise every
    in-game sample looks busy and no turn ever appears to end.
    """
    deadline = time.monotonic() + BOOT_TIMEOUT_SECONDS
    for _ in range(boot_keys):
        if time.monotonic() > deadline:
            break
        title_waiting = learn_waiting_addresses(connection)
        busy = time_one_turn(connection, title_waiting)
        if busy is not None:
            # The key that left the title also drew the first board; let it
            # settle, then learn the keyboard loop play_game() really waits in.
            time.sleep(SETTLE_SECONDS)
            return learn_waiting_addresses(connection)
    raise AssertionError("game never reached the board")


def main() -> int:
    args = parse_args()
    emulator = Path(os.environ.get("ZESARUX", str(DEFAULT_ZESARUX))).resolve()
    if not args.tap.is_file():
        raise SystemExit(f"TAP not found: {args.tap}")
    if not emulator.is_file():
        raise SystemExit(f"ZEsarUX not found: {emulator}")

    port = reserve_port()
    process = subprocess.Popen(
        [
            str(emulator), "--noconfigfile", "--machine", args.machine,
            "--tape", str(args.tap.resolve()), "--vo", "null", "--ao", "null",
            "--nosplash", "--nowelcomemessage", "--enable-remoteprotocol",
            "--remoteprotocol-port", str(port), "--quickexit", "--fastautoload",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    connection = None
    try:
        connection = connect(port)
        time.sleep(5.0)  # let --fastautoload finish before pressing anything
        waiting = reach_board(connection, args.boot_keys)
        timings: list[float] = []
        restarts = 0
        while len(timings) < args.samples:
            wait_until_idle(connection, waiting, BUSY_TIMEOUT_SECONDS)
            busy = time_one_turn(connection, waiting)
            if busy is not None:
                timings.append(busy)
                continue
            # Teleporting is deliberately unsafe, so the player does get killed.
            # Take the GAME OVER modal's own offer to play again and carry on.
            restarts += 1
            if restarts > MAXIMUM_RESTARTS:
                break
            press_char(connection, "r")
            time.sleep(SETTLE_SECONDS)
            waiting = reach_board(connection, args.boot_keys)
        if len(timings) < MINIMUM_SAMPLES:
            raise AssertionError(
                f"only {len(timings)} turns measured in {restarts} game(s)"
            )
    finally:
        if connection is not None:
            connection.close()
        process.terminate()
        process.wait(timeout=10)

    print(f"{args.tap.name} on {args.machine}: {len(timings)} turns")
    for index, milliseconds in enumerate(timings):
        print(f"  turn {index + 1}: {milliseconds:7.1f} ms")
    print(
        f"  median:  {statistics.median(timings):7.1f} ms"
        f"   min {min(timings):.1f}   max {max(timings):.1f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
