/* util.c - error reporting and checked allocation helpers. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "bgrep.h"

const char *progname = "bgrep";

static void report(const char *fmt, va_list ap)
{
	fflush(stdout);		/* keep stdout and stderr in order */
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
}

/* Report a fatal error and exit with BG_ERROR. */
void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	report(fmt, ap);
	va_end(ap);
	exit(BG_ERROR);
}

/* Report a non-fatal error; the caller records that one occurred. */
void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	report(fmt, ap);
	va_end(ap);
}

void *xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);

	if (p == NULL)
		die("out of memory");
	return p;
}

void *xrealloc(void *old, size_t n)
{
	void *p = realloc(old, n ? n : 1);

	if (p == NULL)
		die("out of memory");
	return p;
}

char *xstrdup(const char *s)
{
	size_t  n = strlen(s) + 1;

	return memcpy(xmalloc(n), s, n);
}

void strlist_add(StrList *l, const char *s)
{
	if (l->count == l->cap)
	{
		l->cap  = l->cap ? l->cap * 2 : 8;
		l->item = xrealloc(l->item, l->cap * sizeof(*l->item));
	}
	l->item[l->count++] = xstrdup(s);
}
