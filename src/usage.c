/* usage.c - short usage message for bad invocations. */
#include <stdlib.h>

#include "bgrep.h"

void display_usage(void)
{
	fprintf(stderr, "Usage: %s [OPTION]... PATTERN [FILE]...\n", progname);
	fprintf(stderr, "Try '%s --help' for more information.\n",   progname);
	exit(BG_ERROR);
}
