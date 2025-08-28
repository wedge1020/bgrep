#include "bgrep.h"

void display_usage(char *program_name)
{
	fprintf(stderr, "Usage: %s [OPTION]... PATTERN [FILE]...\n", program_name);
	fprintf(stderr, "Try '%s --help' for more information.\n",   program_name);
	exit   (1);
}
