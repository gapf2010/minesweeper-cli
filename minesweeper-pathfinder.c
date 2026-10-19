#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_RIGHT 1003
#define KEY_LEFT 1004
#define MAX_SCORES 5
#define MAX_NAME 32

typedef struct {
    int is_mine;
    int is_revealed;
    int visited;
    int adjacent_mines;
} Cell;

typedef struct {
    char name[MAX_NAME];
    int seconds;
    int rows;
    int cols;
    int mines;
} HighScore;

// Terminal helpers for Linux/macOS vs Windows
#ifndef _WIN32
static struct termios orig_termios;

void reset_terminal_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void set_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int get_key(void) {
    int c = getchar();
    if (c == '\033') {
        int next1 = getchar();
        int next2 = getchar();
        if (next1 == '[') {
            switch (next2) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
        }
    }
    return c;
}
#else
void set_raw_mode(void) {} // Windows _getch handles this natively
void reset_terminal_mode(void) {}

int get_key(void) {
    int c = _getch();
    if (c == 0 || c == 224) {
        int next = _getch();
        switch (next) {
            case 72: return KEY_UP;
            case 80: return KEY_DOWN;
            case 77: return KEY_RIGHT;
            case 75: return KEY_LEFT;
        }
    }
    return c;
}
#endif

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
}

void get_highscore_path(char *path, size_t max_len) {
    char *home = NULL;
#ifdef _WIN32
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
    if (home) {
        snprintf(path, max_len, "%s\\minesweeper_path_scores.txt", home);
    } else {
        strncpy(path, "minesweeper_path_scores.txt", max_len);
    }
#else
    home = getenv("HOME");
    if (home) {
        snprintf(path, max_len, "%s/.minesweeper_path_scores.txt", home);
    } else {
        strncpy(path, "minesweeper_path_scores.txt", max_len);
    }
#endif
}

void load_highscores(HighScore scores[], int *count) {
    char path[512];
    get_highscore_path(path, sizeof(path));
    *count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (*count < MAX_SCORES && fscanf(f, "%31s %d %d %d %d", 
           scores[*count].name, 
           &scores[*count].seconds, 
           &scores[*count].rows, 
           &scores[*count].cols, 
           &scores[*count].mines) == 5) {
        (*count)++;
    }
    fclose(f);
}

void save_highscores(HighScore scores[], int count) {
    char path[512];
    get_highscore_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d %d %d %d\n", scores[i].name, scores[i].seconds, scores[i].rows, scores[i].cols, scores[i].mines);
    }
    fclose(f);
}

void update_highscores(int seconds, int rows, int cols, int mines) {
    HighScore scores[MAX_SCORES];
    int count = 0;
    load_highscores(scores, &count);

    if (count == MAX_SCORES && seconds >= scores[count - 1].seconds) {
        return;
    }

    char name[MAX_NAME];
    printf("\nNew Fast Time! (%d seconds)\nEnter your name (no spaces): ", seconds);
    fflush(stdout);
    
#ifndef _WIN32
    reset_terminal_mode();
#endif
    if (scanf("%31s", name) != 1) {
        strcpy(name, "ANONYMOUS");
    }

    scores[count].seconds = seconds;
    scores[count].rows = rows;
    scores[count].cols = cols;
    scores[count].mines = mines;
    strncpy(scores[count].name, name, MAX_NAME);
    count++;

    // Sort ascending (lowest seconds first)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (scores[j].seconds < scores[i].seconds) {
                HighScore temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }
    if (count > MAX_SCORES) count = MAX_SCORES;
    save_highscores(scores, count);
}

void display_highscores(void) {
    HighScore scores[MAX_SCORES];
    int count = 0;
    load_highscores(scores, &count);
    printf("\n=== TOP FASTEST TIMES (PATHFINDER) ===\n");
    if (count == 0) {
        printf("No high scores recorded yet.\n");
    } else {
        for (int i = 0; i < count; i++) {
            printf("%d. %s - %d sec (%dx%d board, %d mines)\n", 
                   i + 1, scores[i].name, scores[i].seconds, scores[i].rows, scores[i].cols, scores[i].mines);
        }
    }
    printf("======================================\n");
}

void place_mines(Cell **board, int rows, int cols, int mines) {
    int placed = 0;
    while (placed < mines) {
        int r = rand() % rows;
        int c = rand() % cols;
        // Keep start (0,0) and goal (rows-1, cols-1) safe from mines
        if ((r == 0 && c == 0) || (r == rows - 1 && c == cols - 1)) continue;
        if (!board[r][c].is_mine) {
            board[r][c].is_mine = 1;
            placed++;
        }
    }

    // Calculate adjacent numbers
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (board[r][c].is_mine) continue;
            int count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && board[nr][nc].is_mine) {
                        count++;
                    }
                }
            }
            board[r][c].adjacent_mines = count;
        }
    }
}

void step_cell(Cell **board, int r, int c, int *game_over, int *won, int rows, int cols) {
    board[r][c].is_revealed = 1;
    if (board[r][c].is_mine) {
        *game_over = 1;
        return;
    }
    board[r][c].visited = 1;
    if (r == rows - 1 && c == cols - 1) {
        *won = 1;
    }
}

void print_board(Cell **board, int rows, int cols, int player_r, int player_c) {
    // Top border
    printf("+");
    for (int c = 0; c < cols * 3; c++) printf("-");
    printf("+\n");

    for (int r = 0; r < rows; r++) {
        printf("|");
        for (int c = 0; c < cols; c++) {
            int is_player = (r == player_r && c == player_c);
            printf("%s", is_player ? "@" : " ");

            if (!board[r][c].is_revealed) {
                printf(".");
            } else if (board[r][c].is_mine) {
                printf("*");
            } else if (board[r][c].adjacent_mines > 0) {
                printf("%d", board[r][c].adjacent_mines);
            } else {
                printf(" ");
            }

            printf("%s", is_player ? "@" : " ");
        }
        printf("|\n");
    }

    // Bottom border
    printf("+");
    for (int c = 0; c < cols * 3; c++) printf("-");
    printf("+\n");
}

int main() {
    srand((unsigned int)time(NULL));
    char play_again = 'y';

    while (play_again == 'y' || play_again == 'Y') {
        clear_screen();

        int rows = 9, cols = 9, mines = 10;
        display_highscores();
        printf("\nChoose difficulty or custom board:\n");
        printf("1. Beginner (9x9, 10 mines)\n");
        printf("2. Intermediate (16x16, 40 mines)\n");
        printf("3. Custom\n");
        printf("Select option (1-3): ");
        
        int choice = 1;
        if (scanf("%d", &choice) != 1) choice = 1;

        if (choice == 2) {
            rows = 16; cols = 16; mines = 40;
        } else if (choice == 3) {
            printf("Enter rows (5-30): ");
            scanf("%d", &rows);
            printf("Enter cols (5-40): ");
            scanf("%d", &cols);
            printf("Enter mines: ");
            scanf("%d", &mines);
            if (rows < 5) rows = 5; if (rows > 30) rows = 30;
            if (cols < 5) cols = 5; if (cols > 40) cols = 40;
            if (mines < 1) mines = 1;
            if (mines >= rows * cols) mines = rows * cols - 2;
        }

        // Allocate board
        Cell **board = (Cell **)malloc(rows * sizeof(Cell *));
        for (int i = 0; i < rows; i++) {
            board[i] = (Cell *)malloc(cols * sizeof(Cell));
            memset(board[i], 0, cols * sizeof(Cell));
        }

        place_mines(board, rows, cols, mines);

        int player_r = 0, player_c = 0;
        int game_over = 0;
        int won = 0;

        time_t start_time = time(NULL);

        // Step on starting position immediately
        step_cell(board, player_r, player_c, &game_over, &won, rows, cols);

        set_raw_mode();

        while (!game_over && !won) {
            time_t current_time = time(NULL);
            int elapsed_seconds = (int)(current_time - start_time);

            clear_screen();
            printf("===== MINESWEEPER (Pathfinder Edition) =====\n");
            printf("Goal: Reach bottom-right [%d,%d] from top-left [0,0]\n", rows - 1, cols - 1);
            printf("Controls: Arrows=Move | [q]=Up-L [e]=Up-R [z]=Down-L [c]=Down-R | [Q]=Quit\n");
            printf("Time: %d sec | Mines: %d\n\n", elapsed_seconds, mines);

            print_board(board, rows, cols, player_r, player_c);

            int key = get_key();
            if (key == 'Q') {
                game_over = 1;
                break;
            } else if (key == KEY_UP) {
                if (player_r > 0) player_r--;
            } else if (key == KEY_DOWN) {
                if (player_r < rows - 1) player_r++;
            } else if (key == KEY_LEFT) {
                if (player_c > 0) player_c--;
            } else if (key == KEY_RIGHT) {
                if (player_c < cols - 1) player_c++;
            } else if (key == 'q') {
                if (player_r > 0 && player_c > 0) {
                    player_r--;
                    player_c--;
                }
            } else if (key == 'e') {
                if (player_r > 0 && player_c < cols - 1) {
                    player_r--;
                    player_c++;
                }
            } else if (key == 'z') {
                if (player_r < rows - 1 && player_c > 0) {
                    player_r++;
                    player_c--;
                }
            } else if (key == 'c') {
                if (player_r < rows - 1 && player_c < cols - 1) {
                    player_r++;
                    player_c++;
                }
            }

            // Step onto the new position immediately
            step_cell(board, player_r, player_c, &game_over, &won, rows, cols);
        }

        time_t end_time = time(NULL);
        int total_seconds = (int)(end_time - start_time);

        clear_screen();
        reset_terminal_mode();

        // Reveal all mines upon game over
        if (game_over) {
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    if (board[r][c].is_mine) board[r][c].is_revealed = 1;
                }
            }
        }

        // Print win/loss messages and final status ABOVE the board
        if (won) {
            printf("\nCONGRATULATIONS! YOU REACHED THE GOAL IN %d SECONDS!\n\n", total_seconds);
        } else if (game_over) {
            printf("\nBOOM! YOU HIT A MINE! Game Over.\n\n");
        } else {
            printf("\nGame Ended.\n\n");
        }

        print_board(board, rows, cols, player_r, player_c);

        // Cleanup board memory
        for (int i = 0; i < rows; i++) {
            free(board[i]);
        }
        free(board);

        // Ask to play again
        printf("\nDo you want to play again? (y/n): ");
        if (scanf(" %c", &play_again) != 1) {
            break;
        }
    }

    printf("Thanks for playing!\n");
    return 0;
}
