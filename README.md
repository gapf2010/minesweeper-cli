# minesweeper-cli
A little minesweeper game vibecoded in C. There are two versions of the game:

* Minesweeper classic:
Move over the board and hit SPACE to open a square or F to flag it as a mine. Game ends when either you flag all mines (you win) or you open a square containing a mine (you lose).

* Minesweeper pathfinder:
Start in the upper left corner of the board and navigate to get to the bottom right corner. You do not need to find all mines, but it's a nice challenge.

To compile the games run:

```
$ gcc -o minesweeper-classic minesweeper-classic.c
```

or

```
$ gcc -o minesweeper-pathfinder minesweeper-pathfinder.c
```

