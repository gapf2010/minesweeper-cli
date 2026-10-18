# minesweeper-cli
Two little minesweeper games vibe-coded in C. The versions of the game are:

* Minesweeper classic:
Use the arrow keys to move over the board and hit SPACE to open a square or F to flag it as a mine. Game ends when either you open all squares not containing a mine (you win) or you open a square containing a mine (you lose).

* Minesweeper pathfinder:
Start in the upper left corner of the board and use the arrow keys (or QEZC keys for diagonals) to navigate to the bottom right corner. You do not need to find all mines, but it's a nice challenge.

To compile the games run:

```
$ gcc -o minesweeper-classic minesweeper-classic.c
```

or

```
$ gcc -o minesweeper-pathfinder minesweeper-pathfinder.c
```

The games have been tested to compile and work fine on Linux and OpenBSD (using clang compiler "cc" command instead of "gcc"). They should also compile and work on Windows using MinGW, but that's untested.
