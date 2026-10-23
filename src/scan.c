/*
 * scan.c - search an input stream for compiled patterns.
 *
 * Positions inside the data are tracked in nibbles (half bytes): nibble 2k
 * is the high half of byte k.  Without --half-byte every match starts and
 * ends on a byte boundary, so positions simply step two nibbles at a time.
 *
 * Two scanners:
 *
 *  scan_nibble()  Streams the input in fixed-size chunks.  The window kept
 *                 in memory always holds enough bytes before the current
 *                 position (for leading context) and, unless at end of
 *                 file, enough bytes after it (for the longest pattern plus
 *                 trailing context), so memory use is bounded.
 *
 *  scan_regex()   Reads the whole input, renders it as lowercase hex text
 *                 (two characters per byte) and runs regexec() over that.
 *                 Unless --half-byte is given, a match must begin and end
 *                 on an even character index; others are rejected and the
 *                 search resumes one character later.  Memory use is about
 *                 three times the input size.
 *
 * Both report non-overlapping matches, leftmost first; if several patterns
 * match at the same position the one given first wins.  Matches are handed
 * to emit() (output.c) together with the window that contains them.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "bgrep.h"

#define CHUNK  65536

static unsigned nibble_of(const uc *buf, size_t n)
{
	return (n & 1) ? buf[n / 2] & 0x0F : buf[n / 2] >> 4;
}

/* Does pattern p match at nibble position q of buf?  (Caller checked bounds.) */
static int match_at(const Pattern *p, const uc *buf, size_t q)
{
	size_t k;

	if ((q & 1) == 0)
	{
		size_t nb = p->nibs / 2;

		for (k = 0; k < nb; k++)
			if ((buf[q / 2 + k] & p->mask[k]) != p->value[k])
				return 0;
		if (p->nibs & 1)		/* a trailing lone nibble: high half only */
			if ((buf[q / 2 + nb] & p->mask[nb] & 0xF0) != (p->value[nb] & 0xF0))
				return 0;
		return 1;
	}

	for (k = 0; k < p->nibs; k++)
		if ((nibble_of(buf, q + k) & nibble_of(p->mask, k)) != nibble_of(p->value, k))
			return 0;
	return 1;
}

static int scan_nibble(FILE *fp, const Pattern *pats, size_t np, Sink *sink,
                       ulli base)
{
	const Config *cfg   = sink->cfg;
	size_t  maxnibs = 0;
	size_t  step    = cfg->half_byte ? 1 : 2;
	size_t  lead    = cfg->max_before;
	size_t  total   = 0;		/* valid bytes in buf */
	size_t  q       = 0;		/* current nibble position in buf */
	size_t  need;				/* nibbles of look-ahead wanted before deciding */
	int     eof     = 0;
	uc     *buf;
	size_t  i;

	for (i = 0; i < np; i++)
		if (pats[i].nibs > maxnibs)
			maxnibs = pats[i].nibs;
	need = maxnibs + 2 * cfg->max_after + 2;
	buf  = xmalloc(lead + CHUNK + need / 2 + 4);

	while (!eof)
	{
		size_t n = fread(buf + total, 1, CHUNK, fp);
		size_t keep;

		total += n;
		if (n < CHUNK)
		{
			if (ferror(fp))
			{
				free(buf);
				return -1;
			}
			eof = 1;
		}

		while (q < total * 2)
		{
			const Pattern *hit = NULL;

			/* More data might still change the outcome here: wait for it. */
			if (!eof && total * 2 - q < need)
				break;

			for (i = 0; i < np && hit == NULL; i++)
				if (q + pats[i].nibs <= total * 2 && match_at(&pats[i], buf, q))
					hit = &pats[i];

			if (hit == NULL)
			{
				q += step;
				continue;
			}
			if (emit(sink, buf, total, base, q, hit->nibs))
			{
				free(buf);
				return 0;
			}
			q += hit->nibs;
		}

		/* Slide the window: keep 'lead' bytes of history before position q. */
		keep = q / 2 > lead ? q / 2 - lead : 0;
		memmove(buf, buf + keep, total - keep);
		total -= keep;
		q     -= 2 * keep;
		base  += keep;
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
                      size_t from, int half, Next *nx)
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
		if (me > ms && (half || (ms % 2 == 0 && me % 2 == 0)))
		{
			nx->state = 1;
			nx->start = ms;
			nx->end   = me;
			return;
		}
		from = ms + 1;		/* misaligned or empty: try again, one later */
	}
}

static int scan_regex(FILE *fp, const Pattern *pats, size_t np, Sink *sink,
                      ulli base)
{
	static const char digits[] = "0123456789abcdef";
	int     half = sink->cfg->half_byte;
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
			find_next(&pats[i], hex, len * 2, pos, half, &nx[i]);
			if (nx[i].state == 1 && (best == np || nx[i].start < nx[best].start))
				best = i;
		}
		if (best == np)
			break;

		if (emit(sink, data, len, base, nx[best].start, nx[best].end - nx[best].start))
			break;
		pos = nx[best].end;
	}

	free(nx);
	free(hex);
	free(data);
	return 0;
}

/* Skip the first n bytes of fp (-s): seek if possible, else read and drop. */
static int skip_input(FILE *fp, ulli n)
{
	uc buf[4096];

	if (n == 0)
		return 0;
	if (n <= (ulli) INT64_MAX && fseeko(fp, (off_t) n, SEEK_SET) == 0)
		return 0;
	errno = 0;
	while (n > 0)
	{
		size_t want = n < sizeof(buf) ? (size_t) n : sizeof(buf);
		size_t got  = fread(buf, 1, want, fp);

		if (got == 0)
			break;
		n -= got;
	}
	return ferror(fp) ? -1 : 0;
}

/* Scan one open file.  Returns 0 on success, -1 on a read error. */
int scan_file(FILE *fp, const Pattern *pats, size_t np, Sink *sink)
{
	ulli start = sink->cfg->start_offset;

	errno = 0;		/* a failed seek on a pipe is expected, not an error */
	if (skip_input(fp, start) != 0)
		return -1;
	if (sink->cfg->mode == MODE_NIBBLE)
		return scan_nibble(fp, pats, np, sink, start);
	return scan_regex(fp, pats, np, sink, start);
}
