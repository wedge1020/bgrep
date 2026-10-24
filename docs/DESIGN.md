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

## The bitstream model

Everything is measured in **bits**. The input is a stream of bits (file bytes
MSB-first by default, LSB-first with `--lsb-first`) cut into `--input-byte`
(N) bit "bytes" starting at the starting offset (the *grid origin*). A
candidate match may begin every `--align` bits (A) from the origin; by default
A = N. `--half-byte` is A = 4.

A fixed pattern is compiled to its bits **in stream order** (`bv`/`bm`, one
byte per bit, plus packed `value`/`mask` bytes laid out like the file). Hex
digits are first turned into value-order bits per input byte (the leading
digit of a byte holds only N mod 4 bits), then each byte is reversed for
`--lsb-first`, so matching itself knows nothing about bit order. When a
candidate starts on a file-byte boundary the packed bytes are compared
directly (the common case, and the only one for plain byte data); otherwise
bits are compared one at a time.

After a match the scan resumes at the next point of the alignment grid, which
stays anchored at the origin rather than at the match.

## Streaming scanner (default mode)

Reads 64 KiB chunks into a buffer that always holds:

* up to `max_before` input bytes (converted to file bytes) of history before
  the scan position, for leading context (`-B`/`-P`),
* at least `longest pattern + max_after` input bytes after it unless at end of
  file, for trailing context (`-A`/`-S`).

The scanner does not decide a position until that look-ahead is present, then
calls `emit()` with the window. After each read the window slides so memory
stays bounded regardless of file size. `max_before`/`max_after` come from
`-B/-A` plus, when `-P/-S` are used, the largest value an input byte can hold
(255 for 8 bits and below, otherwise the 1 Mi cap) plus `|-O|`. Context is
capped at 1 Mi input bytes each way.

## Regex scanner (`-G`/`-E`)

POSIX `regexec` works on text, so the whole input is read and each input byte
is rendered as text: `ceil(N/4)` lowercase hex digits holding its value, or
N `0`/`1` characters with `--binary`. A match is accepted only if it starts
and ends on a byte boundary of that text; with `--align` different from N it
need only start on the alignment grid (this is refused with `--lsb-first`,
where text characters do not map to contiguous stream bits). `REG_NOTBOL` is
used for every search after the first so `^` means "start of data". Per-pattern
next matches are cached so several patterns do not rescan.

Known limitation: if the leftmost-longest match is unacceptable, a shorter
acceptable match at the same spot is not tried.

## Output

`emit()` receives a window, the match's bit position and length. It computes
the context available (counted in input bytes and clipped to the window, which
at the true edges is the file) and prints *before / match / after* separately.
Each is loaded as a bit array, `-I` flips all bits of chosen input bytes in
the match, and the bits are regrouped into `--output-byte` units and collected
into `-g` groups. Group order is stream order, reversed for
`--little-endian-group` XOR `--byte-reverse-group` XOR `--bit-reverse-group`,
the last also reversing the bits inside each unit. A unit's value is the stream
bits MSB-first, or reversed for `--lsb-first`, so `--output-byte=8` shows the
original file bytes whatever the input width. A short final unit shows only
the digits it covers (`~` if its length is not a multiple of 4).

Offsets are reported in input bytes, `+BITS` when off the grid, or bits with
`--bit-offset`; they are always from the start of the file even with `-s`.

## Decisions worth knowing

* `-f` reads patterns (grep semantics); files are positional.
* Patterns are not comma-split.
* Matches do not overlap; the first pattern wins ties.
* A bare `-O N` is negative, as the original manual page specified.
* `-g/--group-size`, `-A/-B`, `--dec-mode`, `--align`, `--binary`,
  `--lsb-first` and `--bit-offset` are additions; the original manual referred
  to "grouped bytes" but had no way to set a group size.
* Off-grid offsets are written `UNITS+BITS` (e.g. `2+4`) rather than a
  decimal fraction, which is ambiguous for widths like 12 or 7.
* `--input-byte`/`--output-byte` regroup the bitstream; they do not
  zero-extend. To see packed 7-bit text as one value per character use
  `--input-byte=7 --output-byte=7`.

## Testing

`make test` runs `tests/run.sh` (~145 cases). During development the scanner
was also checked against an independent Python reference of the whole model
on random multi-chunk data (input/output widths 1-128, alignment, bit order,
binary and hex patterns, `-s`, context, grouping, `-I`), with the regex mode
cross-checked against the fixed mode, and run under AddressSanitizer/UBSan.
