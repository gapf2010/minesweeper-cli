# minesweeper-cli
Two little minesweeper games vibe-coded in C. They run in the terminal, need
nothing but a C compiler, and work on Linux, the BSDs and Windows.

## The games

* **Minesweeper classic:** open every square that doesn't contain a mine. Your
  first dig is always safe. Digging a number whose flags are all placed opens
  its remaining neighbours (chording).

* **Minesweeper pathfinder:** walk from the upper left corner to the goal (`G`)
  in the bottom right corner. You only learn about the squares you step on. Every
  board has a safe route.
  The squares you walked on stay underlined, or shown as `.` where there is no
  number.

  Reaching the goal is enough to win, but the score rewards finding everything.
  It works like a penalty time, and lower is better:

  ```
  score = seconds + 30 x (safe squares not explored + wrong marks)
  ```

  Each safe square you leave unexplored costs 30 seconds, so exploring the
  whole board beats rushing to the goal, and among full explorations the
  fastest wins. Marking mines (M or F, then a direction) is optional: it never
  lowers your score, so you can just remember where they are. A mark on a safe
  square costs 30 seconds, though. During play the header shows how many marks
  you've placed but not whether they're right; the end screen shows the
  breakdown.

## Controls

| Action | Classic | Pathfinder |
| --- | --- | --- |
| Move | Arrows, WASD or HJKL | Arrows, WASD or HJKL |
| Move diagonally | | Q E Z C or Y U B N |
| Dig | Space or Enter | (moving digs) |
| Flag | F | M or F, then a direction |
| Mark `?` | M or ? | |
| Pause | P | P |
| Quit | Q | Shift+Q |

## Building

Linux and BSD:

```
$ make        # or: sh build.sh
$ make test   # runs the built-in checks
```

Windows, with MinGW gcc or MSVC on the PATH:

```
> build.bat
```

Each game is also a single compile, so no build tool is needed:

```
$ cc -o mine-classic minesweeper-classic.c
$ cc -o mine-path minesweeper-pathfinder.c
```

## Options

| Option | Meaning |
| --- | --- |
| `--seed N` | Play the board with this seed. Scores are not saved. |
| `--no-color` | Monochrome output. Setting `NO_COLOR` does the same. |
| `--ascii` | Use `#` for hidden squares. |
| `--self-test` | Run the built-in checks and exit. |
| `--version`, `--help` | Show the version or the help. |

The seed is shown during play and on the end screen, and it gives the same
board on every platform. In classic the first dig is part of the board layout,
so dig the same square to get the same game.

## Daily challenge and scores

The daily challenge is one board per day (UTC) for everyone. Its table only
keeps the current day's scores.

Each difficulty has its own table of the top 5. The tables are kept together
in `~/.config/mine-cli/` (or `$XDG_CONFIG_HOME/mine-cli/` when that is set),
or `%APPDATA%\mine-cli\` on Windows, as one short file per difficulty:
`classic-beginner.txt`, `path-hp48.txt` and so on.

Boards that don't fit the terminal adapt: squares get narrower, and tall boards
scroll with the cursor (`^` and `v` in the border mean there is more).
