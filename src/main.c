/*
 * bgrep - grep for binary data.
 *
 * Copyright (C) 2015-2026 Matthew Haas.  GPLv3+; see the LICENSE file.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgrep.h"

int main(int argc, char **argv)
{
	Config    cfg;
	StrList   raw   = { NULL, 0, 0 };
	Pattern  *pats  = NULL;
	size_t    np    = 0;
	int       first, nfiles, i;
	int       matched = 0;		/* did anything match / get listed? */
	int       failed  = 0;		/* did any file produce an error? */
	char     *slash;
	static char *stdin_only[] = { "-", NULL };
	char    **files;

	if (argc > 0 && argv[0] != NULL && argv[0][0] != '\0')
	{
		slash    = strrchr(argv[0], '/');
		progname = slash != NULL ? slash + 1 : argv[0];
	}

	first = parse_args(argc, argv, &cfg, &raw);
	compile_patterns(&cfg, &raw, &pats, &np);

	files  = argv + first;
	nfiles = argc - first;
	if (nfiles == 0)
	{
		files  = stdin_only;
		nfiles = 1;
	}
	if (cfg.with_filename < 0)
		cfg.with_filename = nfiles > 1;

#ifdef __OpenBSD__
	if (pledge("stdio rpath", NULL) == -1)
		die("pledge: %s", strerror(errno));
#endif

	for (i = 0; i < nfiles; i++)
	{
		int         is_stdin = strcmp(files[i], "-") == 0;
		FILE       *fp       = is_stdin ? stdin : fopen(files[i], "rb");
		Sink        sink;

		sink.cfg       = &cfg;
		sink.name      = is_stdin ? cfg.label : files[i];
		sink.show_name = cfg.with_filename;
		sink.count     = 0;

		if (fp == NULL)
		{
			warn("%s: %s", files[i], strerror(errno));
			failed = 1;
			continue;
		}

		if (scan_file(fp, pats, np, &sink) != 0)
		{
			warn("%s: %s", sink.name, errno ? strerror(errno) : "read error");
			failed = 1;
		}
		if (!is_stdin)
			fclose(fp);

		if (cfg.list == LIST_MATCHING && sink.count > 0)
			puts(sink.name);
		else if (cfg.list == LIST_NONMATCHING && sink.count == 0)
			puts(sink.name);
		else if (cfg.list == LIST_NONE && cfg.count_only)
		{
			if (sink.show_name)
				printf("%s:", sink.name);
			printf("%llu\n", sink.count);
		}

		if (cfg.list == LIST_NONMATCHING ? sink.count == 0 : sink.count > 0)
			matched = 1;
	}

	if (fflush(stdout) != 0 || ferror(stdout))
		die("write error: %s", strerror(errno));

	if (failed)
		return BG_ERROR;
	return matched ? BG_MATCH : BG_NOMATCH;
}
