/*
 * args.c - command-line parsing.
 *
 * Options that the manual page describes but that are not implemented yet
 * (offset formatting, context bytes, byte transforms) are still recognised,
 * so that they fail with a clear message instead of "unrecognized option".
 */
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "bgrep.h"

enum
{
	OPT_HELP = 256,
	OPT_NO_OFFSET,
	OPT_LABEL,
	OPT_UNIMPLEMENTED
};

#define SHORT_OPTS  "Ve:f:cLlm:qbHhFGEp:P:S:I:s:"

static const struct option long_options[] =
{
	{ "help",                   no_argument,       NULL, OPT_HELP        },
	{ "version",                no_argument,       NULL, 'V'             },
	{ "regexp",                 required_argument, NULL, 'e'             },
	{ "file",                   required_argument, NULL, 'f'             },
	{ "fixed",                  no_argument,       NULL, 'F'             },
	{ "basic-regexp",           no_argument,       NULL, 'G'             },
	{ "extended-regexp",        no_argument,       NULL, 'E'             },
	{ "count",                  no_argument,       NULL, 'c'             },
	{ "files-without-match",    no_argument,       NULL, 'L'             },
	{ "files-with-matches",     no_argument,       NULL, 'l'             },
	{ "max-count",              required_argument, NULL, 'm'             },
	{ "quiet",                  no_argument,       NULL, 'q'             },
	{ "silent",                 no_argument,       NULL, 'q'             },
	{ "byte-offset",            no_argument,       NULL, 'b'             },
	{ "no-byte-offset",         no_argument,       NULL, OPT_NO_OFFSET   },
	{ "with-filename",          no_argument,       NULL, 'H'             },
	{ "no-filename",            no_argument,       NULL, 'h'             },
	{ "label",                  required_argument, NULL, OPT_LABEL       },

	/* documented in bgrep(1), planned, not yet implemented */
	{ "first-byte",             no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "first-byte-with-prefix", no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "last-byte",              no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "last-byte-with-suffix",  no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "auto-mode",              no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "hex-mode",               no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "oct-mode",               no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "hex-prefix",             no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "no-hex-prefix",          no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "hex-suffix",             no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "no-hex-suffix",          no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "pad-output",             optional_argument, NULL, OPT_UNIMPLEMENTED },
	{ "no-pad-output",          no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "ascii",                  no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "no-ascii",               no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "prefix-num-bytes",       required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "suffix-num-bytes",       required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "input-byte",             required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "output-byte",            required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "little-endian-group",    no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "big-endian-group",       no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "byte-reverse-group",     no_argument,       NULL, OPT_UNIMPLEMENTED },
	{ "invert-byte",            required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "starting-offset",        required_argument, NULL, OPT_UNIMPLEMENTED },
	{ NULL,                     0,                 NULL, 0               }
};

/* Parse an unsigned number; accepts decimal, 0x hex and 0 octal. */
static ulli parse_number(const char *s, const char *opt)
{
	char *end = NULL;
	ulli  v;

	errno = 0;
	v     = strtoull(s, &end, 0);
	if (end == s || *end != '\0' || errno != 0 || strchr(s, '-') != NULL)
		die("invalid argument '%s' for %s", s, opt);
	return v;
}

/* Read patterns, one per line, from the named file ("-" is stdin). */
static void load_pattern_file(const char *name, StrList *pats)
{
	FILE   *fp   = strcmp(name, "-") == 0 ? stdin : fopen(name, "r");
	char   *line = NULL;
	size_t  cap  = 0;
	ssize_t n;

	if (fp == NULL)
		die("%s: %s", name, strerror(errno));
	while ((n = getline(&line, &cap, fp)) != -1)
	{
		if (n > 0 && line[n - 1] == '\n')
			line[--n] = '\0';
		if (n > 0 && line[n - 1] == '\r')	/* tolerate CRLF files */
			line[--n] = '\0';
		if (n > 0)							/* blank lines are skipped */
			strlist_add(pats, line);
	}
	if (ferror(fp))
		die("%s: read error", name);
	free(line);
	if (fp != stdin)
		fclose(fp);
}

/*
 * Parse argv into cfg and pats.  Returns the index of the first FILE
 * argument (== argc when none were given).
 */
int parse_args(int argc, char **argv, Config *cfg, StrList *pats)
{
	int  c;
	int  idx = 0;

	cfg->mode          = MODE_NIBBLE;
	cfg->list          = LIST_NONE;
	cfg->count_only    = 0;
	cfg->quiet         = 0;
	cfg->show_offset   = 1;
	cfg->with_filename = -1;
	cfg->max_count     = 0;
	cfg->label         = "(stdin)";

	while ((c = getopt_long(argc, argv, SHORT_OPTS, long_options, &idx)) != -1)
	{
		switch (c)
		{
			case OPT_HELP:      display_help();                    break;
			case 'V':           display_version();                 break;
			case 'e':           strlist_add(pats, optarg);         break;
			case 'f':           load_pattern_file(optarg, pats);   break;
			case 'F':           cfg->mode = MODE_NIBBLE;           break;
			case 'G':           cfg->mode = MODE_BRE;              break;
			case 'E':           cfg->mode = MODE_ERE;              break;
			case 'c':           cfg->count_only    = 1;            break;
			case 'L':           cfg->list = LIST_NONMATCHING;      break;
			case 'l':           cfg->list = LIST_MATCHING;         break;
			case 'm':           cfg->max_count = parse_number(optarg, "--max-count"); break;
			case 'q':           cfg->quiet         = 1;            break;
			case 'b':           cfg->show_offset   = 1;            break;
			case OPT_NO_OFFSET: cfg->show_offset   = 0;            break;
			case 'H':           cfg->with_filename = 1;            break;
			case 'h':           cfg->with_filename = 0;            break;
			case OPT_LABEL:     cfg->label         = optarg;       break;

			case 'p': case 'P': case 'S': case 'I': case 's':
				die("option '-%c' is not yet implemented", c);
				break;
			case OPT_UNIMPLEMENTED:
				die("option '--%s' is not yet implemented", long_options[idx].name);
				break;

			default:
				display_usage();
		}
	}

	/* Without -e/-f, the first non-option argument is the PATTERN. */
	if (pats->count == 0)
	{
		if (optind >= argc)
			display_usage();
		strlist_add(pats, argv[optind++]);
	}

	return optind;
}
