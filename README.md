# bgrep

`grep` for binary data. Describe the bytes you are looking for in hex and
`bgrep` reports where they occur:

    $ bgrep -H '7f 45 4c 46' /bin/ls /bin/cat
    /bin/ls:0:7f 45 4c 46
    /bin/cat:0:7f 45 4c 46

Output is `[file:][offset:]bytes`, with the offset in hexadecimal.

## Patterns

* **Default:** hex digits, `.` matches any one hex digit, whitespace is
  ignored. `'ff d8 ff ..'` is a JPEG marker plus any byte; `'4. 50'`
  is a byte in `0x40`-`0x4f` followed by `0x50`. Input is streamed in
  constant memory, so huge files and pipes are fine.
* **`-E` / `-G`:** a POSIX extended / basic regular expression applied to the
  data rendered as hex text, for alternation, repetition and anchors:
  `bgrep -E 'ff(d8|d9)'`, `bgrep -E '00{8,}'`. Matches must fall on byte
  boundaries. These modes load each input fully into memory.
* Several patterns: repeat `-e`, or put them one per line in a file for `-f`.

## Options

| | |
|---|---|
| `-F` `-G` `-E` | pattern type: hex (default), basic regex, extended regex |
| `-e PAT` `-f FILE` | pattern(s) / file of patterns |
| `-c` `-l` `-L` `-m N` `-q` | count / list matching / list non-matching / limit / quiet |
| `-b` `--no-byte-offset` | show (default) or hide the offset |
| `-H` `-h` `--label=L` | file name prefix control, name for stdin |

Exit status is 0 on a match, 1 on none, 2 on error. See `man bgrep` (or
`man ./man/bgrep.1`) for full details and examples.

The manual page also reserves a number of further options (offset formats,
context bytes, ASCII column, byte inversion and regrouping). They are
recognized but report "not yet implemented"; see the roadmap below.

## Building

Needs a C compiler and a POSIX `regex.h`. Works on Linux, macOS and OpenBSD
(with GNU make or the native make).

    make
    make test
    sudo make install                      # PREFIX=/usr/local by default
    make install MANDIR=/usr/local/man     # e.g. OpenBSD layout

On OpenBSD, `bgrep` restricts itself with `pledge(2)`.

## Roadmap

1. Offset display: `--first-byte`/`--last-byte`, hex/octal modes, prefixes,
   zero padding, `--ascii` hex-editor column
2. Context: `-P`/`-S` prefix/suffix bytes, `-s` start offset
3. Data transforms: `-I` invert, endian/byte-reverse grouping,
   `--input-byte`/`--output-byte`

## History

Started in 2015 as an exploration (and an exercise in writing a manual page).
Revived and rewritten in 2026: the original prototype did not support
multiple files, restarted matches incorrectly, and never actually used the
regex library.

GPLv3+, see `LICENSE`.
