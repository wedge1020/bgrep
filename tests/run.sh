#!/bin/sh
# bgrep test suite.  Usage: sh tests/run.sh ./bgrep
BG=${1:-./bgrep}
T=$(mktemp -d "${TMPDIR:-/tmp}/bgrep.XXXXXX") || exit 2
trap 'rm -rf "$T"' EXIT
pass=0; fail=0

# check NAME EXPECTED_OUTPUT EXPECTED_STATUS COMMAND...
check() {
	name=$1; want=$2; wantrc=$3; shift 3
	got=$("$@" 2>"$T/err"); rc=$?
	if [ "$got" = "$want" ] && [ "$rc" = "$wantrc" ]; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
		printf 'FAIL: %s\n  want (rc=%s): %s\n  got  (rc=%s): %s\n' \
			"$name" "$wantrc" "$want" "$rc" "$got"
	fi
}

printf 'xxhello\000yyhelp\000hello\000' > "$T/a.bin"
printf '\252\252\252\253\252' > "$T/b.bin"
printf 'nothing here' > "$T/c.bin"
printf 'll' > "$T/d.bin"

check "literal with offset"      "2:68 65 6c 6c 6f 00" 0 "$BG" -m1 '68 65 6c 6c 6f 00' "$T/a.bin"
check "wildcard nibbles"         "2:68 65 6c 6c 6f 00
f:68 65 6c 6c 6f 00" 0 "$BG" '68 65 .. .. 6f 00' "$T/a.bin"
check "half-byte wildcard"       "3:65 6c
b:65 6c
10:65 6c" 0 "$BG" '6. 6c' "$T/a.bin"
check "uppercase hex"            "3:ab" 0 "$BG" 'AB' "$T/b.bin"
check "restart after partial"    "2:aa ab" 0 "$BG" 'aa ab' "$T/b.bin"
check "no match"                 "" 1 "$BG" 'dead beef' "$T/a.bin"
check "stdin"                    "2:68 65" 0 sh -c "'$BG' -m1 6865 < '$T/a.bin'"
check "label"                    "x:2:68 65" 0 sh -c "'$BG' -H --label=x 6865 -m1 < '$T/a.bin'"
check "multi-file prefix"        "$T/a.bin:4:6c 6c
$T/d.bin:0:6c 6c" 0 "$BG" -m1 '6c 6c' "$T/a.bin" "$T/c.bin" "$T/d.bin"
check "count"                    "3" 0 "$BG" -c '68 65' "$T/a.bin"
check "count none"               "0" 1 "$BG" -c 'dead' "$T/a.bin"
check "max count"                "2:68 65" 0 "$BG" -m 1 '68 65' "$T/a.bin"
check "no offset"                "aa ab" 0 "$BG" --no-byte-offset 'aa ab' "$T/b.bin"
check "-l"                       "$T/a.bin" 0 "$BG" -l '6c 6c' "$T/a.bin" "$T/c.bin"
check "-L"                       "$T/c.bin" 0 "$BG" -L '6c 6c' "$T/a.bin" "$T/c.bin"
check "-H filename"              "$T/b.bin:3:ab" 0 "$BG" -H ab "$T/b.bin"
check "quiet match"              "" 0 "$BG" -q 'ab' "$T/b.bin"
check "quiet no match"           "" 1 "$BG" -q 'dead' "$T/b.bin"
check "-e multiple"              "0:aa aa
3:ab" 0 "$BG" -e ab -e 'aa aa' "$T/b.bin"
check "odd digits rejected"      "" 2 "$BG" 'abc' "$T/a.bin"
check "bad symbol rejected"      "" 2 "$BG" 'zz' "$T/a.bin"
check "missing file"             "" 2 "$BG" 'ab' "$T/nonexistent"
check "no args"                  "" 2 "$BG"
check "unimplemented option"     "" 2 "$BG" --input-byte=7 'ab' "$T/b.bin"
check "ERE alternation"          "2:68 65 6c 6c
a:68 65 6c 70
f:68 65 6c 6c" 0 "$BG" -E '68656c(6c|70)' "$T/a.bin"
check "ERE finds hello"          "2:68 65 6c 6c 6f" 0 "$BG" -m1 -E '68656c6c6f' "$T/a.bin"
check "ERE misaligned skipped"   "" 1 "$BG" -E '8656' "$T/a.bin"
check "BRE wildcard"             "2:68 65 6c 6c" 0 "$BG" -m1 -G '68656c..' "$T/a.bin"
check "regex bad"                "" 2 "$BG" -E '(' "$T/a.bin"

# pattern file
printf 'dead\n\n6865\n' > "$T/pats"
check "-f pattern file"          "2:68 65" 0 "$BG" -m1 -f "$T/pats" "$T/a.bin"

# chunk boundary: match straddling the 64 KiB read boundary
dd if=/dev/zero bs=65535 count=1 2>/dev/null > "$T/big.bin"
printf '\336\255\276\357' >> "$T/big.bin"
dd if=/dev/zero bs=1000 count=1 2>/dev/null >> "$T/big.bin"
check "chunk boundary"           "ffff:de ad be ef" 0 "$BG" 'de ad be ef' "$T/big.bin"
check "chunk boundary (ERE)"     "ffff:de ad be ef" 0 "$BG" -E 'deadbeef' "$T/big.bin"

# empty input and directory
: > "$T/empty"
check "empty file"               "" 1 "$BG" 'ab' "$T/empty"
check "directory"                "" 2 "$BG" 'ab' "$T"


# ---- step 5: offset formatting -------------------------------------------
printf '\101\102\003\252\273\314\335\356' > "$T/r.bin"        # AB 03 aa bb cc dd ee
printf '\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\377' > "$T/o.bin"  # ff at 16
printf '\000\000\000\377\000\000\000\000\000\000\000\000\000\000\000\000\000' > "$T/z.bin"  # ff at 3
printf '\253\253\253\253\253\253\253\253\253\253\253\253\253\253\253\253' > "$T/s.bin"      # 16 x ab

check "last-byte"                "1:41 42" 0 "$BG" --last-byte '41 42' "$T/r.bin"
check "last-byte-with-suffix"    "3:41 42 | 03 aa" 0 "$BG" --last-byte-with-suffix -A 2 '41 42' "$T/r.bin"
check "first-byte-with-prefix"   "1:42 03 | aa" 0 "$BG" --first-byte-with-prefix -B 2 'aa' "$T/r.bin"
check "default hex offset"       "10:ff" 0 "$BG" ff "$T/o.bin"
check "dec-mode offset"          "16:ff" 0 "$BG" --dec-mode ff "$T/o.bin"
check "oct-mode offset"          "20:ff" 0 "$BG" --oct-mode ff "$T/o.bin"
check "hex-prefix"               "0x10:ff" 0 "$BG" --hex-prefix ff "$T/o.bin"
check "hex-suffix"               "10h:ff" 0 "$BG" --hex-suffix ff "$T/o.bin"
check "hex-prefix+suffix"        "0x10h:ff" 0 "$BG" --hex-prefix --hex-suffix ff "$T/o.bin"
check "prefix ignored in dec"    "16:ff" 0 "$BG" --dec-mode --hex-prefix ff "$T/o.bin"
check "pad explicit"             "0010:ff" 0 "$BG" -p4 ff "$T/o.bin"
check "pad auto (size)"          "03:ff" 0 "$BG" -p ff "$T/z.bin"
check "no-pad"                   "3:ff" 0 "$BG" -p4 --no-pad-output ff "$T/z.bin"
check "count default decimal"    "16" 0 "$BG" -c ab "$T/s.bin"
check "count hex-mode"           "10" 0 "$BG" -c --hex-mode ab "$T/s.bin"
check "count oct-mode"           "20" 0 "$BG" -c --oct-mode ab "$T/s.bin"
check "ascii column"             "2:68 65 6c 6c 6f  |hello|" 0 "$BG" --ascii -m1 '68 65 6c 6c 6f' "$T/a.bin"
check "ascii non-printable"      "0:41 42 03  |AB.|" 0 "$BG" --ascii '41 42 03' "$T/r.bin"

# ---- step 6: context and start offset ------------------------------------
check "-A"                       "0:41 42 | 03 aa" 0 "$BG" -A 2 '41 42' "$T/r.bin"
check "-B"                       "3:42 03 | aa" 0 "$BG" -B 2 'aa' "$T/r.bin"
check "-A and -B"                "3:03 | aa | bb" 0 "$BG" -B 1 -A 1 'aa' "$T/r.bin"
check "context clipped at edges" "0:41 42" 0 "$BG" -B 10 '41 42' "$T/r.bin"
check "context clipped at EOF"   "7:ee" 0 "$BG" -A 10 'ee' "$T/r.bin"
check "-S from match byte"       "0:41 42 03 | aa bb cc" 0 "$BG" -S 3 '41 42 ..' "$T/r.bin"
check "-S with -O +1"            "0:41 42 03 | aa bb cc dd" 0 "$BG" -S 3 -O +1 '41 42 ..' "$T/r.bin"
check "-S with bare -O (minus)"  "0:41 42 03 | aa bb" 0 "$BG" -S 3 -O 1 '41 42 ..' "$T/r.bin"
check "-P from match byte"       "2:41 42 | 03 aa" 0 "$BG" -P 1 '03 aa' "$T/r.bin"
check "-S beyond match ignored"  "0:41 42" 0 "$BG" -S 9 '41 42' "$T/r.bin"
check "-s start offset"          "5:cc" 0 "$BG" -s 4 cc "$T/r.bin"
check "-s past match"            "" 1 "$BG" -s 6 cc "$T/r.bin"
check "-s on a pipe"             "5:cc" 0 sh -c "cat '$T/r.bin' | '$BG' -s 4 cc"
check "-s hex-mode"              "5:cc" 0 "$BG" --hex-mode -s 4 cc "$T/r.bin"
check "-s 0x prefix"             "5:cc" 0 "$BG" -s 0x4 cc "$T/r.bin"
check "-s regex"                 "5:cc" 0 "$BG" -E -s 4 cc "$T/r.bin"
check "context across chunks"    "ffff:00 00 00 | de ad be ef | 00 00 00" 0 "$BG" -B 3 -A 3 'de ad be ef' "$T/big.bin"
check "context across (ERE)"     "ffff:00 00 00 | de ad be ef | 00 00 00" 0 "$BG" -E -B 3 -A 3 'deadbeef' "$T/big.bin"
check "-S across chunks"         "ffff:de ad be ef | 00 00 00 00" 0 "$BG" -S 4 -O 235 'de ad be ef' "$T/big.bin"

# ---- step 7: byte transforms ---------------------------------------------
check "invert 1"                 "0:be 42" 0 "$BG" -I 1 '41 42' "$T/r.bin"
check "invert list"              "0:be bd" 0 "$BG" -I 1,2 '41 42' "$T/r.bin"
check "invert repeated -I"       "0:be bd" 0 "$BG" -I 1 -I 2 '41 42' "$T/r.bin"
check "invert out of range"      "0:41 42" 0 "$BG" -I 9 '41 42' "$T/r.bin"
check "invert keeps ascii raw"   "0:be 42  |AB|" 0 "$BG" -I 1 --ascii '41 42' "$T/r.bin"
check "group 2"                  "2:6865 6c6c 6f" 0 "$BG" -g 2 -m1 '68 65 6c 6c 6f' "$T/a.bin"
check "little-endian group"      "2:6568 6c6c 6f" 0 "$BG" -g 2 --little-endian-group -m1 '68 65 6c 6c 6f' "$T/a.bin"
check "byte-reverse group"       "2:6568 6c6c 6f" 0 "$BG" -g 2 --byte-reverse-group -m1 '68 65 6c 6c 6f' "$T/a.bin"
check "LE + byte-reverse"        "2:6865 6c6c 6f" 0 "$BG" -g 2 --little-endian-group --byte-reverse-group -m1 '68 65 6c 6c 6f' "$T/a.bin"
check "big-endian overrides"     "2:6865 6c6c 6f" 0 "$BG" -g 2 --little-endian-group --big-endian-group -m1 '68 65 6c 6c 6f' "$T/a.bin"
printf '\360\001' > "$T/bits.bin"
check "bit-reverse 1-byte group" "0:0f 80" 0 "$BG" --bit-reverse-group 'f0 01' "$T/bits.bin"
check "bit-reverse 2-byte group" "0:800f" 0 "$BG" -g 2 --bit-reverse-group 'f0 01' "$T/bits.bin"
check "group applies to context" "4:03aa | bb | ccdd" 0 "$BG" -g 2 -B 2 -A 2 'bb' "$T/r.bin"
check "bad group size"           "" 2 "$BG" -g 0 ab "$T/b.bin"
check "huge group size"          "" 2 "$BG" -g 99999 ab "$T/b.bin"
check "bad number"               "" 2 "$BG" -m x ab "$T/b.bin"
check "negative number"          "" 2 "$BG" -s -1 ab "$T/b.bin"
check "option order irrelevant"  "10:ff" 0 "$BG" -s 0x10 --hex-mode ff "$T/o.bin"

# ---- half-byte matching --------------------------------------------------
check "half-byte odd offset"     "2+4:86 56" 0 "$BG" --half-byte -m1 8656 "$T/a.bin"
check "half-byte odd digits"     "2+4:86 5" 0 "$BG" --half-byte -m1 865 "$T/a.bin"
check "half-byte wildcard"       "2+4:86 5" 0 "$BG" --half-byte -m1 '8.5' "$T/a.bin"
check "half-byte aligned"        "2:68 65" 0 "$BG" --half-byte -m1 6865 "$T/a.bin"
check "half-byte ERE"            "2+4:86 56" 0 "$BG" --half-byte -E -m1 8656 "$T/a.bin"
check "half-byte last offset"    "3+4:86 56" 0 "$BG" --half-byte --last-byte -m1 8656 "$T/a.bin"
check "odd digits need flag"     "" 2 "$BG" 865 "$T/a.bin"
check "half-byte + -A"           "2+4:86 5 | 6c" 0 "$BG" --half-byte -m1 -A 1 865 "$T/a.bin"
check "half-byte + ascii"        "2+4:86 5  |.|" 0 "$BG" --half-byte -m1 --ascii 865 "$T/a.bin"
check "half-byte + -I"           "2+4:79 5" 0 "$BG" --half-byte -m1 -I 1 865 "$T/a.bin"

# ---- bitstream model: --input-byte / --output-byte / --align -------------
printf '\253\315\357' > "$T/w12.bin"                    # ab cd ef = 12-bit abc def
printf 'ABC' > "$T/w6.bin"                              # 41 42 43 = 6-bit 10 14 09 03
printf '\350\062\233\375\006' > "$T/gsm.bin"            # "hello" in GSM 7-bit packing
printf '\200\001' > "$T/bit.bin"

check "12-bit units, 8-bit view" "0:ab cd ef" 0 "$BG" --input-byte=12 'abc def' "$T/w12.bin"
check "12-bit units, 12-bit out" "0:abc def" 0 "$BG" --input-byte=12 --output-byte=12 'abc def' "$T/w12.bin"
check "12-bit unit offset"       "1:def" 0 "$BG" --input-byte=12 --output-byte=12 def "$T/w12.bin"
check "12-bit misaligned"        "" 1 "$BG" --input-byte=12 bcd "$T/w12.bin"
check "12-bit --align=4"         "0+4:bcd" 0 "$BG" --input-byte=12 --output-byte=12 --align=4 bcd "$T/w12.bin"
check "12-bit whole bytes only"  "" 2 "$BG" --input-byte=12 abcd "$T/w12.bin"
check "12-bit bit offset"        "c:de f" 0 "$BG" --input-byte=12 --bit-offset def "$T/w12.bin"
check "bit offset (8-bit, hex)"  "80:ff" 0 "$BG" --bit-offset ff "$T/o.bin"
check "bit offset (dec)"         "128:ff" 0 "$BG" --dec-mode --bit-offset ff "$T/o.bin"
check "12-bit -s absolute"       "1:def" 0 "$BG" --input-byte=12 --output-byte=12 -s 1 def "$T/w12.bin"
check "12-bit -s skips"          "" 1 "$BG" --input-byte=12 --output-byte=12 -s 1 abc "$T/w12.bin"
check "12-bit context after"     "0:abc | def" 0 "$BG" --input-byte=12 --output-byte=12 -A 1 abc "$T/w12.bin"
check "12-bit context before"    "1:abc | def" 0 "$BG" --input-byte=12 --output-byte=12 -B 1 def "$T/w12.bin"
check "12-bit -S from unit value" "0:abc | def" 0 "$BG" --input-byte=12 --output-byte=12 -S 1 abc "$T/w12.bin"
check "12-bit invert"            "0:543" 0 "$BG" --input-byte=12 --output-byte=12 -I 1 abc "$T/w12.bin"
check "6-bit units"              "1:14 09" 0 "$BG" --input-byte=6 --output-byte=6 '14 09' "$T/w6.bin"
check "6-bit, 8-bit view"        "1:50 9" 0 "$BG" --input-byte=6 '14 09' "$T/w6.bin"
check "6-bit wildcard digit"     "1:14 09" 0 "$BG" --input-byte=6 --output-byte=6 '.4 0.' "$T/w6.bin"
check "6-bit leading digit range" "" 2 "$BG" --input-byte=6 '40' "$T/w6.bin"
check "6-bit binary"             "1:14 09" 0 "$BG" --binary --input-byte=6 --output-byte=6 '010100 001001' "$T/w6.bin"
check "6-bit binary wildcards"   "1:14 09" 0 "$BG" --binary --input-byte=6 --output-byte=6 '0101.. 00....' "$T/w6.bin"
check "6-bit regex (hex text)"   "1:14 09" 0 "$BG" -E --input-byte=6 --output-byte=6 '1409' "$T/w6.bin"
check "6-bit regex (binary)"     "1:14 09" 0 "$BG" -E --binary --input-byte=6 --output-byte=6 '010100001001' "$T/w6.bin"
check "12-bit regex"             "0:abc" 0 "$BG" -E --input-byte=12 --output-byte=12 'abc' "$T/w12.bin"
check "12-bit regex misaligned"  "" 1 "$BG" -E --input-byte=12 --output-byte=12 'bcd' "$T/w12.bin"
check "12-bit regex --align=4"   "0+4:bcd" 0 "$BG" -E --input-byte=12 --output-byte=12 --align=4 'bcd' "$T/w12.bin"
check "GSM 7-bit septets"        "0:68 65 6c 6c 6f" 0 "$BG" --lsb-first --input-byte=7 --output-byte=7 '68 65 6c 6c 6f' "$T/gsm.bin"
check "GSM 7-bit ascii"          "0:68 65 6c 6c 6f  |hello|" 0 "$BG" --lsb-first --input-byte=7 --output-byte=7 --ascii '68 65 6c 6c 6f' "$T/gsm.bin"
check "GSM back to octets"       "0:e8 32 9b fd 6~" 0 "$BG" --lsb-first --input-byte=7 '68 65 6c 6c 6f' "$T/gsm.bin"
check "GSM regex"                "0:68 65 6c 6c 6f" 0 "$BG" -E --lsb-first --input-byte=7 --output-byte=7 '68656c6c6f' "$T/gsm.bin"
check "lsb-first 8-bit same"     "0:ab cd ef" 0 "$BG" --lsb-first 'ab cd ef' "$T/w12.bin"
check "msb-first overrides lsb"  "0:ab cd ef" 0 "$BG" --lsb-first --msb-first 'ab cd ef' "$T/w12.bin"
check "lsb-first partial pattern" "" 2 "$BG" --lsb-first --half-byte 'abc' "$T/w12.bin"
check "1-bit pattern, --align=1" "0:8~
1+7:8~" 0 "$BG" --binary --align=1 1 "$T/bit.bin"
check "output-byte=4"            "0:a b c d" 0 "$BG" --output-byte=4 'ab cd' "$T/w12.bin"
check "output-byte=4 grouped"    "0:ab cd" 0 "$BG" --output-byte=4 -g 2 'ab cd' "$T/w12.bin"
check "output-byte=16"           "0:abcd ef" 0 "$BG" --output-byte=16 'ab cd ef' "$T/w12.bin"
check "ascii needs out<=8"       "" 2 "$BG" --ascii --output-byte=9 ab "$T/w12.bin"
check "input-byte 0"             "" 2 "$BG" --input-byte=0 ab "$T/w12.bin"
check "input-byte 129"           "" 2 "$BG" --input-byte=129 ab "$T/w12.bin"
check "output-byte 129"          "" 2 "$BG" --output-byte=129 ab "$T/w12.bin"
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ012345' > "$T/alpha.bin"      # two 128-bit units
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' > "$T/alpha26.bin"          # second unit incomplete
check "128-bit bytes"            "0:4142434445464748494a4b4c4d4e4f50" 0 "$BG" --input-byte=128 --output-byte=128 '4142434445464748494a4b4c4d4e4f50' "$T/alpha.bin"
check "128-bit second unit"      "1:5152535455565758595a303132333435" 0 "$BG" --input-byte=128 --output-byte=128 '5152535455565758595a303132333435' "$T/alpha.bin"
check "128-bit context"          "0:4142434445464748494a4b4c4d4e4f50 | 5152535455565758595a303132333435" 0 "$BG" --input-byte=128 --output-byte=128 -A 1 '4142434445464748494a4b4c4d4e4f50' "$T/alpha.bin"
check "128-bit -s"               "1:5152535455565758595a303132333435" 0 "$BG" --input-byte=128 --output-byte=128 -s 1 '5152535455565758595a303132333435' "$T/alpha.bin"
check "128-bit partial tail"     "" 2 "$BG" --input-byte=128 '5152535455565758595a' "$T/alpha26.bin"
check "128-bit tail not a unit"  "0:4142434445464748494a4b4c4d4e4f50" 0 "$BG" --input-byte=128 --output-byte=128 -A 1 '4142434445464748494a4b4c4d4e4f50' "$T/alpha26.bin"

# the alignment grid stays anchored at the origin after a short match
check "grid stays anchored"      "0:0~
1:0~
2:0~
3:0~" 0 "$BG" --binary --input-byte=6 --align=6 0 "$T/w6.bin"
check "grid with -s"             "1:0~
2:0~
3:0~" 0 "$BG" --binary --input-byte=6 --align=6 -s 1 0 "$T/w6.bin"
check "stride past end of data"  "" 1 "$BG" --input-byte=8 --align=4096 --binary 1111 "$T/w6.bin"

printf '%s passed, %s failed\n' "$pass" "$fail"
[ "$fail" = 0 ]
