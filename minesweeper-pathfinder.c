/* Minesweeper, pathfinder edition. See README.md. */
#include "common.h"

#define HEADER_LINES 5
#define POLL_MS 100
#define GENERATE_ATTEMPTS 500

/* A shortest walk corner to corner uses max(rows, cols) cells; those have to
 * stay free or no board of that size could be solved. */
static int pathfinder_max_mines(int rows, int cols) {
    return rows * cols - (rows > cols ? rows : cols);
}

static const Preset presets[] = {
    {"beginner", "Beginner", 9, 9, 10, 0, 0},
    {"hp48", "HP 48G/GX mode", 8, 16, 20, 0, 0},
    {"intermediate", "Intermediate", 16, 16, 40, 0, 0},
    {"daily", "Daily challenge", 16, 16, 40, 1, 0},
    {"custom", "Custom", 0, 0, 0, 0, 1},
};

static const GameDef game_def = {
    "path", "Pathfinder Edition", presets, (int)(sizeof(presets) / sizeof(presets[0])),
    pathfinder_max_mines
};

/* Can the goal be reached from (0,0), moving in all 8 directions? */
static int path_exists(const Board *b) {
    int total = b->rows * b->cols;
    int *queue = (int *)alloc_or_die((size_t)total * sizeof(int));
    char *seen = (char *)alloc_or_die((size_t)total);
    int head = 0, tail = 0, found = 0;

    queue[tail++] = 0;
    seen[0] = 1;
    while (head < tail && !found) {
        int r = queue[head] / b->cols;
        int c = queue[head] % b->cols;
        int dr, dc;

        head++;
        if (r == b->goal_r && c == b->goal_c) found = 1;
        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) {
                int nr = r + dr, nc = c + dc, id;
                if (!in_bounds(b, nr, nc) || b->cells[nr][nc].is_mine) continue;
                id = nr * b->cols + nc;
                if (!seen[id]) {
                    seen[id] = 1;
                    queue[tail++] = id;
                }
            }
        }
    }
    free(queue);
    free(seen);
    return found;
}

/* Reserves a random shortest walk from start to goal. */
static void reserve_path(Board *b) {
    int r = 0, c = 0;

    b->cells[0][0].reserved = 1;
    while (r != b->goal_r || c != b->goal_c) {
        int left_r = b->goal_r - r, left_c = b->goal_c - c;

        if (left_r == left_c) {
            r++;
            c++;
        } else if (left_r > left_c) {
            r++;
            if (left_c > 0 && rng_below(2)) c++;
        } else {
            c++;
            if (left_r > 0 && rng_below(2)) r++;
        }
        b->cells[r][c].reserved = 1;
    }
}

/*
 * Every board can be solved: boards that wall off the goal are thrown away.
 * Very dense boards almost never pass by luck, so after enough failures a
 * walk is set aside before placing the mines. That is what keeps this from
 * looping forever.
 */
static void generate(Board *b) {
    int attempt;

    for (attempt = 0; attempt < GENERATE_ATTEMPTS; attempt++) {
        board_clear(b);
        b->cells[0][0].reserved = 1;
        b->cells[b->goal_r][b->goal_c].reserved = 1;
        board_place_mines(b);
        if (path_exists(b)) return;
    }
    board_clear(b);
    reserve_path(b);
    board_place_mines(b);
}

/* Moves the player onto a cell. Returns 1 if it was a mine. */
static int step(Board *b, int r, int c) {
    Cell *cell = &b->cells[r][c];
    int dr, dc;

    cell->is_revealed = 1;
    cell->mark = MARK_NONE;
    if (cell->is_mine) {
        cell->exploded = 1;
        return 1;
    }
    cell->visited = 1;
    if (cell->adjacent_mines == 0) { /* every neighbour is known to be safe */
        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) {
                if (in_bounds(b, r + dr, c + dc)) {
                    b->cells[r + dr][c + dc].is_revealed = 1;
                    b->cells[r + dr][c + dc].mark = MARK_NONE;
                }
            }
        }
    }
    return 0;
}

static int count_revealed_safe(const Board *b) {
    int r, c, count = 0;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            if (b->cells[r][c].is_revealed && !b->cells[r][c].is_mine) count++;
        }
    }
    return count;
}

/* Lower is better: fast, and with little of the board left unexplored. */
static int compute_points(const Board *b, int seconds) {
    return (b->rows * b->cols - b->mines - count_revealed_safe(b)) * seconds;
}

/* Maps a key to a move. Returns 0 if it isn't a movement key. */
static int key_direction(int key, int *dr, int *dc) {
    if (key >= 'A' && key <= 'Z') key = tolower(key);
    *dr = 0;
    *dc = 0;
    switch (key) {
        case K_UP: case 'w': case 'k': *dr = -1; break;
        case K_DOWN: case 's': case 'j': *dr = 1; break;
        case K_LEFT: case 'a': case 'h': *dc = -1; break;
        case K_RIGHT: case 'd': case 'l': *dc = 1; break;
        case 'q': case 'y': *dr = -1; *dc = -1; break;
        case 'e': case 'u': *dr = -1; *dc = 1; break;
        case 'z': case 'b': *dr = 1; *dc = -1; break;
        case 'c': case 'n': *dr = 1; *dc = 1; break;
        default: return 0;
    }
    return 1;
}

static void draw(const Board *b, const Preset *p, unsigned long seed, int seconds,
                 int player_r, int player_c, const char *message) {
    int fits;

    frame_begin();
    printf("===== MINESWEEPER (Pathfinder Edition) - %s =====", p->label);
    frame_eol();
    fputs("Reach G. Arrows/WASD/HJKL=Move  Q,E,Z,C or Y,U,B,N=Diagonals", stdout);
    frame_eol();
    fputs("M/F+direction=Mark suspected mine  P=Pause  Shift+Q=Quit", stdout);
    frame_eol();
    printf("Time: %d sec | Mines: %d | Remaining: %d | Seed: %lu", seconds, b->mines,
           b->rows * b->cols - b->mines - count_revealed_safe(b), seed);
    frame_eol();
    fputs(message, stdout);
    frame_eol();
    fits = board_print_fit(b, player_r, player_c, HEADER_LINES);
    if (!fits) fputs("Terminal too narrow for this board.", stdout);
    frame_end();
}

/* Plays one game. Returns 0 if input closed and the program should end. */
static int play(const Options *opt, const Preset *p) {
    Board b;
    GameTimer timer;
    unsigned long seed = choose_seed(opt, p);
    int player_r = 0, player_c = 0;
    int lost = 0, won = 0, quit = 0, input_open = 1, marking = 0;
    int dirty = 1, last_seconds = -1, last_w = 0, last_h = 0;
    int seconds;
    const char *message = "";

    rng_seed(seed);
    board_init(&b, p->rows, p->cols, p->mines);
    b.goal_r = b.rows - 1;
    b.goal_c = b.cols - 1;
    generate(&b);
    step(&b, player_r, player_c);
    timer_start(&timer);

    raw_enter();
    show_cursor(0);
    screen_clear();

    while (!lost && !won && !quit) {
        int term_w, term_h, key, dr, dc;

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
            draw(&b, p, seed, seconds, player_r, player_c, message);
            dirty = 0;
        }

        key = key_poll(POLL_MS);
        if (key == K_NONE) continue;
        dirty = 1;
        message = "";

        if (key == K_EOF) {
            input_open = 0;
            quit = 1;
        } else if (key == 'Q' || key == K_CTRL_C) {
            quit = 1;
        } else if (key == K_ESC) {
            marking = 0;
        } else if (key == 'p' || key == 'P') {
            marking = 0;
            if (!pause_screen(&timer, "MINESWEEPER (Pathfinder Edition)")) {
                input_open = 0;
                quit = 1;
            }
        } else if (marking) {
            marking = 0;
            if (key_direction(key, &dr, &dc) && in_bounds(&b, player_r + dr, player_c + dc)) {
                Cell *cell = &b.cells[player_r + dr][player_c + dc];
                if (!cell->is_revealed) {
                    cell->mark = cell->mark == MARK_FLAG ? MARK_NONE : MARK_FLAG;
                }
            }
        } else if (key == 'm' || key == 'M' || key == 'f' || key == 'F') {
            marking = 1;
            message = "Mark which way? (any direction key, Esc to cancel)";
        } else if (key_direction(key, &dr, &dc) && in_bounds(&b, player_r + dr, player_c + dc)) {
            if (b.cells[player_r + dr][player_c + dc].mark == MARK_FLAG) {
                message = "That cell is marked as a mine. Unmark it first to step there.";
            } else {
                player_r += dr;
                player_c += dc;
                lost = step(&b, player_r, player_c);
                won = !lost && player_r == b.goal_r && player_c == b.goal_c;
            }
        }
    }

    seconds = timer_seconds(&timer);
    raw_leave();
    show_cursor(1);
    screen_clear();

    b.finished = 1;
    if (lost) board_reveal_mines(&b);

    if (won) {
        printf("\nCONGRATULATIONS! YOU REACHED THE GOAL IN %d SECONDS!\n\n", seconds);
    } else if (lost) {
        printf("\nBOOM! YOU HIT A MINE! Game Over.\n\n");
    } else {
        printf("\nGame Ended.\n\n");
    }
    board_print_final(&b, player_r, player_c);
    printf("Seed: %lu\n", seed);

    if (won) {
        int points = compute_points(&b, seconds);
        printf("Score: %d points (%d cells left unexplored)\n", points,
               b.rows * b.cols - b.mines - count_revealed_safe(&b));
        if (opt->has_seed) {
            printf("Played with --seed, so this score is not saved.\n");
        } else {
            HighScore result;
            memset(&result, 0, sizeof(result));
            result.seconds = seconds;
            result.points = points;
            result.rows = b.rows;
            result.cols = b.cols;
            result.mines = b.mines;
            result.date = p->is_daily ? today_utc() : 0;
            if (!scores_record(&game_def, p, &result, 1)) input_open = 0;
        }
    }
    board_free(&b);
    return input_open;
}

static int self_test(void) {
    static const int sizes[][3] = {
        {9, 9, 10}, {8, 16, 20}, {16, 16, 40}, {9, 9, 30}, {9, 9, 72}, {5, 5, 20},
        {30, 40, 1160}, {30, 5, 120}
    };
    unsigned long seed;
    size_t i;

    self_test_common();

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        for (seed = 1; seed <= 100; seed++) {
            Board b;
            int rows = sizes[i][0], cols = sizes[i][1], mines = sizes[i][2];

            CHECK(mines <= pathfinder_max_mines(rows, cols));
            rng_seed(seed);
            board_init(&b, rows, cols, mines);
            b.goal_r = rows - 1;
            b.goal_c = cols - 1;
            generate(&b);
            CHECK(board_count_mines(&b) == mines);
            CHECK(board_counts_correct(&b));
            CHECK(!b.cells[0][0].is_mine && !b.cells[rows - 1][cols - 1].is_mine);
            CHECK(path_exists(&b));
            board_free(&b);
        }
    }

    /* Points count the final step, so full exploration really scores 0. */
    {
        Board b;
        int r, c;

        board_init(&b, 5, 5, 1);
        b.goal_r = 4;
        b.goal_c = 4;
        b.cells[2][2].is_mine = 1;
        CHECK(compute_points(&b, 10) == 240);
        b.cells[0][0].is_revealed = 1;
        CHECK(compute_points(&b, 10) == 230);
        for (r = 0; r < 5; r++) {
            for (c = 0; c < 5; c++) {
                if (!b.cells[r][c].is_mine) b.cells[r][c].is_revealed = 1;
            }
        }
        CHECK(compute_points(&b, 10) == 0);
        board_free(&b);
    }

    /* A zero cell opens its neighbours, and stepping on a mine loses. */
    {
        Board b;

        board_init(&b, 5, 5, 1);
        b.goal_r = 4;
        b.goal_c = 4;
        b.cells[4][0].is_mine = 1;
        b.cells[1][1].mark = MARK_FLAG;
        CHECK(step(&b, 0, 0) == 0);
        CHECK(b.cells[1][1].is_revealed && b.cells[1][1].mark == MARK_NONE);
        CHECK(step(&b, 4, 0) == 1 && b.cells[4][0].exploded);
        board_free(&b);
    }
    return self_test_result("pathfinder");
}

int main(int argc, char **argv) {
    Options opt;

    parse_options(argc, argv, &opt);
    if (opt.self_test) return self_test();

    for (;;) {
        Preset custom;
        const Preset *p = menu_choose(&game_def, &custom, 1);

        if (!p || !play(&opt, p) || !ask_play_again()) break;
    }
    printf("\nThanks for playing!\n");
    return 0;
}
