#include "bgrep.h"

void display_version(char *program_name)
{
	fprintf(stdout, "%s %s\n", program_name, BGREP_VERSION);
	fprintf(stdout, "Copyright (C) %s %s\n", BGREP_COPYRIGHT, BGREP_AUTHOR);
	fprintf(stdout, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\n");

	fprintf(stdout, "This program is free software: you can redistribute it and/or modify\n");
	fprintf(stdout, "it under the terms of the GNU General Public License as published by\n");
	fprintf(stdout, "the Free Software Foundation, either version 3 of the License, or\n");
	fprintf(stdout, "(at your option) any later version.\n\n");

	fprintf(stdout, "This program is distributed in the hope that it will be useful,\n");
	fprintf(stdout, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	fprintf(stdout, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	fprintf(stdout, "GNU General Public License for more details.\n\n");

	fprintf(stdout, "You should have received a copy of the GNU General Public License\n");
	fprintf(stdout, "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");

	exit(0);
}
