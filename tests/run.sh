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
check "unimplemented option"     "" 2 "$BG" --ascii 'ab' "$T/b.bin"
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

printf '%s passed, %s failed\n' "$pass" "$fail"
[ "$fail" = 0 ]
