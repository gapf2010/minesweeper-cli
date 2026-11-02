#!/bin/sh
# Builds both games with the system C compiler (set CC to override).
set -e
CC=${CC:-cc}
"$CC" -O2 -o mine-classic minesweeper-classic.c
"$CC" -O2 -o mine-path minesweeper-pathfinder.c
