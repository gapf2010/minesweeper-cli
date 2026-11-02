/*
 * common.h - code shared by both minesweeper games.
 *
 * Header-only on purpose: each game still builds with a single
 * "cc -o name file.c" and needs nothing beyond the C library and the
 * operating system's own headers (POSIX on Linux/BSD, Win32 on Windows).
 */
#ifndef MINESWEEPER_COMMON_H
#define MINESWEEPER_COMMON_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE /* keeps POSIX/BSD APIs visible with gcc -std=c99 */
#endif

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* GetTickCount64 */
#endif
#include <windows.h>
#include <conio.h>
#include <io.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define MS_VERSION "2.0"

#define MAX_SCORES 5
#define MAX_NAME 32
#define MAX_PATH_LEN 1024

#define MIN_ROWS 5
#define MAX_ROWS 30
#define MIN_COLS 5
#define MAX_COLS 40

/* Keys that are not plain characters. */
#define K_NONE (-1) /* nothing pressed before the timeout */
#define K_EOF (-2)  /* input closed */
#define K_ESC 27
#define K_UP 1001
#define K_DOWN 1002
#define K_RIGHT 1003
#define K_LEFT 1004
#define K_CTRL_C 1005 /* Windows reports Ctrl-C as a key */

#define MARK_NONE 0
#define MARK_FLAG 1
#define MARK_QUESTION 2

typedef struct {
    int is_mine;
    int is_revealed;
    int mark;
    int adjacent_mines;
    int visited;  /* pathfinder: the player walked here */
    int exploded; /* this is the mine that ended the game */
    int reserved; /* never place a mine here */
} Cell;

typedef struct {
    int rows;
    int cols;
    int mines;
    int goal_r; /* -1 when the game has no goal cell */
    int goal_c;
    int finished; /* draw wrong flags once the game is over */
    Cell **cells;
} Board;

typedef struct {
    char name[MAX_NAME];
    int seconds;
    int rows;
    int cols;
    int mines;
    int points; /* lower is better; classic stores the seconds here */
    int date;   /* YYYYMMDD (UTC) for daily challenges, otherwise 0 */
} HighScore;

typedef struct {
    const char *id; /* score file suffix */
    const char *label;
    int rows;
    int cols;
    int mines;
    int is_daily;
    int is_custom;
} Preset;

typedef struct {
    const char *key;   /* score file name part */
    const char *title; /* shown in menus */
    const Preset *presets;
    int preset_count;
    int (*max_mines)(int rows, int cols);
} GameDef;

typedef struct {
    int has_seed;
    unsigned long seed;
    int self_test;
} Options;

/* ------------------------------------------------------------------ */
/* Random numbers                                                      */
/* ------------------------------------------------------------------ */

/* Our own generator, because rand() differs between C libraries and the
 * same seed has to give the same board on every platform. (SplitMix32) */
static unsigned long rng_state;

static void rng_seed(unsigned long seed) {
    rng_state = seed & 0xFFFFFFFFUL;
}

static unsigned long rng_next(void) {
    unsigned long z;
    rng_state = (rng_state + 0x9E3779B9UL) & 0xFFFFFFFFUL;
    z = rng_state;
    z = ((z ^ (z >> 16)) * 0x85EBCA6BUL) & 0xFFFFFFFFUL;
    z = ((z ^ (z >> 13)) * 0xC2B2AE35UL) & 0xFFFFFFFFUL;
    return z ^ (z >> 16);
}

/* Uniform value in 0..n-1, without modulo bias. */
static int rng_below(int n) {
    unsigned long range = (unsigned long)n;
    unsigned long limit = 0xFFFFFFFFUL - (0xFFFFFFFFUL % range + 1) % range;
    unsigned long v;
    do {
        v = rng_next();
    } while (v > limit);
    return (int)(v % range);
}

/* ------------------------------------------------------------------ */
/* Terminal                                                            */
/* ------------------------------------------------------------------ */

static int use_vt = 0;       /* terminal understands ANSI escapes */
static int use_color = 0;    /* colored output wanted and possible */
static int use_ascii = 0;    /* avoid the non-ASCII hidden-cell glyph */
static int term_inited = 0;

#ifndef _WIN32

static struct termios orig_termios;
static volatile sig_atomic_t raw_active = 0;

static void raw_leave(void) {
    if (raw_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        raw_active = 0;
    }
}

static void raw_enter(void) {
    struct termios raw;
    if (raw_active || tcgetattr(STDIN_FILENO, &orig_termios) != 0) return;
    raw = orig_termios;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    raw_active = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void on_fatal_signal(int sig) {
    /* Only async-signal-safe calls in here. */
    static const char show_cursor[] = "\033[0m\033[?25h\n";
    if (raw_active) tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    if (use_vt) {
        ssize_t ignored = write(STDOUT_FILENO, show_cursor, sizeof(show_cursor) - 1);
        (void)ignored;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Drop keys typed ahead, so they don't end up as a player name. */
static void input_flush(void) {
    tcflush(STDIN_FILENO, TCIFLUSH);
}

/* Returns one byte, K_NONE after timeout_ms (negative: wait forever) or K_EOF. */
static int read_byte(int timeout_ms) {
    for (;;) {
        fd_set fds;
        struct timeval tv;
        unsigned char c;
        int ready;
        ssize_t n;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, timeout_ms < 0 ? NULL : &tv);
        if (ready == 0) return K_NONE;
        if (ready < 0) return K_NONE; /* interrupted, e.g. window resize */
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1) return c;
        if (n == 0) return K_EOF;
        return K_EOF;
    }
}

/*
 * Reads a key, waiting at most timeout_ms. A bare Esc is told apart from an
 * escape sequence by the 50 ms gap after it, and sequences we don't know
 * (F-keys, Ctrl+arrows...) are swallowed whole instead of leaking into the
 * game as letters.
 */
static int key_poll(int timeout_ms) {
    int c = read_byte(timeout_ms);
    int next, final;

    if (c != K_ESC) return c;
    next = read_byte(50);
    if (next == K_NONE || next == K_EOF) return K_ESC;
    if (next != '[' && next != 'O') return K_NONE;

    final = read_byte(50);
    while (final >= 0x20 && final < 0x40) final = read_byte(50); /* parameters */
    switch (final) {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case K_EOF: return K_EOF;
        default: return K_NONE;
    }
}

static void term_size(int *width, int *height) {
    struct winsize ws;
    *width = 0;
    *height = 0;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    }
}

static long long ms_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* stdio is bypassed so no input ever sits in a buffer read() can't see. */
static int read_line(char *buf, size_t size) {
    size_t len = 0;
    int got_any = 0;

    fflush(stdout);
    for (;;) {
        int c = read_byte(-1);
        if (c == K_NONE) continue;
        if (c == K_EOF) {
            if (!got_any) return 0;
            break;
        }
        got_any = 1;
        if (c == '\n') break;
        if (len + 1 < size) buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return 1;
}

static void platform_init(void) {
    static const int sigs[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
    const char *term = getenv("TERM");
    size_t i;

    use_vt = isatty(STDOUT_FILENO) && term && *term && strcmp(term, "dumb") != 0;
    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) signal(sigs[i], on_fatal_signal);
}

static void platform_restore(void) {
    raw_leave();
}

#else /* _WIN32 */

static HANDLE con_out = INVALID_HANDLE_VALUE;
static DWORD orig_out_mode = 0;
static UINT orig_out_cp = 0;
static int have_out_mode = 0;

static void raw_enter(void) {} /* _getch needs no terminal mode change */
static void raw_leave(void) {}

static void input_flush(void) {
    if (_isatty(0)) {
        while (_kbhit()) (void)_getch();
    }
}

static int win_translate_extended(int code) {
    switch (code) {
        case 72: return K_UP;
        case 80: return K_DOWN;
        case 77: return K_RIGHT;
        case 75: return K_LEFT;
        default: return K_NONE;
    }
}

static int key_poll(int timeout_ms) {
    long long waited = 0;
    int c;

    if (!_isatty(0)) { /* redirected input: _getch would read the console */
        c = getchar();
        return c == EOF ? K_EOF : c;
    }
    while (!_kbhit()) {
        if (timeout_ms >= 0 && waited >= timeout_ms) return K_NONE;
        Sleep(10);
        waited += 10;
    }
    c = _getch();
    /* 0 and 224 prefix special keys; 224 alone can also be a real letter. */
    if (c == 0 || (c == 224 && _kbhit())) return win_translate_extended(_getch());
    if (c == 3) return K_CTRL_C;
    return c;
}

static void term_size(int *width, int *height) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    *width = 0;
    *height = 0;
    if (GetConsoleScreenBufferInfo(con_out, &info)) {
        *width = info.srWindow.Right - info.srWindow.Left + 1;
        *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

static long long ms_now(void) {
    return (long long)GetTickCount64();
}

static int read_line(char *buf, size_t size) {
    size_t len;
    int c;

    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) return 0;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    } else { /* too long for buf: drop the rest of the line */
        while ((c = getchar()) != EOF && c != '\n') {}
    }
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
    return 1;
}

static void platform_restore(void) {
    if (have_out_mode) SetConsoleMode(con_out, orig_out_mode);
    if (orig_out_cp) SetConsoleOutputCP(orig_out_cp);
}

static BOOL WINAPI on_console_event(DWORD event) {
    (void)event;
    if (use_vt) {
        DWORD written;
        static const char reset[] = "\033[0m\033[?25h\n";
        WriteConsoleA(con_out, reset, (DWORD)(sizeof(reset) - 1), &written, NULL);
    }
    platform_restore();
    return FALSE; /* let the default handler end the process */
}

static void platform_init(void) {
    con_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(con_out, &orig_out_mode)) {
        have_out_mode = 1;
        if (SetConsoleMode(con_out, orig_out_mode | ENABLE_PROCESSED_OUTPUT |
                                        ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            use_vt = 1;
        }
    }
    orig_out_cp = GetConsoleOutputCP();
    if (!SetConsoleOutputCP(CP_UTF8)) {
        orig_out_cp = 0;
        use_ascii = 1;
    }
    SetConsoleCtrlHandler(on_console_event, TRUE);
}

#endif /* _WIN32 */

static void show_cursor(int show) {
    if (use_vt) fputs(show ? "\033[?25h" : "\033[?25l", stdout);
}

static void term_restore(void) {
    if (use_vt) fputs("\033[0m\033[?25h", stdout);
    fflush(stdout);
    platform_restore();
}

static void term_init(int want_color) {
    static char out_buf[1 << 16];

    if (term_inited) return;
    term_inited = 1;
    platform_init();
    use_color = use_vt && want_color && !getenv("NO_COLOR");
    /* Whole frames are written at once; this is what stops the flicker. */
    setvbuf(stdout, out_buf, _IOFBF, sizeof(out_buf));
    atexit(term_restore);
}

static void screen_clear(void) {
    if (use_vt) {
        fputs("\033[H\033[2J", stdout);
        return;
    }
#ifdef _WIN32
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        COORD home = {0, 0};
        DWORD cells, written;

        fflush(stdout);
        if (GetConsoleScreenBufferInfo(con_out, &info)) {
            cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
            FillConsoleOutputCharacterA(con_out, ' ', cells, home, &written);
            FillConsoleOutputAttribute(con_out, info.wAttributes, cells, home, &written);
            SetConsoleCursorPosition(con_out, home);
        }
    }
#endif
}

/* Frames overwrite the previous one in place instead of blanking the screen. */
static void frame_begin(void) {
    if (use_vt) fputs("\033[H", stdout);
    else screen_clear();
}

static void frame_eol(void) {
    if (use_vt) fputs("\033[K", stdout);
    putchar('\n');
}

static void frame_end(void) {
    if (use_vt) fputs("\033[J", stdout);
    fflush(stdout);
}

static void style(const char *code) {
    if (use_color) fputs(code, stdout);
}

static void style_reset(void) {
    if (use_color) fputs("\033[0m", stdout);
}

/* ------------------------------------------------------------------ */
/* Line input                                                          */
/* ------------------------------------------------------------------ */

/* Returns 0 on end of input. Blank or invalid input keeps *value. */
static int prompt_int(const char *prompt, int *value) {
    char line[64];
    char *end;
    long v;

    fputs(prompt, stdout);
    if (!read_line(line, sizeof(line))) return 0;
    v = strtol(line, &end, 10);
    if (end != line && v >= INT_MIN && v <= INT_MAX) *value = (int)v;
    return 1;
}

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Commas would break the score file; control characters the screen. */
static void sanitize_name(char *name) {
    size_t i, start = 0, len;

    for (i = 0; name[i]; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == ',' || c < 0x20 || c == 0x7F) name[i] = ' ';
    }
    while (name[start] == ' ') start++;
    memmove(name, name + start, strlen(name + start) + 1);
    len = strlen(name);
    if (len >= MAX_NAME) {
        len = MAX_NAME - 1;
        /* Don't cut a UTF-8 character in half. */
        while (len > 0 && ((unsigned char)name[len] & 0xC0) == 0x80) len--;
        name[len] = '\0';
    }
    while (len > 0 && name[len - 1] == ' ') name[--len] = '\0';
    if (len == 0) strcpy(name, "ANONYMOUS");
}

/* ------------------------------------------------------------------ */
/* Board                                                               */
/* ------------------------------------------------------------------ */

static void *alloc_or_die(size_t size) {
    void *p = calloc(1, size);
    if (!p) {
        fputs("Out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static void board_init(Board *b, int rows, int cols, int mines) {
    int r;

    b->rows = rows;
    b->cols = cols;
    b->mines = mines;
    b->goal_r = -1;
    b->goal_c = -1;
    b->finished = 0;
    b->cells = (Cell **)alloc_or_die((size_t)rows * sizeof(Cell *));
    for (r = 0; r < rows; r++) {
        b->cells[r] = (Cell *)alloc_or_die((size_t)cols * sizeof(Cell));
    }
}

static void board_free(Board *b) {
    int r;

    for (r = 0; r < b->rows; r++) free(b->cells[r]);
    free(b->cells);
    b->cells = NULL;
}

static void board_clear(Board *b) {
    int r;

    for (r = 0; r < b->rows; r++) {
        memset(b->cells[r], 0, (size_t)b->cols * sizeof(Cell));
    }
}

static int in_bounds(const Board *b, int r, int c) {
    return r >= 0 && r < b->rows && c >= 0 && c < b->cols;
}

/*
 * Puts b->mines mines on cells that aren't reserved, then fills in the
 * adjacent counts. A partial shuffle always finishes, however dense the
 * board. Returns 0 if there are fewer free cells than mines.
 */
static int board_place_mines(Board *b) {
    int total = b->rows * b->cols;
    int *free_cells = (int *)alloc_or_die((size_t)total * sizeof(int));
    int free_count = 0;
    int i, r, c, dr, dc;

    for (i = 0; i < total; i++) {
        if (!b->cells[i / b->cols][i % b->cols].reserved) free_cells[free_count++] = i;
    }
    if (free_count < b->mines) {
        free(free_cells);
        return 0;
    }
    for (i = 0; i < b->mines; i++) {
        int j = i + rng_below(free_count - i);
        int pick = free_cells[j];
        free_cells[j] = free_cells[i];
        free_cells[i] = pick;
        b->cells[pick / b->cols][pick % b->cols].is_mine = 1;
    }
    free(free_cells);

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            int count = 0;
            for (dr = -1; dr <= 1; dr++) {
                for (dc = -1; dc <= 1; dc++) {
                    if (in_bounds(b, r + dr, c + dc) && b->cells[r + dr][c + dc].is_mine) count++;
                }
            }
            b->cells[r][c].adjacent_mines = b->cells[r][c].is_mine ? 0 : count;
        }
    }
    return 1;
}

static void board_reveal_mines(Board *b) {
    int r, c;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            if (b->cells[r][c].is_mine) b->cells[r][c].is_revealed = 1;
        }
    }
}

static int board_count_flags(const Board *b) {
    int r, c, count = 0;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            if (b->cells[r][c].mark == MARK_FLAG) count++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

/* Cell width that fits the terminal: 3 = " x ", 2 = "x ", 1 = "x". The
 * narrow forms show the cursor in reverse video, so they need ANSI. */
static int cell_width(int cols, int term_w) {
    if (!use_vt || term_w <= 0 || cols * 3 + 2 <= term_w) return 3;
    if (cols * 2 + 2 <= term_w) return 2;
    return 1;
}

static void print_border(int cols, int width, char fill) {
    int i;

    putchar('+');
    for (i = 0; i < cols * width; i++) putchar(fill);
    putchar('+');
}

static void print_cell(const Board *b, int r, int c) {
    const Cell *cell = &b->cells[r][c];
    int is_goal = (r == b->goal_r && c == b->goal_c);
    static const char *const number_style[9] = {
        "", "\033[94m", "\033[32m", "\033[91m", "\033[35m",
        "\033[31m", "\033[36m", "\033[37m", "\033[90m"
    };

    if (cell->exploded) {
        style("\033[1;91m");
        putchar('X');
    } else if (b->finished && cell->mark == MARK_FLAG && !cell->is_mine) {
        style("\033[1;91m");
        putchar('x');
    } else if (cell->mark == MARK_FLAG) {
        style("\033[1;93m");
        putchar('F');
    } else if (cell->mark == MARK_QUESTION) {
        style("\033[1;95m");
        putchar('?');
    } else if (is_goal) {
        style("\033[1;92m");
        putchar('G');
    } else if (!cell->is_revealed) {
        style("\033[90m");
        fputs(use_ascii ? "#" : "\xE2\x96\xA0", stdout); /* U+25A0 */
    } else if (cell->is_mine) {
        style("\033[91m");
        putchar('*');
    } else if (cell->adjacent_mines > 0) {
        style(number_style[cell->adjacent_mines]);
        if (cell->visited && use_vt) fputs("\033[4m", stdout);
        putchar('0' + cell->adjacent_mines);
        if (cell->visited && use_vt) fputs("\033[24m", stdout);
    } else {
        putchar(cell->visited ? '.' : ' ');
    }
    style_reset();
}

/* Draws rows top..top+view-1; ^ and v in the border mean there is more. */
static void board_print(const Board *b, int cursor_r, int cursor_c, int top, int view, int width) {
    int r, c;

    print_border(b->cols, width, top > 0 ? '^' : '-');
    frame_eol();
    for (r = top; r < top + view; r++) {
        putchar('|');
        for (c = 0; c < b->cols; c++) {
            int is_cursor = (r == cursor_r && c == cursor_c);
            if (width == 3) {
                putchar(is_cursor ? '[' : ' ');
                print_cell(b, r, c);
                putchar(is_cursor ? ']' : ' ');
            } else {
                if (is_cursor) fputs("\033[7m", stdout);
                print_cell(b, r, c);
                if (is_cursor) fputs("\033[0m", stdout);
                if (width == 2) putchar(' ');
            }
        }
        putchar('|');
        frame_eol();
    }
    print_border(b->cols, width, top + view < b->rows ? 'v' : '-');
    frame_eol();
}

/*
 * Draws the board below a header of header_lines lines, following the cursor
 * when it doesn't fit on screen. Returns 0 if it is too wide even at one
 * column per cell.
 */
static int board_print_fit(const Board *b, int cursor_r, int cursor_c, int header_lines) {
    int term_w, term_h, view, top = 0, width;

    term_size(&term_w, &term_h);
    width = cell_width(b->cols, term_w);
    view = b->rows;
    if (use_vt && term_h > 0) {
        int room = term_h - header_lines - 3;
        if (room < 1) room = 1;
        if (room < view) view = room;
    }
    if (view < b->rows && cursor_r >= 0) {
        top = clamp(cursor_r - view / 2, 0, b->rows - view);
    }
    board_print(b, cursor_r, cursor_c, top, view, width);
    return !(use_vt && term_w > 0 && b->cols * width + 2 > term_w);
}

/* The whole board, for the end-of-game screen. */
static void board_print_final(const Board *b, int cursor_r, int cursor_c) {
    int term_w, term_h;

    term_size(&term_w, &term_h);
    board_print(b, cursor_r, cursor_c, 0, b->rows, cell_width(b->cols, term_w));
}

/* ------------------------------------------------------------------ */
/* Game clock (pausable)                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    long long started;
    long long paused_at;
    long long paused_total;
    int running;
} GameTimer;

static void timer_start(GameTimer *t) {
    t->started = ms_now();
    t->paused_at = 0;
    t->paused_total = 0;
    t->running = 1;
}

static void timer_pause(GameTimer *t) {
    t->paused_at = ms_now();
}

static void timer_resume(GameTimer *t) {
    t->paused_total += ms_now() - t->paused_at;
    t->paused_at = 0;
}

static int timer_seconds(const GameTimer *t) {
    long long now;

    if (!t->running) return 0;
    now = t->paused_at ? t->paused_at : ms_now();
    return (int)((now - t->started - t->paused_total) / 1000);
}

/* Blocks until P is pressed again. Returns 0 if input closed. */
static int pause_screen(GameTimer *t, const char *title) {
    int key;

    timer_pause(t);
    screen_clear();
    printf("%s\n\nPAUSED - the board is hidden.\nPress P to resume.\n", title);
    fflush(stdout);
    do {
        key = key_poll(-1);
        if (key == K_EOF) return 0;
    } while (key != 'p' && key != 'P' && key != K_CTRL_C);
    timer_resume(t);
    screen_clear();
    return 1;
}

/* ------------------------------------------------------------------ */
/* High scores                                                         */
/* ------------------------------------------------------------------ */

static int today_utc(void) {
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);

    if (!t) return 0;
    return (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;
}

static int score_better(const HighScore *a, const HighScore *b) {
    if (a->points != b->points) return a->points < b->points;
    return a->seconds < b->seconds;
}

static void scores_sort(HighScore *scores, int count) {
    int i, j;

    for (i = 1; i < count; i++) {
        HighScore item = scores[i];
        for (j = i; j > 0 && score_better(&item, &scores[j - 1]); j--) {
            scores[j] = scores[j - 1];
        }
        scores[j] = item;
    }
}

/* scores is sorted. Entry and ranking share one rule, score_better. */
static int score_qualifies(const HighScore *scores, int count, const HighScore *entry) {
    return count < MAX_SCORES || score_better(entry, &scores[count - 1]);
}

/* scores holds MAX_SCORES items; the worst one drops off when it is full. */
static void score_insert(HighScore *scores, int *count, const HighScore *entry) {
    if (*count < MAX_SCORES) (*count)++;
    scores[*count - 1] = *entry;
    scores_sort(scores, *count);
}

/* suffix == NULL gives the pre-2.0 file shared by every difficulty. */
static void score_path(const GameDef *game, const char *suffix, char *path, size_t size) {
    const char *home;
    const char *dot = "";
    const char *sep = "/";

#ifdef _WIN32
    sep = "\\";
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    home = getenv("HOME");
    if (home) dot = ".";
#endif
    if (!home) {
        home = ".";
        sep = "/";
    }
    if (suffix) {
        snprintf(path, size, "%s%s%sminesweeper_%s_scores.%s.txt", home, sep, dot, game->key, suffix);
    } else {
        snprintf(path, size, "%s%s%sminesweeper_%s_scores.txt", home, sep, dot, game->key);
    }
}

/* Parses "name,seconds,rows,cols,mines[,points[,date]]". */
static int score_parse(const char *line, HighScore *e) {
    int n;

    memset(e, 0, sizeof(*e));
    n = sscanf(line, "%31[^,\n],%d,%d,%d,%d,%d,%d", e->name, &e->seconds, &e->rows, &e->cols,
               &e->mines, &e->points, &e->date);
    if (n < 5 || e->seconds < 0 || e->rows <= 0 || e->cols <= 0 || e->mines <= 0) return 0;
    if (n == 5) e->points = e->seconds;
    return 1;
}

/* Does a score from the old shared file belong to this difficulty? */
static int legacy_matches(const GameDef *game, const Preset *p, const HighScore *e) {
    int i;

    if (p->is_daily) return 0;
    if (!p->is_custom) return e->rows == p->rows && e->cols == p->cols && e->mines == p->mines;
    for (i = 0; i < game->preset_count; i++) {
        const Preset *o = &game->presets[i];
        if (!o->is_custom && !o->is_daily && e->rows == o->rows && e->cols == o->cols &&
            e->mines == o->mines) {
            return 0;
        }
    }
    return 1;
}

static void scores_load(const GameDef *game, const Preset *p, HighScore *scores, int *count) {
    char path[MAX_PATH_LEN];
    char line[256];
    int legacy = 0;
    int today = today_utc();
    FILE *f;

    *count = 0;
    score_path(game, p->id, path, sizeof(path));
    f = fopen(path, "r");
    if (!f) { /* first run after upgrading: pick up the old scores */
        score_path(game, NULL, path, sizeof(path));
        f = fopen(path, "r");
        legacy = 1;
    }
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        HighScore e;
        if (!score_parse(line, &e)) continue;
        if (legacy && !legacy_matches(game, p, &e)) continue;
        if (p->is_daily && e.date != today) continue;
        if (*count < MAX_SCORES) {
            score_insert(scores, count, &e);
        } else if (score_better(&e, &scores[*count - 1])) {
            score_insert(scores, count, &e);
        }
    }
    fclose(f);
}

static void scores_save(const GameDef *game, const Preset *p, const HighScore *scores, int count) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int i;

    score_path(game, p->id, path, sizeof(path));
    f = fopen(path, "w");
    if (!f) return;
    for (i = 0; i < count; i++) {
        fprintf(f, "%s,%d,%d,%d,%d,%d,%d\n", scores[i].name, scores[i].seconds, scores[i].rows,
                scores[i].cols, scores[i].mines, scores[i].points, scores[i].date);
    }
    fclose(f);
}

static void scores_print(const Preset *p, const HighScore *scores, int count, int show_points) {
    int i;

    printf("--- %s ---\n", p->label);
    if (count == 0) printf("(no scores yet)\n");
    for (i = 0; i < count; i++) {
        printf("%d. %s - %d sec", i + 1, scores[i].name, scores[i].seconds);
        if (show_points) printf(", %d points", scores[i].points);
        if (p->is_custom) {
            printf(" (%dx%d, %d mines)", scores[i].rows, scores[i].cols, scores[i].mines);
        }
        putchar('\n');
    }
}

/* Records a win. The board must be final. Returns 0 if input closed. */
static int scores_record(const GameDef *game, const Preset *p, const HighScore *result,
                         int show_points) {
    HighScore scores[MAX_SCORES];
    HighScore entry = *result;
    char name[128];
    int count = 0;

    scores_load(game, p, scores, &count);
    if (!score_qualifies(scores, count, &entry)) return 1;

    input_flush();
    printf("\nNew high score!\nEnter your name: ");
    if (!read_line(name, sizeof(name))) return 0;
    sanitize_name(name);
    memcpy(entry.name, name, strlen(name) + 1);
    score_insert(scores, &count, &entry);
    scores_save(game, p, scores, count);
    putchar('\n');
    scores_print(p, scores, count, show_points);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Menu and command line                                               */
/* ------------------------------------------------------------------ */

static void menu_show_all_scores(const GameDef *game, int show_points) {
    char line[16];
    int i;

    screen_clear();
    printf("=== HIGH SCORES (%s) ===\n", game->title);
    for (i = 0; i < game->preset_count; i++) {
        HighScore scores[MAX_SCORES];
        int count = 0;
        scores_load(game, &game->presets[i], scores, &count);
        scores_print(&game->presets[i], scores, count, show_points);
    }
    printf("\nPress Enter to go back. ");
    read_line(line, sizeof(line));
}

/*
 * Fills *custom when a custom board is picked. Returns the preset to play,
 * or NULL to quit (Q or end of input).
 */
static const Preset *menu_choose(const GameDef *game, Preset *custom, int show_points) {
    for (;;) {
        char line[16];
        int i, choice;

        screen_clear();
        printf("===== MINESWEEPER (%s) =====\n\nChoose a difficulty:\n", game->title);
        for (i = 0; i < game->preset_count; i++) {
            const Preset *p = &game->presets[i];
            HighScore scores[MAX_SCORES];
            int count = 0;

            scores_load(game, p, scores, &count);
            if (p->is_custom) {
                printf("%d. %s", i + 1, p->label);
            } else {
                printf("%d. %s (%dx%d, %d mines)", i + 1, p->label, p->rows, p->cols, p->mines);
            }
            if (count > 0) {
                printf(" - best: %s, %d sec", scores[0].name, scores[0].seconds);
            }
            putchar('\n');
        }
        printf("S. High scores\nQ. Quit\n\nSelect option (1-%d): ", game->preset_count);
        if (!read_line(line, sizeof(line))) return NULL;

        if (line[0] == 'q' || line[0] == 'Q') return NULL;
        if (line[0] == 's' || line[0] == 'S') {
            menu_show_all_scores(game, show_points);
            continue;
        }
        choice = atoi(line);
        if (choice < 1 || choice > game->preset_count) continue;

        if (game->presets[choice - 1].is_custom) {
            char prompt[64];
            int rows = 9, cols = 9, mines = 10, limit;

            *custom = game->presets[choice - 1];
            snprintf(prompt, sizeof(prompt), "Enter rows (%d-%d): ", MIN_ROWS, MAX_ROWS);
            if (!prompt_int(prompt, &rows)) return NULL;
            snprintf(prompt, sizeof(prompt), "Enter cols (%d-%d): ", MIN_COLS, MAX_COLS);
            if (!prompt_int(prompt, &cols)) return NULL;
            rows = clamp(rows, MIN_ROWS, MAX_ROWS);
            cols = clamp(cols, MIN_COLS, MAX_COLS);
            limit = game->max_mines(rows, cols);
            snprintf(prompt, sizeof(prompt), "Enter mines (1-%d): ", limit);
            if (!prompt_int(prompt, &mines)) return NULL;
            custom->rows = rows;
            custom->cols = cols;
            custom->mines = clamp(mines, 1, limit);
            return custom;
        }
        return &game->presets[choice - 1];
    }
}

/* Forced seeds replay one board; daily seeds come from the UTC date. */
static unsigned long choose_seed(const Options *opt, const Preset *p) {
    unsigned long mix;

    if (opt->has_seed) return opt->seed & 0xFFFFFFFFUL;
    if (p->is_daily) return (unsigned long)today_utc();
    mix = (unsigned long)time(NULL) * 2654435761UL;
    mix ^= (unsigned long)ms_now();
    return mix & 0xFFFFFFFFUL;
}

static int ask_play_again(void) {
    char line[16];

    input_flush();
    printf("\nDo you want to play again? (y/n): ");
    return read_line(line, sizeof(line)) && (line[0] == 'y' || line[0] == 'Y');
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n"
           "  --seed N     play the board with this seed (scores are not saved)\n"
           "  --no-color   plain monochrome output (NO_COLOR is also honoured)\n"
           "  --ascii      use only ASCII characters on the board\n"
           "  --self-test  run the built-in checks and exit\n"
           "  --version    show the version\n"
           "  --help       show this help\n",
           prog);
}

/* Handles the command line and sets up the terminal. */
static void parse_options(int argc, char **argv, Options *opt) {
    int want_color = 1;
    int i;

    memset(opt, 0, sizeof(*opt));
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            char *end;
            opt->seed = strtoul(argv[++i], &end, 10);
            opt->has_seed = 1;
            if (*end || end == argv[i]) {
                fprintf(stderr, "Invalid seed: %s\n", argv[i]);
                exit(2);
            }
        } else if (strcmp(argv[i], "--no-color") == 0) {
            want_color = 0;
        } else if (strcmp(argv[i], "--ascii") == 0) {
            use_ascii = 1;
        } else if (strcmp(argv[i], "--self-test") == 0) {
            opt->self_test = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("minesweeper-cli %s\n", MS_VERSION);
            exit(0);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(2);
        }
    }
    if (!opt->self_test) term_init(want_color);
}

/* ------------------------------------------------------------------ */
/* Self-test helpers                                                   */
/* ------------------------------------------------------------------ */

static int test_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            test_failures++;                                               \
        }                                                                  \
    } while (0)

static int board_count_mines(const Board *b) {
    int r, c, count = 0;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            if (b->cells[r][c].is_mine) count++;
        }
    }
    return count;
}

static int board_counts_correct(const Board *b) {
    int r, c, dr, dc;

    for (r = 0; r < b->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            int count = 0;
            if (b->cells[r][c].is_mine) continue;
            for (dr = -1; dr <= 1; dr++) {
                for (dc = -1; dc <= 1; dc++) {
                    if (in_bounds(b, r + dr, c + dc) && b->cells[r + dr][c + dc].is_mine) count++;
                }
            }
            if (count != b->cells[r][c].adjacent_mines) return 0;
        }
    }
    return 1;
}

static void self_test_common(void) {
    HighScore scores[MAX_SCORES];
    HighScore e;
    char name[128];
    int count = 0, i;

    /* The generator is defined by us, so its output must never change. */
    rng_seed(12345);
    i = rng_below(1000);
    rng_seed(12345);
    CHECK(i == rng_below(1000));
    for (i = 0; i < 10000; i++) {
        int v = rng_below(7);
        CHECK(v >= 0 && v < 7);
    }

    /* Filling a full table must not overflow and must keep the best five. */
    memset(&e, 0, sizeof(e));
    for (i = 10; i > 0; i--) {
        e.points = i;
        e.seconds = i;
        if (score_qualifies(scores, count, &e)) score_insert(scores, &count, &e);
        CHECK(count <= MAX_SCORES);
    }
    CHECK(count == MAX_SCORES);
    CHECK(scores[0].points == 1 && scores[MAX_SCORES - 1].points == 5);
    e.points = 5;
    e.seconds = 5;
    CHECK(!score_qualifies(scores, count, &e)); /* ties don't displace */
    e.seconds = 4;
    CHECK(score_qualifies(scores, count, &e)); /* better time on equal points */

    strcpy(name, "");
    sanitize_name(name);
    CHECK(strcmp(name, "ANONYMOUS") == 0);
    strcpy(name, "  a,b  ");
    sanitize_name(name);
    CHECK(strcmp(name, "a b") == 0);
    memset(name, 'x', 100);
    name[100] = '\0';
    sanitize_name(name);
    CHECK(strlen(name) == MAX_NAME - 1);

    CHECK(score_parse("bob,12,9,9,10\n", &e) && e.points == 12 && e.date == 0);
    CHECK(score_parse("bob b,12,9,9,10,99\n", &e) && e.points == 99);
    CHECK(score_parse("bob,12,9,9,10,99,20261102\n", &e) && e.date == 20261102);
    CHECK(!score_parse("garbage\n", &e));
}

static int self_test_result(const char *what) {
    if (test_failures) {
        printf("%s: %d check(s) failed\n", what, test_failures);
        return 1;
    }
    printf("%s: all checks passed\n", what);
    return 0;
}

#endif /* MINESWEEPER_COMMON_H */
