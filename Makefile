# POSIX make: works with GNU make on Linux and bmake/make on the BSDs.
# Windows: use build.bat, or "make CC=gcc EXE=.exe" under MinGW.
CC = cc
CFLAGS = -O2
EXE =

CLASSIC = mine-classic$(EXE)
PATHFINDER = mine-path$(EXE)

all: $(CLASSIC) $(PATHFINDER)

$(CLASSIC): minesweeper-classic.c common.h
	$(CC) $(CFLAGS) -o $(CLASSIC) minesweeper-classic.c

$(PATHFINDER): minesweeper-pathfinder.c common.h
	$(CC) $(CFLAGS) -o $(PATHFINDER) minesweeper-pathfinder.c

test: all
	./$(CLASSIC) --self-test
	./$(PATHFINDER) --self-test

clean:
	rm -f $(CLASSIC) $(PATHFINDER)
