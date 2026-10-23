/*
 * pattern.c - turn raw PATTERN strings into something the scanners can use.
 *
 * Nibble mode:  "68 65 .. 6f"  ->  packed value/mask pairs.  A '.' is a
 *               wildcard for one hex digit, so "6." matches 0x60-0x6f and
 *               ".." matches any byte.  Whitespace is ignored.  An even
 *               number of digits is required unless --half-byte is given.
 *
 * Regex modes:  the data is rendered as lowercase hex text and the pattern
 *               is a POSIX regex over that text (see scan.c).  Whitespace is
 *               stripped from the pattern first, and matching is
 *               case-insensitive so "FF" and "ff" are equivalent.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bgrep.h"

static int hexval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	return (c | 0x20) - 'a' + 10;		/* caller checked isxdigit() */
}

static void compile_nibble(Pattern *p, const char *text, int half_byte)
{
	size_t  cap = strlen(text) / 2 + 1;
	size_t  nn  = 0;			/* nibbles seen */
	const char *s;

	p->value = xmalloc(cap);
	p->mask  = xmalloc(cap);
	memset(p->value, 0, cap);
	memset(p->mask,  0, cap);

	for (s = text; *s != '\0'; s++)
	{
		int  v = 0, m = 0;
		size_t i = nn / 2;

		if (isspace((unsigned char) *s))
			continue;
		if (isxdigit((unsigned char) *s))
		{
			v = hexval((unsigned char) *s);
			m = 0xF;
		}
		else if (*s == '.')
		{
			v = 0;
			m = 0;
		}
		else
			die("invalid pattern symbol '%c' in pattern '%s' "
			    "(use -E or -G for regular expressions)", *s, text);

		if (nn % 2 == 0)
		{
			p->value[i] = (uc) (v << 4);
			p->mask[i]  = (uc) (m << 4);
		}
		else
		{
			p->value[i] |= (uc) v;
			p->mask[i]  |= (uc) m;
		}
		nn++;
	}

	if (nn == 0)
		die("empty pattern");
	if (nn % 2 != 0 && !half_byte)
		die("malformed pattern '%s': odd number of hex digits "
		    "(see --half-byte)", text);
	p->nibs = nn;
}

static void compile_regex(Pattern *p, const char *text, Mode mode)
{
	char   *clean = xmalloc(strlen(text) + 1);
	char   *d     = clean;
	const char *s;
	int     rc;

	for (s = text; *s != '\0'; s++)
		if (!isspace((unsigned char) *s))
			*d++ = *s;
	*d = '\0';
	if (clean[0] == '\0')
		die("empty pattern");

	rc = regcomp(&p->re, clean,
	             REG_ICASE | (mode == MODE_ERE ? REG_EXTENDED : 0));
	if (rc != 0)
	{
		char msg[256];

		regerror(rc, &p->re, msg, sizeof(msg));
		die("bad regular expression '%s': %s", text, msg);
	}
	free(clean);
	p->nibs = 0;
}

/* Compile every pattern in 'raw'; *out is an array of *n Patterns. */
void compile_patterns(const Config *cfg, const StrList *raw,
                      Pattern **out, size_t *n)
{
	size_t   i;
	Pattern *pats = xmalloc(raw->count * sizeof(*pats));

	for (i = 0; i < raw->count; i++)
	{
		memset(&pats[i], 0, sizeof(pats[i]));
		if (cfg->mode == MODE_NIBBLE)
			compile_nibble(&pats[i], raw->item[i], cfg->half_byte);
		else
			compile_regex(&pats[i], raw->item[i], cfg->mode);
	}
	*out = pats;
	*n   = raw->count;
}
