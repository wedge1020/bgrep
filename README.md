# bgrep

`grep` for binary data. Describe the bytes you are looking for in hex and
`bgrep` reports where they occur:

    $ bgrep -H -m1 '7f 45 4c 46' /bin/ls /bin/cat
    /bin/ls:0:7f 45 4c 46
    /bin/cat:0:7f 45 4c 46

Output is `[file:][offset:]bytes`, with the offset in hexadecimal by default.

## Patterns

* **Default:** hex digits; `.` matches any one hex digit; whitespace is
  ignored. `'ff d8 ff ..'` is a JPEG marker plus any byte; `'4. 50'` is a
  byte in `0x40`-`0x4f` followed by `0x50`. Input is streamed in constant
  memory, so huge files and pipes are fine.
* **`-E` / `-G`:** a POSIX extended / basic regular expression applied to the
  data rendered as hex text, for alternation, repetition and anchors:
  `bgrep -E 'ff(d8|d9)'`, `bgrep -E '00{8,}'`. These modes load each input
  fully into memory.
* **Bitstream model:** the input is a stream of bits cut into
  `--input-byte=N`-bit "bytes" (1-128, default 8), so packed 6-, 7-, 10-, 12-
  or 14-bit data can be searched directly, with `--lsb-first` for formats that
  fill bytes from the low bit. `--output-byte=M` regroups what is displayed,
  `--align=BITS` (or `--half-byte` = `--align=4`) lets matches start off the
  byte grid (offset shown as `2+4`: two bytes and four bits), and `--binary`
  writes patterns one bit per symbol.
* Several patterns: repeat `-e`, or put them one per line in a file for `-f`.

## Options at a glance

| Area | Options |
|---|---|
| Pattern | `-F` `-G` `-E` `-e PAT` `-f FILE` `--binary` |
| Bitstream | `--input-byte=N` `--output-byte=M` `--align=B` `--half-byte` `--lsb-first` `--msb-first` `--bit-offset` |
| Selection | `-c` `-l` `-L` `-m N` `-q` `-s OFFSET` |
| Prefix | `-b` `--no-byte-offset` `-H` `-h` `--label=L` |
| Offset | `--first-byte[-with-prefix]` `--last-byte[-with-suffix]` |
| Numbers | `--auto-mode` `--hex-mode` `--oct-mode` `--dec-mode` `--[no-]hex-prefix` `--[no-]hex-suffix` `-p[N]` |
| Context | `-B N` `-A N` `-P K` `-S K` `-O [+-]N` |
| Display | `--ascii` `-g N` `--little-endian-group` `--byte-reverse-group` `--bit-reverse-group` `-I K[,K]` |

`-P K` / `-S K` take the number of context bytes from the value of the K-th
matched byte, which follows length fields in a record:

    $ bgrep -S 3 '41 42 ..' records.bin     # AB, length, then <length> bytes
    0:41 42 03 | aa bb cc

Exit status is 0 on a match, 1 on none, 2 on error. See `man bgrep` (or
`man ./man/man1/bgrep.1`) for the full reference and `docs/` for worked
examples and design notes.

Some examples of the bitstream options:

    $ bgrep --input-byte=12 --output-byte=12 'abc def' samples.bin   # 12-bit values
    $ bgrep --lsb-first --input-byte=7 --output-byte=7 --ascii '68 65 6c 6c 6f' sms.bin
    0:68 65 6c 6c 6f  |hello|                                         # GSM 7-bit "hello"
    $ bgrep --half-byte 865 data.bin                                  # nibbles inside 68 65
    2+4:86 5

## Building

Needs a C compiler and a POSIX `regex.h`. Works on Linux, macOS and OpenBSD
(with GNU make or the native make).

    make                  # builds bin/bgrep
    make test
    sudo make install     # bin/bgrep -> /usr/local/bin, man page -> section 1

The man page is installed to `/usr/local/share/man/man1` (`/usr/local/man/man1`
on OpenBSD). Override with `PREFIX=`, `BINDIR=`, `MANDIR=` (the man *root*)
or `DESTDIR=`. On OpenBSD, `bgrep` restricts itself with `pledge(2)`.

## Layout

    src/    C sources              inc/    headers
    man/man1/bgrep.1  manual page  docs/   design notes and examples
    tests/  test suite (make test) bin/    build output (not tracked)

## History

Started in 2015 as an exploration (and an exercise in writing a manual page).
Revived and rewritten in 2026: the original prototype did not support
multiple files, restarted matches incorrectly, and never actually used the
regex library.

GPLv3+, see `LICENSE`.
