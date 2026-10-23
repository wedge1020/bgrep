/*
 * args.c - command-line parsing.
 *
 * Numeric option arguments are only converted after all options have been
 * seen, so that --hex-mode / --oct-mode / --dec-mode affect them wherever
 * they appear on the command line.
 *
 * --input-byte and --output-byte (non-8-bit bytes) are still recognised but
 * report "not yet implemented".
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
	OPT_FIRST,
	OPT_FIRST_PREFIX,
	OPT_LAST,
	OPT_LAST_SUFFIX,
	OPT_AUTO,
	OPT_HEX,
	OPT_OCT,
	OPT_DEC,
	OPT_HEX_PREFIX,
	OPT_NO_HEX_PREFIX,
	OPT_HEX_SUFFIX,
	OPT_NO_HEX_SUFFIX,
	OPT_NO_PAD,
	OPT_ASCII,
	OPT_NO_ASCII,
	OPT_LITTLE,
	OPT_BIG,
	OPT_BYTE_REVERSE,
	OPT_BIT_REVERSE,
	OPT_HALF_BYTE,
	OPT_UNIMPLEMENTED
};

#define SHORT_OPTS  "Ve:f:cLlm:qbHhFGEp::P:S:I:s:O:A:B:g:"

static const struct option long_options[] =
{
	{ "help",                   no_argument,       NULL, OPT_HELP        },
	{ "version",                no_argument,       NULL, 'V'             },
	{ "regexp",                 required_argument, NULL, 'e'             },
	{ "file",                   required_argument, NULL, 'f'             },
	{ "fixed",                  no_argument,       NULL, 'F'             },
	{ "basic-regexp",           no_argument,       NULL, 'G'             },
	{ "extended-regexp",        no_argument,       NULL, 'E'             },
	{ "half-byte",              no_argument,       NULL, OPT_HALF_BYTE   },
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
	{ "starting-offset",        required_argument, NULL, 's'             },

	{ "first-byte",             no_argument,       NULL, OPT_FIRST       },
	{ "first-byte-with-prefix", no_argument,       NULL, OPT_FIRST_PREFIX },
	{ "last-byte",              no_argument,       NULL, OPT_LAST        },
	{ "last-byte-with-suffix",  no_argument,       NULL, OPT_LAST_SUFFIX },
	{ "auto-mode",              no_argument,       NULL, OPT_AUTO        },
	{ "hex-mode",               no_argument,       NULL, OPT_HEX         },
	{ "oct-mode",               no_argument,       NULL, OPT_OCT         },
	{ "dec-mode",               no_argument,       NULL, OPT_DEC         },
	{ "hex-prefix",             no_argument,       NULL, OPT_HEX_PREFIX  },
	{ "no-hex-prefix",          no_argument,       NULL, OPT_NO_HEX_PREFIX },
	{ "hex-suffix",             no_argument,       NULL, OPT_HEX_SUFFIX  },
	{ "no-hex-suffix",          no_argument,       NULL, OPT_NO_HEX_SUFFIX },
	{ "pad-output",             optional_argument, NULL, 'p'             },
	{ "no-pad-output",          no_argument,       NULL, OPT_NO_PAD      },
	{ "ascii",                  no_argument,       NULL, OPT_ASCII       },
	{ "no-ascii",               no_argument,       NULL, OPT_NO_ASCII    },

	{ "before-bytes",           required_argument, NULL, 'B'             },
	{ "after-bytes",            required_argument, NULL, 'A'             },
	{ "prefix-num-bytes",       required_argument, NULL, 'P'             },
	{ "suffix-num-bytes",       required_argument, NULL, 'S'             },
	{ "off-by",                 required_argument, NULL, 'O'             },

	{ "invert-byte",            required_argument, NULL, 'I'             },
	{ "group-size",             required_argument, NULL, 'g'             },
	{ "little-endian-group",    no_argument,       NULL, OPT_LITTLE      },
	{ "big-endian-group",       no_argument,       NULL, OPT_BIG         },
	{ "byte-reverse-group",     no_argument,       NULL, OPT_BYTE_REVERSE },
	{ "bit-reverse-group",      no_argument,       NULL, OPT_BIT_REVERSE },

	/* documented in bgrep(1), planned, not yet implemented */
	{ "input-byte",             required_argument, NULL, OPT_UNIMPLEMENTED },
	{ "output-byte",            required_argument, NULL, OPT_UNIMPLEMENTED },
	{ NULL,                     0,                 NULL, 0               }
};

/* Parse an unsigned number in the given base (0 = auto: 0x hex, 0 octal). */
static ulli parse_number(const char *s, const char *opt, int base)
{
	char *end = NULL;
	ulli  v;

	errno = 0;
	v     = strtoull(s, &end, base);
	if (end == s || *end != '\0' || errno != 0 || strchr(s, '-') != NULL)
		die("invalid argument '%s' for %s", s, opt);
	return v;
}

/* Like parse_number, bounded by 'max'. */
static ulli parse_bounded(const char *s, const char *opt, int base, ulli max)
{
	ulli v = parse_number(s, opt, base);

	if (v > max)
		die("argument '%s' for %s is too large (maximum %llu)", s, opt, max);
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

/* Parse the -I list: comma-separated 1-based byte positions. */
static void parse_invert_list(const char *arg, Config *cfg, int base)
{
	char *copy = xstrdup(arg);
	char *save = NULL;
	char *tok;

	for (tok = strtok_r(copy, ",", &save); tok != NULL;
	     tok = strtok_r(NULL, ",", &save))
	{
		ulli v = parse_bounded(tok, "--invert-byte", base, 0xFFFF);

		if (v == 0)
			die("--invert-byte positions start at 1");
		cfg->invert = xrealloc(cfg->invert, (cfg->ninvert + 1) * sizeof(*cfg->invert));
		cfg->invert[cfg->ninvert++] = (unsigned) v;
	}
	free(copy);
}

static void conflict(const char *what)
{
	die("--half-byte cannot be combined with %s", what);
}

/*
 * Parse argv into cfg and pats.  Returns the index of the first FILE
 * argument (== argc when none were given).
 */
int parse_args(int argc, char **argv, Config *cfg, StrList *pats)
{
	int         c;
	int         idx      = 0;
	int         base;
	/* raw numeric arguments, converted once the number base is known */
	const char *a_max    = NULL, *a_start = NULL, *a_pad   = NULL;
	const char *a_before = NULL, *a_after = NULL, *a_group = NULL;
	const char *a_pre    = NULL, *a_suf   = NULL, *a_off   = NULL;
	StrList     inv      = { NULL, 0, 0 };
	size_t      i;

	memset(cfg, 0, sizeof(*cfg));
	cfg->mode          = MODE_NIBBLE;
	cfg->list          = LIST_NONE;
	cfg->show_offset   = 1;
	cfg->with_filename = -1;
	cfg->label         = "(stdin)";
	cfg->base          = BASE_AUTO;
	cfg->offset_style  = OFF_FIRST;
	cfg->group_size    = 1;

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
			case OPT_HALF_BYTE: cfg->half_byte     = 1;            break;
			case 'c':           cfg->count_only    = 1;            break;
			case 'L':           cfg->list = LIST_NONMATCHING;      break;
			case 'l':           cfg->list = LIST_MATCHING;         break;
			case 'm':           a_max    = optarg;                 break;
			case 'q':           cfg->quiet         = 1;            break;
			case 'b':           cfg->show_offset   = 1;            break;
			case OPT_NO_OFFSET: cfg->show_offset   = 0;            break;
			case 'H':           cfg->with_filename = 1;            break;
			case 'h':           cfg->with_filename = 0;            break;
			case OPT_LABEL:     cfg->label         = optarg;       break;
			case 's':           a_start  = optarg;                 break;

			case OPT_FIRST:        cfg->offset_style = OFF_FIRST;        break;
			case OPT_FIRST_PREFIX: cfg->offset_style = OFF_FIRST_PREFIX; break;
			case OPT_LAST:         cfg->offset_style = OFF_LAST;         break;
			case OPT_LAST_SUFFIX:  cfg->offset_style = OFF_LAST_SUFFIX;  break;
			case OPT_AUTO:      cfg->base = BASE_AUTO;             break;
			case OPT_HEX:       cfg->base = BASE_HEX;              break;
			case OPT_OCT:       cfg->base = BASE_OCT;              break;
			case OPT_DEC:       cfg->base = BASE_DEC;              break;
			case OPT_HEX_PREFIX:    cfg->hex_prefix = 1;           break;
			case OPT_NO_HEX_PREFIX: cfg->hex_prefix = 0;           break;
			case OPT_HEX_SUFFIX:    cfg->hex_suffix = 1;           break;
			case OPT_NO_HEX_SUFFIX: cfg->hex_suffix = 0;           break;
			case 'p':           cfg->pad = -1; a_pad = optarg;     break;
			case OPT_NO_PAD:    cfg->pad = 0;  a_pad = NULL;       break;
			case OPT_ASCII:     cfg->ascii = 1;                    break;
			case OPT_NO_ASCII:  cfg->ascii = 0;                    break;

			case 'B':           a_before = optarg;                 break;
			case 'A':           a_after  = optarg;                 break;
			case 'P':           a_pre    = optarg;                 break;
			case 'S':           a_suf    = optarg;                 break;
			case 'O':           a_off    = optarg;                 break;

			case 'I':           strlist_add(&inv, optarg);         break;
			case 'g':           a_group  = optarg;                 break;
			case OPT_LITTLE:    cfg->little_endian = 1;            break;
			case OPT_BIG:       cfg->little_endian = 0;            break;
			case OPT_BYTE_REVERSE: cfg->byte_reverse = 1;          break;
			case OPT_BIT_REVERSE:  cfg->bit_reverse  = 1;          break;

			case OPT_UNIMPLEMENTED:
				die("option '--%s' is not yet implemented", long_options[idx].name);
				break;

			default:
				display_usage();
		}
	}

	/* Now that the base is settled, convert the numeric arguments. */
	base = cfg_in_base(cfg);
	if (a_max)    cfg->max_count    = parse_number(a_max,    "--max-count", base);
	if (a_start)  cfg->start_offset = parse_number(a_start,  "--starting-offset", base);
	if (a_pad)    cfg->pad          = (int) parse_bounded(a_pad, "--pad-output", base, 64);
	if (a_before) cfg->before_fixed = parse_bounded(a_before, "--before-bytes", base, CONTEXT_MAX);
	if (a_after)  cfg->after_fixed  = parse_bounded(a_after,  "--after-bytes",  base, CONTEXT_MAX);
	if (a_pre)    cfg->prefix_idx   = (unsigned) parse_bounded(a_pre, "--prefix-num-bytes", base, 0xFFFF);
	if (a_suf)    cfg->suffix_idx   = (unsigned) parse_bounded(a_suf, "--suffix-num-bytes", base, 0xFFFF);
	if (a_group)  cfg->group_size   = (size_t) parse_bounded(a_group, "--group-size", base, GROUP_MAX);
	if (a_off)
	{
		/* "+N" adds, "-N" subtracts, and a bare "N" subtracts (see bgrep(1)). */
		const char *digits = a_off + (a_off[0] == '+' || a_off[0] == '-');
		long long   v      = (long long) parse_bounded(digits, "--off-by", base, 0xFFFF);

		cfg->off_by = a_off[0] == '+' ? v : -v;
	}
	for (i = 0; i < inv.count; i++)
		parse_invert_list(inv.item[i], cfg, base);

	if (cfg->group_size == 0)
		die("--group-size must be at least 1");
	if (a_pre && cfg->prefix_idx == 0)
		die("--prefix-num-bytes positions start at 1");
	if (a_suf && cfg->suffix_idx == 0)
		die("--suffix-num-bytes positions start at 1");

	/* The most context any one match can ask for bounds the scan window. */
	cfg->max_before = (size_t) cfg->before_fixed +
	                  (cfg->prefix_idx ? 255 + (size_t) (cfg->off_by < 0 ? -cfg->off_by : cfg->off_by) : 0);
	cfg->max_after  = (size_t) cfg->after_fixed +
	                  (cfg->suffix_idx ? 255 + (size_t) (cfg->off_by < 0 ? -cfg->off_by : cfg->off_by) : 0);
	if (cfg->max_before > CONTEXT_MAX || cfg->max_after > CONTEXT_MAX)
		die("context size too large (maximum %lu bytes)", CONTEXT_MAX);

	if (cfg->half_byte)
	{
		if (cfg->max_before || cfg->max_after)  conflict("context bytes (-A, -B, -P, -S)");
		if (cfg->ninvert)                        conflict("--invert-byte");
		if (cfg->ascii)                          conflict("--ascii");
		if (cfg->group_size != 1 || cfg->little_endian ||
		    cfg->byte_reverse || cfg->bit_reverse)
			conflict("group options");
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
