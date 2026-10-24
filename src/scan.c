/*
 * scan.c - search an input stream for compiled patterns.
 *
 * The input is a bitstream.  Positions here are bit positions; the stream is
 * cut into --input-byte (N) bit "bytes" starting at the --starting-offset
 * (the grid origin), and candidate matches start every --align bits from
 * there (by default every N bits, i.e. on byte boundaries of the grid).
 *
 * Two scanners:
 *
 *  scan_fixed()   Streams the input in fixed-size chunks.  The window kept
 *                 in memory always holds enough bytes before the current
 *                 position (for leading context) and, unless at end of
 *                 file, enough after it (for the longest pattern plus
 *                 trailing context), so memory use is bounded.  Patterns are
 *                 compared bytewise when a candidate starts on a file-byte
 *                 boundary and bit by bit otherwise.
 *
 *  scan_regex()   Reads the whole input and renders each N-bit byte as text
 *                 (ceil(N/4) hex digits, or N binary digits with --binary),
 *                 then runs regexec() over that.  Memory use is a few times
 *                 the input size (about 8x with --binary).
 *
 * Both report non-overlapping matches, leftmost first; if several patterns
 * match at the same position the one given first wins.  After a match the
 * search resumes at the first alignment-grid position after its end.  Matches are handed to emit()
 * (output.c) together with the window that contains them.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "bgrep.h"

#define CHUNK  65536

/* Does pattern p match at bit position q of buf?  (Caller checked bounds.) */
static int match_at(const Pattern *p, const uc *buf, size_t q, int lsb)
{
	size_t i;

	if ((q & 7) == 0)
	{
		const uc *b = buf + (q >> 3);

		for (i = 0; i < p->nbytes; i++)
			if ((b[i] & p->mask[i]) != p->value[i])
				return 0;
		return 1;
	}

	for (i = 0; i < p->nbits; i++)
		if (p->bm[i] && stream_bit(buf, q + i, lsb) != p->bv[i])
			return 0;
	return 1;
}

/* Bytes of history needed to print 'units' units of leading context. */
static size_t bytes_for_units(size_t units, unsigned n)
{
	return (units * n + 7) / 8 + 2;
}

static int scan_fixed(FILE *fp, const Pattern *pats, size_t np, Sink *sink,
                      ulli base)
{
	const Config *cfg   = sink->cfg;
	size_t  maxbits = 0;
	size_t  step    = cfg->align;
	size_t  lead    = bytes_for_units(cfg->max_before, cfg->in_bits);
	size_t  total   = 0;		/* valid bytes in buf */
	size_t  q       = (size_t) (cfg->start_bits % 8);	/* bit position in buf */
	size_t  need;				/* bits of look-ahead wanted before deciding */
	int     eof     = 0;
	uc     *buf;
	size_t  i;

	for (i = 0; i < np; i++)
		if (pats[i].nbits > maxbits)
			maxbits = pats[i].nbits;
	need = maxbits + cfg->max_after * cfg->in_bits + 16;
	buf  = xmalloc(lead + CHUNK + need / 8 + 8);

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

		while (q < total * 8)
		{
			const Pattern *hit = NULL;

			/* More data might still change the outcome here: wait for it. */
			if (!eof && total * 8 - q < need)
				break;

			for (i = 0; i < np && hit == NULL; i++)
				if (q + pats[i].nbits <= total * 8 &&
				    match_at(&pats[i], buf, q, cfg->lsb_first))
					hit = &pats[i];

			if (hit == NULL)
			{
				q += step;
				continue;
			}
			if (emit(sink, buf, total, base, q, hit->nbits))
			{
				free(buf);
				return 0;
			}
			q += hit->nbits;

			/* Resume at the next point of the alignment grid, which is
			 * anchored at the starting offset, not at the match. */
			{
				ulli rem = (base * 8 + q - cfg->start_bits) % step;

				if (rem != 0)
					q += step - (size_t) rem;
			}
		}

		/* Slide the window: keep 'lead' bytes of history before position q. */
		keep = q / 8 > lead ? q / 8 - lead : 0;
		if (keep > total)		/* q ran past the data: it names future input */
			keep = total;
		memmove(buf, buf + keep, total - keep);
		total -= keep;
		q     -= 8 * keep;
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
			if (cap > SIZE_MAX / 16)
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

/* Geometry of the text rendering used by the regex scanner. */
typedef struct
{
	unsigned  n;			/* bits per input byte */
	unsigned  per;			/* text characters per input byte */
	unsigned  first;		/* bits shown by the first hex digit of a byte */
	int       binary;
	size_t    nunits;		/* input bytes rendered */
	size_t    nchars;
} Text;

/* Bit position (from the grid origin) at which text character 'ch' starts. */
static size_t char_bit(const Text *t, size_t ch)
{
	size_t   u = ch / t->per;
	unsigned d = (unsigned) (ch % t->per);

	if (t->binary)
		return u * t->n + d;
	return u * t->n + (d == 0 ? 0 : t->first + 4 * (d - 1));
}

/* Cached "next match at or after the current position" for one pattern. */
typedef struct
{
	int     state;			/* 0 = not computed, 1 = found, 2 = no more */
	size_t  start;			/* text indexes of the match */
	size_t  end;
} Next;

static void find_next(const Pattern *p, const char *text, const Text *t,
                      const Config *cfg, size_t from, Next *nx)
{
	if (nx->state == 2 || (nx->state == 1 && nx->start >= from))
		return;

	for (;;)
	{
		regmatch_t m;
		size_t     ms, me;
		int        ok;

		if (from > t->nchars ||
		    regexec(&p->re, text + from, 1, &m, from ? REG_NOTBOL : 0) != 0)
		{
			nx->state = 2;
			return;
		}
		ms = from + (size_t) m.rm_so;
		me = from + (size_t) m.rm_eo;

		if (me == ms)
			ok = 0;
		else if (cfg->align == cfg->in_bits)	/* both ends on a byte boundary */
			ok = ms % t->per == 0 && me % t->per == 0;
		else									/* start on the alignment grid */
			ok = char_bit(t, ms) % cfg->align == 0;

		if (ok)
		{
			nx->state = 1;
			nx->start = ms;
			nx->end   = me;
			return;
		}
		from = ms + 1;		/* unacceptable: try again, one character later */
	}
}

static int scan_regex(FILE *fp, const Pattern *pats, size_t np, Sink *sink,
                      ulli base)
{
	static const char digits[] = "0123456789abcdef";
	const Config *cfg  = sink->cfg;
	unsigned  n        = cfg->in_bits;
	size_t    skew     = (size_t) (cfg->start_bits % 8);
	size_t    len, i, u, pos = 0;
	uc       *data     = slurp(fp, &len);
	char     *text;
	Next     *nx;
	Text      t;

	if (data == NULL)
		return -1;
	if (len > (SIZE_MAX / 2) / 16)
		die("input too large for regular-expression mode");

	t.n      = n;
	t.binary = cfg->binary;
	t.per    = cfg->binary ? n : (n + 3) / 4;
	t.first  = n - 4 * ((n + 3) / 4 - 1);
	t.nunits = len * 8 > skew ? (len * 8 - skew) / n : 0;
	t.nchars = t.nunits * t.per;
	text     = xmalloc(t.nchars + 1);

	for (u = 0; u < t.nunits; u++)
	{
		uc       vb[BITS_MAX];		/* the byte's bits, most significant first */
		unsigned k, d, at = 0;
		char    *out = text + u * t.per;

		for (k = 0; k < n; k++)
			vb[cfg->lsb_first ? n - 1 - k : k] =
				(uc) stream_bit(data, skew + u * n + k, cfg->lsb_first);

		if (cfg->binary)
		{
			for (k = 0; k < n; k++)
				out[k] = (char) ('0' + vb[k]);
			continue;
		}
		for (d = 0; d < t.per; d++)
		{
			unsigned w = d == 0 ? t.first : 4, v = 0;

			for (k = 0; k < w; k++)
				v = (v << 1) | vb[at++];
			out[d] = digits[v];
		}
	}
	text[t.nchars] = '\0';

	nx = xmalloc(np * sizeof(*nx));
	memset(nx, 0, np * sizeof(*nx));

	while (pos < t.nchars)
	{
		size_t best = np;

		for (i = 0; i < np; i++)
		{
			find_next(&pats[i], text, &t, cfg, pos, &nx[i]);
			if (nx[i].state == 1 && (best == np || nx[i].start < nx[best].start))
				best = i;
		}
		if (best == np)
			break;

		{
			size_t sb = char_bit(&t, nx[best].start);
			size_t eb = char_bit(&t, nx[best].end);

			if (emit(sink, data, len, base, skew + sb, eb - sb))
				break;
		}
		pos = nx[best].end;
	}

	free(nx);
	free(text);
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
	ulli start = sink->cfg->start_bits / 8;		/* whole file bytes to skip */

	errno = 0;		/* a failed seek on a pipe is expected, not an error */
	if (skip_input(fp, start) != 0)
		return -1;
	if (sink->cfg->mode == MODE_NIBBLE)
		return scan_fixed(fp, pats, np, sink, start);
	return scan_regex(fp, pats, np, sink, start);
}
