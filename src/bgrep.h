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
#define BGREP_VERSION    "0.3"

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

/* Run-time settings, filled in by parse_args(). */
typedef struct
{
	Mode          mode;
	ListMode      list;
	int           count_only;		/* -c */
	int           quiet;			/* -q */
	int           show_offset;		/* -b / --no-byte-offset */
	int           with_filename;	/* -H / -h; -1 means "decide by file count" */
	ulli          max_count;		/* -m; 0 means unbounded */
	const char   *label;			/* name shown for standard input */
} Config;

/* A growable list of strings (raw PATTERN text). */
typedef struct
{
	char        **item;
	size_t        count;
	size_t        cap;
} StrList;

/*
 * A compiled pattern.  In MODE_NIBBLE, byte i of the input matches when
 * (byte & mask[i]) == value[i]; mask nibbles are 0xF for a literal hex
 * digit and 0x0 for a '.' wildcard.  In the regex modes, 're' is compiled.
 */
typedef struct
{
	size_t        len;				/* bytes matched (nibble mode only) */
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
	ulli          count;
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
int    emit(Sink *, ulli, const uc *, size_t);
int    scan_file(FILE *, const Pattern *, size_t, Sink *);

/* help.c, usage.c, version.c */
void   display_help(void);
void   display_usage(void);
void   display_version(void);

#endif
