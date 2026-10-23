# bgrep design notes

## Source layout

| File | Role |
|---|---|
| `inc/bgrep.h` | shared types: `Config`, `Pattern`, `Sink`, prototypes |
| `src/main.c` | file loop, summary output (`-c/-l/-L`), exit status |
| `src/args.c` | option parsing; numeric arguments are converted *after* all options are read so `--hex-mode` etc. apply wherever they appear |
| `src/pattern.c` | hex/wildcard and regex pattern compilation |
| `src/scan.c` | the two scanners and `-s` start-offset skipping |
| `src/output.c` | `emit()`: context, offsets, number bases, grouping, transforms |
| `src/util.c` | `die`/`warn`, checked allocation, string lists |
| `src/help.c`, `usage.c`, `version.c` | informational output |

## Positions are nibbles

Everything in the scanners is measured in nibbles (half bytes); nibble `2k`
is the high half of byte `k`. Normal matching steps two nibbles at a time,
`--half-byte` steps one. A pattern is stored as packed `value`/`mask` bytes
with one nibble per half-byte, so `.` is just mask `0`.

## Streaming scanner (default mode)

Reads 64 KiB chunks into a buffer that always holds:

* up to `max_before` bytes *before* the scan position (for `-B`/`-P` context),
* at least `longest pattern + max_after` bytes *after* it unless at end of file.

The scanner does not decide a position until that look-ahead is present, then
calls `emit()` with the window. After each read the window slides so memory
stays at `64 KiB + context + pattern length` regardless of file size.
`max_before`/`max_after` come from `-B/-A` plus 255 (+|`-O`|) when `-P/-S` are
used, since a length byte is at most 255. Context is capped at 1 MiB.

## Regex scanner (`-G`/`-E`)

POSIX `regexec` works on text, so the whole input is read and rendered as
lowercase hex (two characters per byte). A match is accepted only if it
starts and ends on an even character index (or anywhere with `--half-byte`);
otherwise the search resumes one character later. `REG_NOTBOL` is used for
every search after the first so `^` means "start of data". Per-pattern next
matches are cached so several patterns do not rescan.

Known limitation: if the leftmost-longest match is misaligned, a shorter
aligned match at the same spot is not tried.

## Output

`emit()` receives a window, the match position and length. It computes the
context available (clipped to the window, which at the true edges is the
file), applies `-I` to a scratch copy, and prints each of
*before / match / after* separately with the grouping rules: stream order, or
reversed for `--little-endian-group` XOR `--byte-reverse-group` XOR
`--bit-reverse-group`, plus a per-byte bit reversal for the latter.

## Decisions worth knowing

* `-f` reads patterns (grep semantics); files are positional.
* Patterns are not comma-split.
* Matches do not overlap; the first pattern wins ties.
* A bare `-O N` is negative, as the original manual page specified.
* `-g/--group-size`, `-A/-B`, `--dec-mode` and `--half-byte` are additions;
  the original manual referred to "grouped bytes" but had no way to set a
  group size.
* `--input-byte`/`--output-byte` (bytes of other than 8 bits) are
  unimplemented because the intended semantics for patterns that are not a
  multiple of four bits are not settled.

## Testing

`make test` runs `tests/run.sh` (~100 cases). During development the streaming
scanner was also checked against an independent reference implementation on
random multi-chunk data, with and without context and `--half-byte`, and run
under AddressSanitizer/UBSan.
