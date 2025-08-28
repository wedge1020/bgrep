#include "bgrep.h"

void display_help(char *program_name)
{
	fprintf(stdout, "Usage: %s [OPTION]... PATTERN [FILE]...\n", program_name);
	fprintf(stdout, "Search for byte PATTERN in each FILE or standard input.\n");
	fprintf(stdout, "PATTERN is, by default, a basic regular expression (BRE).\n");
	fprintf(stdout, "Example: %s '68 65 .. .. 6f 00' data1.file data2.file\n\n", program_name);

	fprintf(stdout, "Options are as follows:\n");
	fprintf(stdout, "      --help                   this help information\n");
	fprintf(stdout, "  -V, --version                current version being run\n");
	fprintf(stdout, "  -e, --regexp                 specify REGEX PATTERN to search for\n");
	fprintf(stdout, "  -f, --file                   FILE to search for PATTERN\n");
	fprintf(stdout, "  -c, --count                  display only number of matches to STDOUT\n");
	fprintf(stdout, "  -L, --files-without-match    display filenames where no matches occurred\n");
	fprintf(stdout, "  -l, --files-with-matches     display filenames where matches occurred\n");
	fprintf(stdout, "  -m, --max-count=NUM          display the first NUM matches\n");
	fprintf(stdout, "  -q, --quiet                  disable output, exit with 0 if match found\n");
	fprintf(stdout, "  -b, --byte-offset            show hex offset of matched pattern instance\n");
	fprintf(stdout, "      --no-byte-offset         do not show hex offset of matched instance\n");
	fprintf(stdout, "  -H, --with-filename          show filename of each match with match data\n");
	fprintf(stdout, "  -h, --no-filename            do not show filename of each match\n");
	fprintf(stdout, "      --label                  set label to show instead of '(stdin)'\n");


	fprintf(stdout, "\nReport bugs to: %s\n", BGREP_AUTHOR);
	fprintf(stdout, "%s URL: %s\n", program_name, BGREP_URL);
/*
		{ "first-byte",             ARG_NO,    NULL, 4   },
		{ "first-byte-with-prefix", ARG_NO,    NULL, 5   },
		{ "last-byte",              ARG_NO,    NULL, 6   },
		{ "last-byte-with-suffix",  ARG_NO,    NULL, 7   },
		{ "auto-mode",              ARG_NO,    NULL, 8   },
		{ "hex-mode",               ARG_NO,    NULL, 9   },
		{ "oct-mode",               ARG_NO,    NULL, 10   },
		{ "hex-prefix",             ARG_NO,    NULL, 11  },
		{ "no-hex-prefix",          ARG_NO,    NULL, 12  },
		{ "hex-suffix",             ARG_NO,    NULL, 13  },
		{ "no-hex-suffix",          ARG_NO,    NULL, 14  },
		{ "pad-output",             ARG_YES,   NULL, 'p' },
		{ "no-pad-output",          ARG_NO,    NULL, 15  },
		{ "ascii",                  ARG_NO,    NULL, 16  },
		{ "no-ascii",               ARG_NO,    NULL, 17  },
		{ "prefix-num-bytes",       ARG_YES,   NULL, 'P' },
		{ "suffix-num-bytes",       ARG_YES,   NULL, 'S' },
		{ "input-byte",             ARG_YES,   NULL, 18  },
		{ "output-byte",            ARG_YES,   NULL, 19  },
		{ "little-endian-group",    ARG_NO,    NULL, 20  },
		{ "big-endian-group",       ARG_NO,    NULL, 21  },
		{ "byte-reverse-group",     ARG_NO,    NULL, 22  },
//		{ "bit-reverse-group",      ARG_NO,    NULL, 23  },
		{ "invert-byte",            ARG_YES,   NULL, 'I' },
		{ "starting-offset",        ARG_NO,    NULL, 's' },
*/
	exit(0);
}
