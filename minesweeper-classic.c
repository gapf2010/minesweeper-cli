/* Minesweeper, classic edition. See README.md. */
#include "common.h"

#define HEADER_LINES 4
#define POLL_MS 100

static int classic_max_mines(int rows, int cols) {
    return rows * cols - 9; /* the first dig keeps a 3x3 area clear */
}

static const Preset presets[] = {
    {"beginner", "Beginner", 9, 9, 10, 0, 0},
    {"intermediate", "Intermediate", 16, 16, 40, 0, 0},
    {"expert", "Expert", 16, 30, 99, 0, 0},
    {"daily", "Daily challenge", 16, 16, 40, 1, 0},
    {"custom", "Custom", 0, 0, 0, 0, 1},
};

static const GameDef game_def = {
    "classic", "Classic Edition", presets, (int)(sizeof(presets) / sizeof(presets[0])),
    classic_max_mines
};

/* Mines are placed on the first dig, away from it and its neighbours. */
static void generate(Board *b, int start_r, int start_c) {
    int dr, dc;

    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            if (in_bounds(b, start_r + dr, start_c + dc)) {
                b->cells[start_r + dr][start_c + dc].reserved = 1;
            }
        }
    }
    board_place_mines(b);
}

static void reveal(Board *b, int r, int c, int *revealed) {
    Cell *cell;
    int dr, dc;

    if (!in_bounds(b, r, c)) return;
    cell = &b->cells[r][c];
    if (cell->is_revealed || cell->mark == MARK_FLAG) return;

    cell->is_revealed = 1;
    cell->mark = MARK_NONE;
    (*revealed)++;
    if (cell->adjacent_mines == 0 && !cell->is_mine) {
        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) reveal(b, r + dr, c + dc, revealed);
        }
    }
}

/* Digging a number whose flags are all placed opens the other neighbours. */
static int chord(Board *b, int r, int c, int *revealed) {
    int dr, dc, flags = 0, hit = 0;

    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            if (in_bounds(b, r + dr, c + dc) && b->cells[r + dr][c + dc].mark == MARK_FLAG) flags++;
        }
    }
    if (flags != b->cells[r][c].adjacent_mines) return 0;

    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            Cell *n;
            if (!in_bounds(b, r + dr, c + dc)) continue;
            n = &b->cells[r + dr][c + dc];
            if (n->is_revealed || n->mark == MARK_FLAG) continue;
            if (n->is_mine) {
                n->exploded = 1;
                hit = 1;
            } else {
                reveal(b, r + dr, c + dc, revealed);
            }
        }
    }
    return hit;
}

/* Returns 1 if this dig set off a mine. */
static int dig(Board *b, int r, int c, int *revealed) {
    Cell *cell = &b->cells[r][c];

    if (cell->mark == MARK_FLAG) return 0;
    if (cell->is_revealed) {
        return cell->adjacent_mines > 0 ? chord(b, r, c, revealed) : 0;
    }
    if (cell->is_mine) {
        cell->exploded = 1;
        return 1;
    }
    reveal(b, r, c, revealed);
    return 0;
}

static void flag_all_mines(Board *b) {
    int r, c;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            if (b->cells[r][c].is_mine) b->cells[r][c].mark = MARK_FLAG;
        }
    }
}

static void draw(const Board *b, const Preset *p, unsigned long seed, int seconds,
                 int cursor_r, int cursor_c) {
    int fits;

    frame_begin();
    printf("===== MINESWEEPER (Classic Edition) - %s =====", p->label);
    frame_eol();
    fputs("Arrows/WASD/HJKL=Move  Space/Enter=Dig  F=Flag  M/?=Mark  P=Pause  Q=Quit", stdout);
    frame_eol();
    printf("Time: %d sec | Mines left: %d | Seed: %lu", seconds, b->mines - board_count_flags(b), seed);
    frame_eol();
    frame_eol();
    fits = board_print_fit(b, cursor_r, cursor_c, HEADER_LINES);
    if (!fits) fputs("Terminal too narrow for this board.", stdout);
    frame_end();
}

/* Plays one game. Returns 0 if input closed and the program should end. */
static int play(const Options *opt, const Preset *p) {
    Board b;
    GameTimer timer;
    unsigned long seed = choose_seed(opt, p);
    int cursor_r = 0, cursor_c = 0;
    int revealed = 0, lost = 0, won = 0, quit = 0, input_open = 1;
    int total_safe = p->rows * p->cols - p->mines;
    int dirty = 1, last_seconds = -1, last_w = 0, last_h = 0;
    int seconds;

    rng_seed(seed);
    board_init(&b, p->rows, p->cols, p->mines);
    memset(&timer, 0, sizeof(timer));

    raw_enter();
    show_cursor(0);
    screen_clear();

    while (!lost && !won && !quit) {
        int term_w, term_h, key;
        Cell *cell = &b.cells[cursor_r][cursor_c];

        term_size(&term_w, &term_h);
        if (term_w != last_w || term_h != last_h) {
            last_w = term_w;
            last_h = term_h;
            screen_clear();
            dirty = 1;
        }
        seconds = timer_seconds(&timer);
        if (seconds != last_seconds) {
            last_seconds = seconds;
            dirty = 1;
        }
        if (dirty) {
            draw(&b, p, seed, seconds, cursor_r, cursor_c);
            dirty = 0;
        }

        key = key_poll(POLL_MS);
        if (key == K_NONE || key == K_ESC) continue;
        dirty = 1;
        switch (key) {
            case K_EOF:
                input_open = 0;
                quit = 1;
                break;
            case K_CTRL_C:
            case 'q': case 'Q':
                quit = 1;
                break;
            case K_UP: case 'w': case 'W': case 'k': case 'K':
                if (cursor_r > 0) cursor_r--;
                break;
            case K_DOWN: case 's': case 'S': case 'j': case 'J':
                if (cursor_r < b.rows - 1) cursor_r++;
                break;
            case K_LEFT: case 'a': case 'A': case 'h': case 'H':
                if (cursor_c > 0) cursor_c--;
                break;
            case K_RIGHT: case 'd': case 'D': case 'l': case 'L':
                if (cursor_c < b.cols - 1) cursor_c++;
                break;
            case 'f': case 'F':
                if (!cell->is_revealed) {
                    cell->mark = cell->mark == MARK_FLAG ? MARK_NONE : MARK_FLAG;
                }
                break;
            case 'm': case 'M': case '?':
                if (!cell->is_revealed) {
                    cell->mark = cell->mark == MARK_QUESTION ? MARK_NONE : MARK_QUESTION;
                }
                break;
            case 'p': case 'P':
                if (timer.running && !pause_screen(&timer, "MINESWEEPER (Classic Edition)")) {
                    input_open = 0;
                    quit = 1;
                }
                break;
            case '\n': case '\r': case ' ':
                if (!timer.running && cell->mark != MARK_FLAG) {
                    generate(&b, cursor_r, cursor_c);
                    timer_start(&timer);
                }
                if (timer.running) {
                    lost = dig(&b, cursor_r, cursor_c, &revealed);
                    won = !lost && revealed == total_safe;
                }
                break;
            default:
                dirty = 0;
                break;
        }
    }

    seconds = timer_seconds(&timer);
    raw_leave();
    show_cursor(1);
    screen_clear();

    b.finished = 1;
    if (lost) board_reveal_mines(&b);
    if (won) flag_all_mines(&b);

    if (won) {
        printf("\nCONGRATULATIONS! YOU WON IN %d SECONDS!\n\n", seconds);
    } else if (lost) {
        printf("\nBOOM! YOU HIT A MINE! Game Over. (X = that mine, x = wrong flag)\n\n");
    } else {
        printf("\nGame Ended.\n\n");
    }
    board_print_final(&b, -1, -1);
    printf("Seed: %lu\n", seed);

    if (won) {
        if (opt->has_seed) {
            printf("Played with --seed, so this score is not saved.\n");
        } else {
            HighScore result;
            memset(&result, 0, sizeof(result));
            result.seconds = seconds;
            result.points = seconds;
            result.rows = b.rows;
            result.cols = b.cols;
            result.mines = b.mines;
            result.date = p->is_daily ? today_utc() : 0;
            if (!scores_record(&game_def, p, &result, 0)) input_open = 0;
        }
    }
    board_free(&b);
    return input_open;
}

static int self_test(void) {
    static const int sizes[][3] = {
        {9, 9, 10}, {16, 16, 40}, {16, 30, 99}, {5, 5, 16}, {30, 40, 1191}
    };
    unsigned long seed;
    size_t i;

    self_test_common();

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        for (seed = 1; seed <= 300; seed++) {
            Board b;
            int r, c, sr, sc, dr, dc, revealed = 0, hit = 0;
            int rows = sizes[i][0], cols = sizes[i][1], mines = sizes[i][2];

            CHECK(mines <= classic_max_mines(rows, cols));
            rng_seed(seed);
            sr = rows / 2; /* the centre leaves the fewest free cells */
            sc = cols / 2;
            board_init(&b, rows, cols, mines);
            generate(&b, sr, sc);
            CHECK(board_count_mines(&b) == mines);
            CHECK(board_counts_correct(&b));
            for (dr = -1; dr <= 1; dr++) {
                for (dc = -1; dc <= 1; dc++) CHECK(!b.cells[sr + dr][sc + dc].is_mine);
            }
            /* Digging every safe cell must reach exactly the win count. */
            for (r = 0; r < rows; r++) {
                for (c = 0; c < cols; c++) {
                    if (!b.cells[r][c].is_mine && !b.cells[r][c].is_revealed) {
                        hit |= dig(&b, r, c, &revealed);
                    }
                }
            }
            CHECK(!hit);
            CHECK(revealed == rows * cols - mines);
            board_free(&b);
        }
    }

    /* The same seed and first dig always give the same board. */
    {
        Board a, b;
        int r;

        board_init(&a, 16, 30, 99);
        board_init(&b, 16, 30, 99);
        rng_seed(42);
        generate(&a, 3, 3);
        rng_seed(42);
        generate(&b, 3, 3);
        for (r = 0; r < 16; r++) {
            CHECK(memcmp(a.cells[r], b.cells[r], 30 * sizeof(Cell)) == 0);
        }
        board_free(&a);
        board_free(&b);
    }

    /* Chording around a single mine at (0,0). */
    {
        Board b;
        int revealed = 0;

        board_init(&b, 5, 5, 1);
        b.cells[0][0].is_mine = 1;
        b.cells[0][1].adjacent_mines = 1;
        b.cells[1][0].adjacent_mines = 1;
        b.cells[1][1].adjacent_mines = 1;
        CHECK(dig(&b, 1, 1, &revealed) == 0 && revealed == 1);
        CHECK(dig(&b, 1, 1, &revealed) == 0 && revealed == 1); /* no flags: no chord */

        /* A correct flag makes it safe and opens everything else. */
        b.cells[0][0].mark = MARK_FLAG;
        CHECK(dig(&b, 1, 1, &revealed) == 0);
        CHECK(revealed == 24);
        CHECK(dig(&b, 0, 0, &revealed) == 0); /* flags can't be dug */
        board_free(&b);

        /* A wrong flag sets off the real mine. */
        board_init(&b, 5, 5, 1);
        b.cells[0][0].is_mine = 1;
        b.cells[0][1].adjacent_mines = 1;
        b.cells[1][0].adjacent_mines = 1;
        b.cells[1][1].adjacent_mines = 1;
        revealed = 0;
        dig(&b, 1, 1, &revealed);
        b.cells[0][1].mark = MARK_FLAG;
        CHECK(dig(&b, 1, 1, &revealed) == 1);
        CHECK(b.cells[0][0].exploded);
        board_free(&b);
    }
    return self_test_result("classic");
}

int main(int argc, char **argv) {
    Options opt;

    parse_options(argc, argv, &opt);
    if (opt.self_test) return self_test();

    for (;;) {
        Preset custom;
        const Preset *p = menu_choose(&game_def, &custom, 0);

        if (!p || !play(&opt, p) || !ask_play_again()) break;
    }
    printf("\nThanks for playing!\n");
    return 0;
}
