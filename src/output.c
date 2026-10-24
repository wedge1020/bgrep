/*
 * output.c - turn a match into output.
 *
 * emit() is called by the scanners with a "window" of input that contains
 * the match plus whatever context is available around it, so that leading
 * and trailing context (-A/-B/-P/-S) can be printed without the scanners
 * knowing anything about output formats.
 *
 * Everything is handled as a stream of bits.  The matched bits (and each
 * block of context) are optionally modified (-I), regrouped into
 * --output-byte sized units, collected into -g groups whose order or bits
 * may be reversed, and printed as hex.  Offsets are reported in input
 * "bytes" (--input-byte bits each) from the starting offset, with any
 * remainder in bits written "+BITS"; --bit-offset reports bits instead.
 *
 * Output line:
 *   [FILE:][OFFSET:][BEFORE | ]BYTES[ | AFTER][  |ascii|]
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bgrep.h"

/* Base used to read numeric option arguments (0 lets strtoull decide). */
int cfg_in_base(const Config *c)
{
	switch (c->base)
	{
		case BASE_HEX: return 16;
		case BASE_OCT: return 8;
		case BASE_DEC: return 10;
		default:       return 0;
	}
}

/* Base used to print offsets, or (for_count) match counts. */
int cfg_out_base(const Config *c, int for_count)
{
	switch (c->base)
	{
		case BASE_OCT: return 8;
		case BASE_DEC: return 10;
		case BASE_HEX: return 16;
		default:       return for_count ? 10 : 16;
	}
}

/* Number of digits needed to print v in the given base. */
int number_width(ulli v, int base)
{
	int n = 1;

	while (v >= (ulli) base)
	{
		v /= base;
		n++;
	}
	return n;
}

/* Print v in 'base', zero-padded to 'width' digits; decorate adds 0x / h. */
void put_number(const Config *c, ulli v, int base, int width, int decorate)
{
	int hex = decorate && base == 16;

	if (hex && c->hex_prefix)
		fputs("0x", stdout);
	switch (base)
	{
		case 16: printf("%0*llx", width, v); break;
		case 8:  printf("%0*llo", width, v); break;
		default: printf("%0*llu", width, v); break;
	}
	if (hex && c->hex_suffix)
		fputc('h', stdout);
}

/* A value read from the stream, saturating instead of overflowing. */
static ulli unit_value(const uc *win, ulli pos, unsigned nbits, int lsb)
{
	ulli     v = 0;
	unsigned i;

	for (i = 0; i < nbits; i++)
	{
		int bit = stream_bit(win, pos + i, lsb);

		if (lsb)
		{
			if (bit && i >= 64)
				return ~0ULL;
			v |= (ulli) bit << (i < 64 ? i : 0);
		}
		else
		{
			if (v >> 63)
				return ~0ULL;
			v = (v << 1) | (ulli) bit;
		}
	}
	return v;
}

/* Context count taken from match byte 'idx' (1-based), adjusted by -O. */
static ulli dynamic_context(const Config *c, unsigned idx, const uc *win,
                            size_t q, size_t nbits)
{
	ulli v;

	if (idx == 0 || (ulli) idx * c->in_bits > nbits)
		return 0;
	v = unit_value(win, q + (ulli) (idx - 1) * c->in_bits, c->in_bits, c->lsb_first);
	if (c->off_by < 0)
		return v < (ulli) -c->off_by ? 0 : v - (ulli) -c->off_by;
	return v > ~0ULL - (ulli) c->off_by ? ~0ULL : v + (ulli) c->off_by;
}

/* Print one output byte of 'len' stream-order bits (len <= --output-byte). */
static void put_unit(const Config *c, const uc *b, unsigned len)
{
	uc       disp[BITS_MAX + 4];		/* bits, most significant first */
	unsigned i, ndig, pad;
	int      tilde = 0;

	for (i = 0; i < len; i++)
		disp[i] = c->lsb_first ? b[len - 1 - i] : b[i];
	if (c->bit_reverse)
		for (i = 0; i < len / 2; i++)
		{
			uc t = disp[i];

			disp[i] = disp[len - 1 - i];
			disp[len - 1 - i] = t;
		}

	ndig = (len + 3) / 4;
	pad  = 4 * ndig - len;
	if (len < c->out_bits && !c->lsb_first)
	{
		/* a short final byte keeps its leading bits: pad on the right */
		for (i = 0; i < pad; i++)
			disp[len + i] = 0;
		tilde = pad != 0;
		pad   = 0;
	}
	else if (len < c->out_bits)
		tilde = pad != 0;

	for (i = 0; i < ndig; i++)
	{
		unsigned v = 0, k;

		for (k = 0; k < 4; k++)
		{
			unsigned at = 4 * i + k;			/* index in padded string */
			int      bit = at < pad ? 0 : disp[at - pad];

			v = (v << 1) | (unsigned) bit;
		}
		fputc("0123456789abcdef"[v], stdout);
	}
	if (tilde)
		fputc('~', stdout);
}

/*
 * Print nb stream-order bits as output bytes, collected in groups of
 * group_size.  Within a group the bytes appear in stream order (the
 * default, "big-endian" display) or reversed (--little-endian-group);
 * --byte-reverse-group flips that again, and --bit-reverse-group reverses
 * every bit of the group, which is a byte reversal plus a bit reversal
 * inside each byte.
 */
static void put_units(const Config *c, const uc *bits, size_t nb)
{
	size_t m      = c->out_bits;
	size_t nunits = (nb + m - 1) / m;
	size_t gi, j;
	int    flip   = (c->little_endian ? 1 : 0) ^ (c->byte_reverse ? 1 : 0)
	                ^ (c->bit_reverse ? 1 : 0);

	for (gi = 0; gi < nunits; gi += c->group_size)
	{
		size_t glen = nunits - gi < c->group_size ? nunits - gi : c->group_size;

		if (gi != 0)
			fputc(' ', stdout);
		for (j = 0; j < glen; j++)
		{
			size_t u    = gi + (flip ? glen - 1 - j : j);
			size_t from = u * m;
			size_t len  = nb - from < m ? nb - from : m;

			put_unit(c, bits + from, (unsigned) len);
		}
	}
}

/* Copy stream bits [from, from+len) of the window into the work buffer. */
static uc *load_bits(Sink *s, const uc *win, size_t from, size_t len)
{
	size_t i;

	if (s->bits_cap < len)
	{
		s->bits     = xrealloc(s->bits, len);
		s->bits_cap = len;
	}
	for (i = 0; i < len; i++)
		s->bits[i] = (uc) stream_bit(win, from + i, s->cfg->lsb_first);
	return s->bits;
}

static void put_ascii(const Config *c, const uc *win, size_t from, size_t len)
{
	size_t off;

	fputs("  |", stdout);
	for (off = 0; off + c->out_bits <= len; off += c->out_bits)
	{
		ulli v = unit_value(win, from + off, c->out_bits, c->lsb_first);

		fputc(v < 256 && isprint((int) v) ? (int) v : '.', stdout);
	}
	fputc('|', stdout);
}

/*
 * Print an offset given in bits past the starting offset.  Offsets are
 * always reported from the start of the file, whatever -s says.
 */
static void put_offset(const Sink *s, ulli rel_bits)
{
	const Config *c    = s->cfg;
	int           base = cfg_out_base(c, 0);
	ulli          abs  = rel_bits + c->start_bits;

	if (c->bit_offset)
	{
		put_number(c, abs, base, s->pad_width, 1);
		return;
	}
	put_number(c, abs / c->in_bits, base, s->pad_width, 1);
	if (abs % c->in_bits)
		printf("+%u", (unsigned) (abs % c->in_bits));
}

static void print_match(Sink *s, const uc *win, size_t winlen, ulli base,
                        size_t q, size_t nbits)
{
	const Config *c     = s->cfg;
	unsigned      n     = c->in_bits;
	ulli          rel   = base * 8 + q - c->start_bits;	/* bits past the origin */
	size_t        mu    = nbits / n;					/* whole input bytes matched */
	ulli          want_b = c->before_fixed + dynamic_context(c, c->prefix_idx, win, q, nbits);
	ulli          want_a = c->after_fixed  + dynamic_context(c, c->suffix_idx, win, q, nbits);
	size_t        avail_b = (size_t) (rel < q ? rel : q);
	size_t        avail_a = winlen * 8 - (q + nbits);
	size_t        nb, na, i;
	ulli          at, last;

	if (want_b > c->max_before) want_b = c->max_before;
	if (want_a > c->max_after)  want_a = c->max_after;
	nb = want_b < avail_b / n ? (size_t) want_b : avail_b / n;
	na = want_a < avail_a / n ? (size_t) want_a : avail_a / n;

	/* start of the last matched byte (a short final byte counts as one) */
	last = rel + (nbits % n ? (ulli) mu * n : (mu ? (ulli) (mu - 1) * n : 0));
	switch (c->offset_style)
	{
		case OFF_FIRST_PREFIX: at = rel - (ulli) nb * n;                  break;
		case OFF_LAST:         at = last;                                 break;
		case OFF_LAST_SUFFIX:  at = na ? rel + nbits + (ulli) (na - 1) * n : last; break;
		default:               at = rel;                                  break;
	}

	if (s->show_name)
		printf("%s:", s->name);
	if (c->show_offset)
	{
		put_offset(s, at);
		fputc(':', stdout);
	}
	if (nb > 0)
	{
		put_units(c, load_bits(s, win, q - nb * n, nb * n), nb * n);
		fputs(" | ", stdout);
	}

	load_bits(s, win, q, nbits);
	for (i = 0; i < c->ninvert; i++)		/* -I: flip all bits of byte K */
		if (c->invert[i] >= 1 && (size_t) c->invert[i] <= mu)
		{
			size_t k, from = (size_t) (c->invert[i] - 1) * n;

			for (k = 0; k < n; k++)
				s->bits[from + k] ^= 1;
		}
	put_units(c, s->bits, nbits);

	if (na > 0)
	{
		fputs(" | ", stdout);
		put_units(c, load_bits(s, win, q + nbits, na * n), na * n);
	}
	if (c->ascii)
		put_ascii(c, win, q - nb * n, nb * n + nbits + na * n);
	fputc('\n', stdout);
}

/*
 * Record a match of 'nbits' bits starting at bit 'q' of the window 'win'
 * (winlen bytes, whose first byte is at file offset 'base').  Prints it
 * unless a summary mode (-c/-l/-L/-q) is active.  Returns nonzero when
 * scanning of this file should stop.
 */
int emit(Sink *s, const uc *win, size_t winlen, ulli base, size_t q, size_t nbits)
{
	const Config *c = s->cfg;

	s->count++;
	if (c->quiet)
		exit(BG_MATCH);			/* first match settles it */

	if (!c->count_only && c->list == LIST_NONE)
		print_match(s, win, winlen, base, q, nbits);

	if (c->list != LIST_NONE)
		return 1;				/* only need to know there was one */
	return c->max_count != 0 && s->count >= c->max_count;
}
