/*
 * pattern.c - turn raw PATTERN strings into something the scanners can use.
 *
 * Fixed modes.  The input is a bitstream cut into N-bit "bytes" (N is
 * --input-byte, default 8).  A pattern describes whole bytes:
 *
 *   hex (default)  each byte is ceil(N/4) hex digits, written as its numeric
 *                  value, so with N=12 "abc def" is two bytes.  When N is not
 *                  a multiple of 4 the leading digit only holds N mod 4 bits
 *                  and must be small enough to fit (N=6: first digit 0-3).
 *   --binary       each byte is N characters from 0, 1 and '.'.
 *
 *   '.' is a wildcard for the digit (hex) or bit (binary) it replaces.
 *   Whitespace is ignored.  The pattern must be a whole number of bytes
 *   unless --align/--half-byte was given, which also permits a short final
 *   byte (e.g. an odd number of hex digits).
 *
 * The result is the pattern's bits in stream order: for --lsb-first, the
 * bits of each byte are reversed, since the first bit in the stream is the
 * least significant one.
 *
 * Regex modes.  The data is rendered as text (scan.c) and the pattern is a
 * POSIX regex over that text.  Whitespace is stripped from the pattern and
 * matching is case-insensitive, so "FF" and "ff" are equivalent.
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

/* Reverse the bits [from, from+len) of both arrays. */
static void reverse_run(uc *v, uc *m, size_t from, size_t len)
{
	size_t i;

	for (i = 0; i < len / 2; i++)
	{
		uc t;

		t = v[from + i]; v[from + i] = v[from + len - 1 - i]; v[from + len - 1 - i] = t;
		t = m[from + i]; m[from + i] = m[from + len - 1 - i]; m[from + len - 1 - i] = t;
	}
}

static void compile_fixed(Pattern *p, const char *text, const Config *cfg)
{
	unsigned  n     = cfg->in_bits;
	unsigned  dig   = (n + 3) / 4;				/* hex digits per byte */
	unsigned  first = n - 4 * (dig - 1);		/* bits in the leading digit */
	size_t    cap   = strlen(text) * 4 + 1;
	size_t    nb    = 0;						/* bits collected */
	size_t    nd    = 0;						/* digits collected */
	size_t    i;
	const char *s;

	p->bv = xmalloc(cap);
	p->bm = xmalloc(cap);

	for (s = text; *s != '\0'; s++)
	{
		unsigned w;			/* bits this symbol stands for */
		int      v = 0, wild = 0;
		unsigned b;

		if (isspace((unsigned char) *s))
			continue;

		if (cfg->binary)
		{
			w = 1;
			if (*s == '0' || *s == '1')
				v = *s - '0';
			else if (*s == '.')
				wild = 1;
			else
				die("invalid binary pattern symbol '%c' in pattern '%s'", *s, text);
		}
		else
		{
			w = (nd % dig == 0) ? first : 4;
			if (isxdigit((unsigned char) *s))
			{
				v = hexval((unsigned char) *s);
				if (v >> w)
					die("digit '%c' in pattern '%s' is too large: the leading "
					    "digit of a %u-bit byte holds only %u bit%s",
					    *s, text, n, w, w == 1 ? "" : "s");
			}
			else if (*s == '.')
				wild = 1;
			else
				die("invalid pattern symbol '%c' in pattern '%s' "
				    "(use -E or -G for regular expressions)", *s, text);
		}

		for (b = w; b-- > 0; )
		{
			p->bv[nb] = (uc) (wild ? 0 : (v >> b) & 1);
			p->bm[nb] = (uc) (wild ? 0 : 1);
			nb++;
		}
		nd++;
	}

	if (nb == 0)
		die("empty pattern");
	if (nb % n != 0)
	{
		if (!cfg->align_set)
			die("malformed pattern '%s': not a whole number of %u-bit bytes "
			    "(see --half-byte and --align)", text, n);
		if (cfg->lsb_first)
			die("pattern '%s' ends in a partial byte, which --lsb-first "
			    "does not support", text);
		if (!cfg->binary && n % 4 != 0)
			die("pattern '%s' ends in a partial byte; with a %u-bit byte "
			    "use --binary for that", text, n);
	}

	/* Stream order: with --lsb-first each byte's first bit is its lowest. */
	if (cfg->lsb_first)
		for (i = 0; i + n <= nb; i += n)
			reverse_run(p->bv, p->bm, i, n);

	p->nbits  = nb;
	p->nbytes = (nb + 7) / 8;
	p->value  = xmalloc(p->nbytes);
	p->mask   = xmalloc(p->nbytes);
	memset(p->value, 0, p->nbytes);
	memset(p->mask,  0, p->nbytes);
	for (i = 0; i < nb; i++)
		if (p->bm[i])
		{
			uc bit = (uc) (1u << (cfg->lsb_first ? i % 8 : 7 - i % 8));

			p->mask[i / 8] |= bit;
			if (p->bv[i])
				p->value[i / 8] |= bit;
		}
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
			compile_fixed(&pats[i], raw->item[i], cfg);
		else
			compile_regex(&pats[i], raw->item[i], cfg->mode);
	}
	*out = pats;
	*n   = raw->count;
}
