#!/bin/sh
if [ $(uname) = "Linux" ] ; then
  gcc -o mine-classic minesweeper-classic.c
  gcc -o mine-path minesweeper-pathfinder.c
elif [ $(uname) = "OpenBSD" ] ; then
  cc -o mine-classic minesweeper-classic.c
  cc -o mine-path minesweeper-pathfinder.c
fi
