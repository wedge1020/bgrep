# Makefile for bgrep -- portable across GNU make, BSD make (OpenBSD, macOS).
#
#   make              build bin/bgrep
#   make test         build and run the test suite
#   make install      install bin/bgrep to $(PREFIX)/bin (default /usr/local/bin)
#                     and man/man1/bgrep.1 to the section 1 man directory
#   make uninstall
#   make clean
#
# Layout:  src/ C sources   inc/ headers   man/man1/ manual page
#          docs/ extra documentation   tests/ test suite   bin/ build output
#
# The man page goes to $(PREFIX)/man/man1 on OpenBSD and $(PREFIX)/share/man/man1
# elsewhere; override with MANDIR=/some/dir to pick the man root yourself.

CC      ?= cc
CFLAGS  ?= -O2
WARN     = -Wall -Wextra
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?=
INSTALL ?= install

OBJS = src/main.o src/args.o src/pattern.o src/scan.o src/output.o src/util.o \
       src/help.o src/usage.o src/version.o

all: bin/bgrep

bin/bgrep: $(OBJS)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/bgrep $(OBJS)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) $(WARN) -Iinc -D_FILE_OFFSET_BITS=64 $(CPPFLAGS) -c -o $@ $<

$(OBJS): inc/bgrep.h

test: bin/bgrep
	sh tests/run.sh ./bin/bgrep

install: bin/bgrep
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 bin/bgrep $(DESTDIR)$(BINDIR)/bgrep
	@m="$(MANDIR)"; \
	if [ -z "$$m" ]; then \
		if [ "`uname -s`" = OpenBSD ]; then m="$(PREFIX)/man"; \
		else m="$(PREFIX)/share/man"; fi; \
	fi; \
	echo "installing man page to $(DESTDIR)$$m/man1"; \
	$(INSTALL) -d $(DESTDIR)$$m/man1 && \
	$(INSTALL) -m 644 man/man1/bgrep.1 $(DESTDIR)$$m/man1/bgrep.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/bgrep
	@m="$(MANDIR)"; \
	if [ -z "$$m" ]; then \
		if [ "`uname -s`" = OpenBSD ]; then m="$(PREFIX)/man"; \
		else m="$(PREFIX)/share/man"; fi; \
	fi; \
	rm -f $(DESTDIR)$$m/man1/bgrep.1

clean:
	rm -f bin/bgrep $(OBJS)
	-rmdir bin 2>/dev/null

.PHONY: all test install uninstall clean
