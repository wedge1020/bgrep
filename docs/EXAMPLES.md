# bgrep examples

All offsets are hexadecimal unless a `--dec-mode`/`--oct-mode` option is given.

## Finding known headers

    bgrep -H '7f 45 4c 46' /usr/bin/*          # ELF files (offset 0)
    bgrep -lE '^89504e470d0a1a0a' *            # list PNG files
    bgrep -E '504b(0304|0506)' archive.zip     # ZIP local header / end record
    bgrep -c 'ff d8 ff' photo.jpg              # count JPEG markers

`^` anchors to the start of the *data* (not a line).

## Wildcards

`.` stands for one hex digit:

    bgrep 'ff d8 ff e.'  img.jpg               # E0..EF application markers
    bgrep '68 65 .. .. 6f 00' data.bin         # "he??o\0"

## Records with length fields

A record `AB <len> <payload...>`; show each header with its payload:

    bgrep -S 3 '41 42 ..' records.bin

`-S 3` means "print as many following bytes as the 3rd matched byte says".
If the length also counts the 3 header bytes, subtract them:

    bgrep -S 3 -O 3 '41 42 ..' records.bin     # bare -O subtracts; -O +N adds

Fixed context works like `grep -B/-A`:

    bgrep -B 16 -A 16 'de ad be ef' core.dump

## Presenting the output

    bgrep --ascii -m1 '68 65 6c 6c 6f' f.bin   # 2:68 65 6c 6c 6f  |hello|
    bgrep -g 4 --little-endian-group -A 12 '7f 45 4c 46' a.out
    bgrep --dec-mode -p -B 8 -A 8 ff data.bin  # decimal, zero-padded offsets
    bgrep -I 1 '41 42' x.bin                   # print the first byte bit-inverted

## Searching part of a file, or a stream

    bgrep -s 0x100000 'de ad' disk.img         # begin at 1 MiB, offsets stay absolute
    dd if=/dev/sda bs=1M count=64 | bgrep -m5 '55 aa'

## Half-byte matching

    bgrep --half-byte 865 data.bin             # finds the nibbles 8,6,5 inside 68 65
    -> 2.5:86 5

## Many patterns

    printf 'dead beef\n7f454c46\n' > magic.txt
    bgrep -f magic.txt firmware.bin
