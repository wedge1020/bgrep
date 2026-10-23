/*
 * output.c - turn a match into output.
 *
 * emit() is called by the scanners with a "window" of input that contains
 * the match plus whatever context is available around it, so that leading
 * and trailing context bytes (-A/-B/-P/-S) can be printed without the
 * scanners knowing anything about output formats.
 *
 * Output line:
 *   [FILE:][OFFSET:]BYTES[ | CONTEXT-AFTER]   [  |ascii|]
 * where leading context, when present, comes first, set off with " | ".
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

static uc reverse_bits(uc b)
{
	b = (uc) (((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
	b = (uc) (((b & 0xCC) >> 2) | ((b & 0x33) << 2));
	b = (uc) (((b & 0xAA) >> 1) | ((b & 0x55) << 1));
	return b;
}

/*
 * Print n bytes as hex, in groups of group_size separated by spaces.  Within
 * a group the bytes appear in stream order (big-endian display, the
 * default) or reversed (--little-endian-group); --byte-reverse-group flips
 * that again, and --bit-reverse-group reverses every bit of the group,
 * which is a byte reversal plus a bit reversal inside each byte.
 */
static void put_bytes(const Config *c, const uc *p, size_t n)
{
	size_t i, j;
	int    flip = (c->little_endian ? 1 : 0) ^ (c->byte_reverse ? 1 : 0)
	              ^ (c->bit_reverse ? 1 : 0);

	for (i = 0; i < n; i += c->group_size)
	{
		size_t len = n - i < c->group_size ? n - i : c->group_size;

		if (i != 0)
			fputc(' ', stdout);
		for (j = 0; j < len; j++)
		{
			uc b = p[i + (flip ? len - 1 - j : j)];

			if (c->bit_reverse)
				b = reverse_bits(b);
			printf("%02x", b);
		}
	}
}

static void put_ascii(const uc *p, size_t n)
{
	size_t i;

	fputs("  |", stdout);
	for (i = 0; i < n; i++)
		fputc(isprint(p[i]) ? p[i] : '.', stdout);
	fputc('|', stdout);
}

/* Context count taken from match byte 'idx' (1-based), adjusted by -O. */
static size_t dynamic_context(const Config *c, unsigned idx,
                              const uc *m, size_t mlen)
{
	long long v;

	if (idx == 0 || idx > mlen)
		return 0;
	v = (long long) m[idx - 1] + c->off_by;
	return v < 0 ? 0 : (size_t) v;
}

static void put_offset(const Sink *s, ulli v)
{
	put_number(s->cfg, v, cfg_out_base(s->cfg, 0), s->pad_width, 1);
}

/* --half-byte output: the matched nibbles, regrouped from the match start. */
static void print_nibbles(const Sink *s, const uc *win, ulli base,
                          size_t q, size_t nibs)
{
	const Config *c     = s->cfg;
	ulli          first = base * 2 + q;
	ulli          last  = first + nibs - 1;
	ulli          at    = (c->offset_style == OFF_LAST ||
	                       c->offset_style == OFF_LAST_SUFFIX) ? last : first;
	size_t        k;

	if (s->show_name)
		printf("%s:", s->name);
	if (c->show_offset)
	{
		put_offset(s, at / 2);
		if (at & 1)
			fputs(".5", stdout);
		fputc(':', stdout);
	}
	for (k = 0; k < nibs; k++)
	{
		size_t n = q + k;
		uc     b = win[n / 2];

		if (k != 0 && k % 2 == 0)
			fputc(' ', stdout);
		printf("%x", (n & 1) ? b & 0x0F : b >> 4);
	}
	fputc('\n', stdout);
}

static void print_match(Sink *s, const uc *win, size_t winlen, ulli base,
                        size_t mstart, size_t mlen)
{
	const Config *c      = s->cfg;
	const uc     *m      = win + mstart;
	size_t        want_b = (size_t) c->before_fixed + dynamic_context(c, c->prefix_idx, m, mlen);
	size_t        want_a = (size_t) c->after_fixed  + dynamic_context(c, c->suffix_idx, m, mlen);
	size_t        nb     = want_b < mstart ? want_b : mstart;
	size_t        after  = winlen - (mstart + mlen);
	size_t        na     = want_a < after ? want_a : after;
	const uc     *show   = m;
	ulli          at;
	size_t        i;

	switch (c->offset_style)
	{
		case OFF_FIRST_PREFIX: at = base + mstart - nb;            break;
		case OFF_LAST:         at = base + mstart + mlen - 1;      break;
		case OFF_LAST_SUFFIX:  at = base + mstart + mlen - 1 + na; break;
		default:               at = base + mstart;                 break;
	}

	if (c->ninvert > 0)		/* work on a copy so the ASCII column stays raw */
	{
		if (s->scratch_cap < mlen)
		{
			s->scratch     = xrealloc(s->scratch, mlen);
			s->scratch_cap = mlen;
		}
		memcpy(s->scratch, m, mlen);
		for (i = 0; i < c->ninvert; i++)
			if (c->invert[i] >= 1 && c->invert[i] <= mlen)
				s->scratch[c->invert[i] - 1] ^= 0xFF;
		show = s->scratch;
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
		put_bytes(c, m - nb, nb);
		fputs(" | ", stdout);
	}
	put_bytes(c, show, mlen);
	if (na > 0)
	{
		fputs(" | ", stdout);
		put_bytes(c, m + mlen, na);
	}
	if (c->ascii)
		put_ascii(m - nb, nb + mlen + na);
	fputc('\n', stdout);
}

/*
 * Record a match of 'nibs' nibbles starting at nibble 'q' of the window
 * 'win' (winlen bytes, whose first byte is at file offset 'base').  Prints
 * it unless a summary mode (-c/-l/-L/-q) is active.  Returns nonzero when
 * scanning of this file should stop.
 */
int emit(Sink *s, const uc *win, size_t winlen, ulli base, size_t q, size_t nibs)
{
	const Config *c = s->cfg;

	s->count++;
	if (c->quiet)
		exit(BG_MATCH);			/* first match settles it */

	if (!c->count_only && c->list == LIST_NONE)
	{
		if (c->half_byte)
			print_nibbles(s, win, base, q, nibs);
		else
			print_match(s, win, winlen, base, q / 2, nibs / 2);
	}

	if (c->list != LIST_NONE)
		return 1;				/* only need to know there was one */
	return c->max_count != 0 && s->count >= c->max_count;
}
