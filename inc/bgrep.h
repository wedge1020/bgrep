/*
 * bgrep.h - shared types and prototypes for bgrep, the binary grep.
 *
 * Copyright (C) 2015-2026 Matthew Haas.  GPLv3+; see the LICENSE file.
 */
#ifndef BGREP_H
#define BGREP_H

#include <regex.h>
#include <stddef.h>
#include <stdio.h>

#define BGREP_AUTHOR     "Matthew Haas"
#define BGREP_COPYRIGHT  "2015-2026"
#define BGREP_EMAIL      "wedge1020+bgrep@gmail.com"
#define BGREP_URL        "https://github.com/wedge1020/bgrep"
#define BGREP_VERSION    "0.4"

/* Exit statuses, as documented in bgrep(1). */
#define BG_MATCH         0		/* something matched */
#define BG_NOMATCH       1		/* nothing matched */
#define BG_ERROR         2		/* an error occurred */

typedef unsigned char          uc;
typedef unsigned long long int ulli;

/* How PATTERNs are interpreted. */
typedef enum
{
	MODE_NIBBLE,		/* hex digits and '.' wildcards (default, streamed) */
	MODE_BRE,			/* POSIX basic regex over the hex text of the data */
	MODE_ERE			/* POSIX extended regex over the hex text of the data */
} Mode;

typedef enum
{
	LIST_NONE,			/* normal output */
	LIST_MATCHING,		/* -l: print names of files with a match */
	LIST_NONMATCHING	/* -L: print names of files without a match */
} ListMode;

/* Number base for option arguments and printed numbers. */
typedef enum
{
	BASE_AUTO,			/* input: 0x/0 prefixes honoured; offsets hex, counts decimal */
	BASE_HEX,
	BASE_OCT,
	BASE_DEC
} BaseMode;

/* Which byte's offset is reported for a match. */
typedef enum
{
	OFF_FIRST,			/* first matched byte (default) */
	OFF_FIRST_PREFIX,	/* first byte including leading context */
	OFF_LAST,			/* last matched byte */
	OFF_LAST_SUFFIX		/* last byte including trailing context */
} OffsetStyle;

/* Limits on user-supplied sizes (keeps the streaming window bounded). */
#define CONTEXT_MAX      (1UL << 20)
#define GROUP_MAX        4096

/* Run-time settings, filled in by parse_args(). */
typedef struct
{
	/* matching and file-level output */
	Mode          mode;
	ListMode      list;
	int           count_only;		/* -c */
	int           quiet;			/* -q */
	int           show_offset;		/* -b / --no-byte-offset */
	int           with_filename;	/* -H / -h; -1 means "decide by file count" */
	ulli          max_count;		/* -m; 0 means unbounded */
	const char   *label;			/* name shown for standard input */
	ulli          start_offset;		/* -s: begin reading here */
	int           half_byte;		/* --half-byte: nibble-granular matching */

	/* offset formatting */
	BaseMode      base;
	OffsetStyle   offset_style;
	int           hex_prefix;		/* print "0x" before hex offsets */
	int           hex_suffix;		/* print "h" after hex offsets */
	int           pad;				/* 0 none, -1 auto (file size), >0 digits */

	/* context bytes around each match */
	ulli          before_fixed;		/* -B: bytes before */
	ulli          after_fixed;		/* -A: bytes after */
	unsigned      prefix_idx;		/* -P: 1-based match byte holding a "bytes before" count */
	unsigned      suffix_idx;		/* -S: 1-based match byte holding a "bytes after" count */
	long long     off_by;			/* -O: adjustment to the -P/-S counts */
	size_t        max_before;		/* computed: most leading context ever needed */
	size_t        max_after;		/* computed: most trailing context ever needed */

	/* byte display and transforms */
	int           ascii;			/* --ascii column */
	size_t        group_size;		/* bytes per output group */
	int           little_endian;	/* show groups last byte first */
	int           byte_reverse;		/* additionally reverse byte order in groups */
	int           bit_reverse;		/* reverse all bits in groups */
	unsigned     *invert;			/* -I: 1-based match bytes to bit-invert */
	size_t        ninvert;
} Config;

int    cfg_in_base(const Config *);
int    cfg_out_base(const Config *, int for_count);

/* A growable list of strings (raw PATTERN text). */
typedef struct
{
	char        **item;
	size_t        count;
	size_t        cap;
} StrList;

/*
 * A compiled pattern.  In MODE_NIBBLE, nibble k of the pattern is stored in
 * the high (even k) or low (odd k) half of byte k/2 of value[] and mask[]:
 * input matches when (data & mask) == value; mask nibbles are 0xF for a
 * literal hex digit and 0x0 for a '.' wildcard.  In the regex modes, 're'
 * is compiled instead.
 */
typedef struct
{
	size_t        nibs;				/* nibbles matched (nibble mode only) */
	uc           *value;
	uc           *mask;
	regex_t       re;
} Pattern;

/* Per-file reporting state, shared between the scanners and emit(). */
typedef struct
{
	const Config *cfg;
	const char   *name;
	int           show_name;
	int           pad_width;		/* digits to pad offsets to (0 = none) */
	ulli          count;
	uc           *scratch;			/* work buffer for -I */
	size_t        scratch_cap;
} Sink;

extern const char *progname;

/* util.c */
void   die(const char *, ...);
void   warn(const char *, ...);
void  *xmalloc(size_t);
void  *xrealloc(void *, size_t);
char  *xstrdup(const char *);
void   strlist_add(StrList *, const char *);

/* args.c */
int    parse_args(int, char **, Config *, StrList *);

/* pattern.c */
void   compile_patterns(const Config *, const StrList *, Pattern **, size_t *);

/* scan.c */
int    scan_file(FILE *, const Pattern *, size_t, Sink *);

/* output.c */
int    emit(Sink *, const uc *, size_t, ulli, size_t, size_t);
void   put_number(const Config *, ulli, int, int, int);
int    number_width(ulli, int);

/* help.c, usage.c, version.c */
void   display_help(void);
void   display_usage(void);
void   display_version(void);

#endif
