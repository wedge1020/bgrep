/* help.c - --help output.  Only options that actually work are listed. */
#include <stdlib.h>

#include "bgrep.h"

void display_help(void)
{
	printf("Usage: %s [OPTION]... PATTERN [FILE]...\n", progname);
	printf("Search for byte PATTERN in each FILE or standard input.\n");
	printf("PATTERN is a string of hex digits describing the bytes to find, in which\n");
	printf("'.' matches any single digit; whitespace is ignored.  See -E and -G for\n");
	printf("regexes, --binary for bit patterns and --input-byte for non-8-bit bytes.\n");
	printf("Example: %s '68 65 .. .. 6f 00' data1.file data2.file\n\n", progname);

	printf("Pattern selection:\n");
	printf("  -F, --fixed                  PATTERN is hex digits and '.' (default)\n");
	printf("  -G, --basic-regexp           PATTERN is a POSIX basic regex over hex text\n");
	printf("  -E, --extended-regexp        PATTERN is a POSIX extended regex over hex text\n");
	printf("  -e, --regexp=PATTERN         use PATTERN (may be given more than once)\n");
	printf("  -f, --file=FILE              read patterns from FILE, one per line\n");
	printf("      --binary                 PATTERN digits are 0, 1 and '.' per bit\n");
	printf("  -s, --starting-offset=N      begin searching at input byte N\n\n");

	printf("Bitstream model:\n");
	printf("      --input-byte=BITS        treat input as BITS-bit bytes (1-128, default 8)\n");
	printf("      --output-byte=BITS       regroup output into BITS-bit bytes (default 8)\n");
	printf("      --align=BITS             matches may start every BITS bits (default: input byte)\n");
	printf("      --half-byte              same as --align=4: match on nibble boundaries\n");
	printf("      --lsb-first              bit 0 of each file byte comes first (default: msb)\n");
	printf("      --msb-first              the default\n");
	printf("      --bit-offset             report offsets in bits\n\n");

	printf("Output control:\n");
	printf("  -c, --count                  print only a count of matches per file\n");
	printf("  -l, --files-with-matches     print only names of files with a match\n");
	printf("  -L, --files-without-match    print only names of files without a match\n");
	printf("  -m, --max-count=NUM          stop each file after NUM matches\n");
	printf("  -q, --quiet                  no output; exit 0 at the first match\n");
	printf("  -b, --byte-offset            show hex offset of each match (default)\n");
	printf("      --no-byte-offset         do not show the offset\n");
	printf("  -H, --with-filename          prefix matches with the file name\n");
	printf("  -h, --no-filename            do not prefix matches with the file name\n");
	printf("      --label=LABEL            name to show for standard input\n\n");

	printf("Offset reporting and number formats:\n");
	printf("      --first-byte[-with-prefix]   report offset of first match/context byte\n");
	printf("      --last-byte[-with-suffix]    report offset of last match/context byte\n");
	printf("      --auto-mode              numbers: 0x/0 prefixes honoured (default)\n");
	printf("      --hex-mode, --oct-mode, --dec-mode   read and print numbers in that base\n");
	printf("      --[no-]hex-prefix        print offsets with a leading 0x\n");
	printf("      --[no-]hex-suffix        print offsets with a trailing h\n");
	printf("  -p[N], --pad-output[=N]      zero-pad offsets (to N digits, or to file size)\n");
	printf("      --no-pad-output          do not pad offsets (default)\n\n");

	printf("Context and display:\n");
	printf("  -B, --before-bytes=N         also show N bytes before each match\n");
	printf("  -A, --after-bytes=N          also show N bytes after each match\n");
	printf("  -P, --prefix-num-bytes=K     show as many bytes before as match byte K says\n");
	printf("  -S, --suffix-num-bytes=K     show as many bytes after as match byte K says\n");
	printf("  -O, --off-by=[+|-]N          adjust the -P/-S counts by N\n");
	printf("      --ascii                  append an ASCII rendering\n");
	printf("  -g, --group-size=N           show bytes in groups of N\n");
	printf("      --little-endian-group    show each group last byte first\n");
	printf("      --big-endian-group       show each group in stream order (default)\n");
	printf("      --byte-reverse-group     reverse the byte order of each group\n");
	printf("      --bit-reverse-group      reverse every bit of each group\n");
	printf("  -I, --invert-byte=K[,K...]   bit-invert match byte(s) K when printing\n\n");

	printf("Miscellaneous:\n");
	printf("      --help                   display this help and exit\n");
	printf("  -V, --version                display version information and exit\n\n");

	printf("Exit status: 0 if a match was found, 1 if not, 2 on error.\n");
	printf("See bgrep(1) for details.  Report bugs to: %s\n", BGREP_EMAIL);
	exit(BG_MATCH);
}
