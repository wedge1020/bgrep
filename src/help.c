/* help.c - --help output.  Only options that actually work are listed. */
#include <stdlib.h>

#include "bgrep.h"

void display_help(void)
{
	printf("Usage: %s [OPTION]... PATTERN [FILE]...\n", progname);
	printf("Search for byte PATTERN in each FILE or standard input.\n");
	printf("PATTERN is a string of hex digits in which '.' matches any single\n");
	printf("hex digit (nibble); whitespace is ignored.  See -E and -G for regexes.\n");
	printf("Example: %s '68 65 .. .. 6f 00' data1.file data2.file\n\n", progname);

	printf("Pattern selection:\n");
	printf("  -F, --fixed                  PATTERN is hex digits and '.' (default)\n");
	printf("  -G, --basic-regexp           PATTERN is a POSIX basic regex over hex text\n");
	printf("  -E, --extended-regexp        PATTERN is a POSIX extended regex over hex text\n");
	printf("  -e, --regexp=PATTERN         use PATTERN (may be given more than once)\n");
	printf("  -f, --file=FILE              read patterns from FILE, one per line\n\n");

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

	printf("Miscellaneous:\n");
	printf("      --help                   display this help and exit\n");
	printf("  -V, --version                display version information and exit\n\n");

	printf("Exit status: 0 if a match was found, 1 if not, 2 on error.\n");
	printf("See bgrep(1) for details.  Report bugs to: %s\n", BGREP_EMAIL);
	exit(BG_MATCH);
}
