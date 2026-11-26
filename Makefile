# POSIX make: works with GNU make on Linux and make/bmake on the BSDs.
# Windows: use build.bat, or "make CC=gcc EXE=.exe" under MinGW.
#
# Packagers can override CC, CFLAGS, CPPFLAGS, LDFLAGS, PREFIX, BINDIR,
# MANDIR and DESTDIR, e.g.:
#   make install DESTDIR=/tmp/pkg PREFIX=/usr BINDIR=/usr/games
# On OpenBSD, man pages belong in /usr/local/man: add MANDIR=/usr/local/man.
CC = cc
CFLAGS = -O2
CPPFLAGS =
LDFLAGS =
EXE =

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
DESTDIR =

CLASSIC = mine-classic$(EXE)
PATHFINDER = mine-path$(EXE)

all: $(CLASSIC) $(PATHFINDER)

$(CLASSIC): minesweeper-classic.c common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(CLASSIC) minesweeper-classic.c

$(PATHFINDER): minesweeper-pathfinder.c common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(PATHFINDER) minesweeper-pathfinder.c

test: all
	./$(CLASSIC) --self-test
	./$(PATHFINDER) --self-test

install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man6
	install -m 0755 $(CLASSIC) $(DESTDIR)$(BINDIR)/$(CLASSIC)
	install -m 0755 $(PATHFINDER) $(DESTDIR)$(BINDIR)/$(PATHFINDER)
	install -m 0644 man/mine-classic.6 $(DESTDIR)$(MANDIR)/man6/mine-classic.6
	install -m 0644 man/mine-path.6 $(DESTDIR)$(MANDIR)/man6/mine-path.6

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(CLASSIC) $(DESTDIR)$(BINDIR)/$(PATHFINDER)
	rm -f $(DESTDIR)$(MANDIR)/man6/mine-classic.6 $(DESTDIR)$(MANDIR)/man6/mine-path.6

clean:
	rm -f $(CLASSIC) $(PATHFINDER)

.PHONY: all test install uninstall clean
