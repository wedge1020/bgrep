/* version.c - --version output. */
#include <stdlib.h>

#include "bgrep.h"

void display_version(void)
{
	printf("%s %s\n", progname, BGREP_VERSION);
	printf("Copyright (C) %s %s\n", BGREP_COPYRIGHT, BGREP_AUTHOR);
	printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n\n");
	printf("This program is free software: you can redistribute it and/or modify\n");
	printf("it under the terms of the GNU General Public License as published by\n");
	printf("the Free Software Foundation, either version 3 of the License, or\n");
	printf("(at your option) any later version.\n\n");
	printf("This program is distributed in the hope that it will be useful,\n");
	printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	printf("GNU General Public License for more details.\n");
	exit(BG_MATCH);
}
