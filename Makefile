# Makefile for bgrep -- portable across GNU make, BSD make (OpenBSD, macOS).
#
#   make              build ./bgrep
#   make test         build and run the test suite
#   make install      install to $(DESTDIR)$(PREFIX)
#   make clean
#
# Override on the command line as needed, e.g. on OpenBSD:
#   make install PREFIX=/usr/local MANDIR=/usr/local/man

CC      ?= cc
CFLAGS  ?= -O2
WARN     = -Wall -Wextra
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man
INSTALL ?= install

OBJS = src/main.o src/args.o src/pattern.o src/scan.o src/util.o \
       src/help.o src/usage.o src/version.o

all: bgrep

bgrep: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o bgrep $(OBJS)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -c -o $@ $<

$(OBJS): src/bgrep.h

test: bgrep
	sh tests/run.sh ./bgrep

install: bgrep
	$(INSTALL) -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 755 bgrep $(DESTDIR)$(BINDIR)/bgrep
	$(INSTALL) -m 644 man/bgrep.1 $(DESTDIR)$(MANDIR)/man1/bgrep.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/bgrep $(DESTDIR)$(MANDIR)/man1/bgrep.1

clean:
	rm -f bgrep $(OBJS)

.PHONY: all test install uninstall clean
