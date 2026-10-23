/*
 * scan.c - search an input stream for compiled patterns and report matches.
 *
 * Two scanners:
 *
 *  scan_nibble()  Streams the input in fixed-size chunks, carrying over the
 *                 last (longest pattern - 1) bytes between reads so matches
 *                 can straddle chunk boundaries.  Memory use is constant.
 *
 *  scan_regex()   Reads the whole input, renders it as lowercase hex text
 *                 (two characters per byte) and runs regexec() over that.
 *                 Every match must begin and end on a byte boundary (even
 *                 character index); others are rejected and the search
 *                 resumes one character later.  Memory use is ~3x the input.
 *
 * Both report non-overlapping matches, leftmost first; if several patterns
 * match at the same position the one given first wins.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bgrep.h"

#define CHUNK  65536

/*
 * Record a match of 'n' bytes at file offset 'off' and print it unless a
 * summary mode (-c/-l/-L/-q) is active.  Returns nonzero when scanning of
 * this file should stop.
 */
int emit(Sink *s, ulli off, const uc *p, size_t n)
{
	const Config *c = s->cfg;
	size_t        i;

	s->count++;
	if (c->quiet)
		exit(BG_MATCH);			/* first match settles it */

	if (!c->count_only && c->list == LIST_NONE)
	{
		if (s->show_name)
			printf("%s:", s->name);
		if (c->show_offset)
			printf("%llx:", off);
		for (i = 0; i < n; i++)
			printf(i ? " %02x" : "%02x", p[i]);
		putchar('\n');
	}

	if (c->list != LIST_NONE)
		return 1;				/* only need to know there was one */
	return c->max_count != 0 && s->count >= c->max_count;
}

static int match_at(const Pattern *p, const uc *buf)
{
	size_t i;

	for (i = 0; i < p->len; i++)
		if ((buf[i] & p->mask[i]) != p->value[i])
			return 0;
	return 1;
}

static int scan_nibble(FILE *fp, const Pattern *pats, size_t np, Sink *sink)
{
	size_t  maxlen = 0;
	size_t  carry  = 0;
	ulli    base   = 0;			/* file offset of buf[0] */
	int     eof    = 0;
	uc     *buf;
	size_t  i;

	for (i = 0; i < np; i++)
		if (pats[i].len > maxlen)
			maxlen = pats[i].len;
	buf = xmalloc(CHUNK + maxlen);

	while (!eof)
	{
		size_t n     = fread(buf + carry, 1, CHUNK, fp);
		size_t total = carry + n;
		size_t pos   = 0;

		if (n < CHUNK)
		{
			if (ferror(fp))
			{
				free(buf);
				return -1;
			}
			eof = 1;
		}

		while (pos < total)
		{
			const Pattern *hit = NULL;

			/* A pattern could still match if more data follows. */
			if (!eof && total - pos < maxlen)
				break;

			for (i = 0; i < np && hit == NULL; i++)
				if (pos + pats[i].len <= total && match_at(&pats[i], buf + pos))
					hit = &pats[i];

			if (hit == NULL)
			{
				pos++;
				continue;
			}
			if (emit(sink, base + pos, buf + pos, hit->len))
			{
				free(buf);
				return 0;
			}
			pos += hit->len;
		}

		carry = total - pos;
		memmove(buf, buf + pos, carry);
		base += pos;
	}

	free(buf);
	return 0;
}

/* Read all of fp into a malloc'd buffer.  Returns NULL on read error. */
static uc *slurp(FILE *fp, size_t *len)
{
	size_t cap = CHUNK;
	size_t n   = 0;
	uc    *buf = xmalloc(cap);

	for (;;)
	{
		size_t r;

		if (n == cap)
		{
			if (cap > SIZE_MAX / 4)
				die("input too large for regular-expression mode");
			cap *= 2;
			buf  = xrealloc(buf, cap);
		}
		r = fread(buf + n, 1, cap - n, fp);
		if (r == 0)
			break;
		n += r;
	}
	if (ferror(fp))
	{
		free(buf);
		return NULL;
	}
	*len = n;
	return buf;
}

/* Cached "next match at or after the current position" for one pattern. */
typedef struct
{
	int     state;			/* 0 = not computed, 1 = found, 2 = no more */
	size_t  start;			/* hex-text indexes of the match */
	size_t  end;
} Next;

static void find_next(const Pattern *p, const char *hex, size_t nchars,
                      size_t from, Next *nx)
{
	if (nx->state == 2 || (nx->state == 1 && nx->start >= from))
		return;

	for (;;)
	{
		regmatch_t m;
		size_t     ms, me;

		if (from > nchars ||
		    regexec(&p->re, hex + from, 1, &m, from ? REG_NOTBOL : 0) != 0)
		{
			nx->state = 2;
			return;
		}
		ms = from + (size_t) m.rm_so;
		me = from + (size_t) m.rm_eo;
		if (me > ms && ms % 2 == 0 && me % 2 == 0)
		{
			nx->state = 1;
			nx->start = ms;
			nx->end   = me;
			return;
		}
		from = ms + 1;		/* misaligned or empty: try again, one later */
	}
}

static int scan_regex(FILE *fp, const Pattern *pats, size_t np, Sink *sink)
{
	static const char digits[] = "0123456789abcdef";
	size_t  len, i, pos = 0;
	uc     *data = slurp(fp, &len);
	char   *hex;
	Next   *nx;

	if (data == NULL)
		return -1;
	if (len > (SIZE_MAX - 1) / 2)
		die("input too large for regular-expression mode");

	hex = xmalloc(len * 2 + 1);
	for (i = 0; i < len; i++)
	{
		hex[2 * i]     = digits[data[i] >> 4];
		hex[2 * i + 1] = digits[data[i] & 0xF];
	}
	hex[len * 2] = '\0';

	nx = xmalloc(np * sizeof(*nx));
	memset(nx, 0, np * sizeof(*nx));

	while (pos < len * 2)
	{
		size_t best = np;

		for (i = 0; i < np; i++)
		{
			find_next(&pats[i], hex, len * 2, pos, &nx[i]);
			if (nx[i].state == 1 && (best == np || nx[i].start < nx[best].start))
				best = i;
		}
		if (best == np)
			break;

		if (emit(sink, nx[best].start / 2, data + nx[best].start / 2,
		         (nx[best].end - nx[best].start) / 2))
			break;
		pos = nx[best].end;
	}

	free(nx);
	free(hex);
	free(data);
	return 0;
}

/* Scan one open file.  Returns 0 on success, -1 on a read error. */
int scan_file(FILE *fp, const Pattern *pats, size_t np, Sink *sink)
{
	if (sink->cfg->mode == MODE_NIBBLE)
		return scan_nibble(fp, pats, np, sink);
	return scan_regex(fp, pats, np, sink);
}
