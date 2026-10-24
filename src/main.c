/*
 * bgrep - grep for binary data.
 *
 * Copyright (C) 2015-2026 Matthew Haas.  GPLv3+; see the LICENSE file.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bgrep.h"

/*
 * Digits needed to print any offset of a regular file in the output base
 * (offsets count input bytes, or bits with --bit-offset);
 * for pipes and other unsized input assume offsets below 4 GiB.
 */
static int auto_pad_width(const Config *cfg, FILE *fp)
{
	struct stat st;
	ulli        last = 0xFFFFFFFFULL;

	if (fstat(fileno(fp), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0)
		last = cfg->bit_offset ? (ulli) st.st_size * 8 - 1
		                       : ((ulli) st.st_size * 8 - 1) / cfg->in_bits;
	if (!cfg->bit_offset && last < cfg->start_offset)
		last = cfg->start_offset;
	return number_width(last, cfg_out_base(cfg, 0));
}

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
		sink.pad_width = cfg.pad > 0 ? cfg.pad : 0;
		sink.count     = 0;
		sink.bits      = NULL;
		sink.bits_cap  = 0;

		if (fp == NULL)
		{
			warn("%s: %s", files[i], strerror(errno));
			failed = 1;
			continue;
		}

		if (cfg.pad < 0)			/* --pad-output: widest offset in this file */
			sink.pad_width = auto_pad_width(&cfg, fp);

		if (scan_file(fp, pats, np, &sink) != 0)
		{
			warn("%s: %s", sink.name, errno ? strerror(errno) : "read error");
			failed = 1;
		}
		if (!is_stdin)
			fclose(fp);
		free(sink.bits);

		if (cfg.list == LIST_MATCHING && sink.count > 0)
			puts(sink.name);
		else if (cfg.list == LIST_NONMATCHING && sink.count == 0)
			puts(sink.name);
		else if (cfg.list == LIST_NONE && cfg.count_only)
		{
			if (sink.show_name)
				printf("%s:", sink.name);
			put_number(&cfg, sink.count, cfg_out_base(&cfg, 1), 0, 0);
			putchar('\n');
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
