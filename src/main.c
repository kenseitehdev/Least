#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#define MAX_LINES 20000
#define MAX_LINE_LENGTH 10240
#define COMMAND_BUFFER_SIZE 256
#define SEARCH_BUFFER_SIZE 256
char *filename = NULL;
char *lines[MAX_LINES];
int line_count = 0;
int current_line = 0;
char command_buffer[COMMAND_BUFFER_SIZE] = {0};
char search_buffer[SEARCH_BUFFER_SIZE] = {0};
int command_mode = 0;
int search_mode = 0;
struct SyntaxPattern {
    char *pattern;
    int color_pair;
};
struct SyntaxPattern syntax_patterns[] = {
    {"#include", 1},
    {"#define", 1},
    {"#ifdef", 1},
    {"#ifndef", 1},
    {"#endif", 1},
    {"int", 2},
    {"char", 2},
    {"void", 2},
    {"return", 2},
    {"for", 2},
    {"while", 2},
    {"if", 2},
    {"else", 2},
    {"struct", 2},
    {"enum", 2},
    {"typedef", 2},
    {"static", 2},
    {"const", 2},
    {"size_t", 3},
    {"uint32_t", 3},
    {"int32_t", 3},
    {"bool", 3},
    {"float", 3},
    {"double", 3},
    {NULL, 0}
};
void handle_signal(int sig) {
    (void)sig;
}
int load_file(const char *fname) {
    FILE *file = fopen(fname, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }
    filename = strdup(fname);
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (line_count < MAX_LINES) {
            lines[line_count] = strdup(buffer);
            line_count++;
        }
    }
    fclose(file);
    return 0;
}
void highlight_syntax(char *line) {
    int in_string = 0;
    int in_char = 0;
    int in_multiline_comment = 0;
    int in_single_comment = 0;
    char prev_char = '\0';
    for (char *pos = line; *pos != '\0'; pos++) {
        if (!in_string && !in_char) {
            if (pos[0] == '/' && pos[1] == '*' && !in_single_comment) {
                in_multiline_comment = 1;
                attron(COLOR_PAIR(4));  
                printw("/*");
                pos++;
                continue;
            }
            if (pos[0] == '*' && pos[1] == '/' && in_multiline_comment) {
                in_multiline_comment = 0;
                printw("*/");
                attroff(COLOR_PAIR(4));
                pos++;
                continue;
            }
            if (pos[0] == '/' && pos[1] == '/' && !in_multiline_comment) {
                in_single_comment = 1;
                attron(COLOR_PAIR(4));
            }
        }
        if (!in_multiline_comment && !in_single_comment) {
            if (*pos == '"' && prev_char != '\\') {
                in_string = !in_string;
                attron(COLOR_PAIR(5));  
            }
            if (*pos == '\'' && prev_char != '\\') {
                in_char = !in_char;
                attron(COLOR_PAIR(5));
            }
        }
        if (in_multiline_comment || in_single_comment) {
            attron(COLOR_PAIR(4));
            printw("%c", *pos);
        } else if (in_string || in_char) {
            attron(COLOR_PAIR(5));
            printw("%c", *pos);
        } else {
            int pattern_matched = 0;
            for (int i = 0; syntax_patterns[i].pattern != NULL; i++) {
                int len = strlen(syntax_patterns[i].pattern);
                if (strncmp(pos, syntax_patterns[i].pattern, len) == 0 &&
                    (pos == line || !isalnum(*(pos - 1))) &&
                    !isalnum(*(pos + len))) {
                    attron(COLOR_PAIR(syntax_patterns[i].color_pair));
                    printw("%.*s", len, pos);
                    attroff(COLOR_PAIR(syntax_patterns[i].color_pair));
                    pos += len - 1;
                    pattern_matched = 1;
                    break;
                }
            }
            if (!pattern_matched) {
                if (isdigit(*pos) || (*pos == '-' && isdigit(*(pos + 1)))) {
                    attron(COLOR_PAIR(6));  
                    printw("%c", *pos);
                    attroff(COLOR_PAIR(6));
                }
                else if (strchr("+-*/%=<>!&|^~", *pos)) {
                    attron(COLOR_PAIR(7));  
                    printw("%c", *pos);
                    attroff(COLOR_PAIR(7));
                }
                else {
                    printw("%c", *pos);
                }
            }
        }
        prev_char = *pos;
        if (*pos == '\n') {
            in_single_comment = 0;
            attroff(COLOR_PAIR(4));
        }
    }
}
char* run_fzf() {
    def_prog_mode();
    endwin();
    char buffer[MAX_LINE_LENGTH];
    FILE *fp;
    fp = popen("fzf", "w");
    if (fp == NULL) {
        perror("popen");
        return NULL;
    }
    for (int i = 0; i < line_count; i++) {
        fprintf(fp, "%s", lines[i]);
    }
    fclose(fp);
    fp = popen("fzf", "r");
    if (fp == NULL) {
        perror("popen");
        return NULL;
    }
    char* result = NULL;
    if (fgets(buffer, sizeof(buffer), fp)) {
        result = strdup(buffer);
    }
    fclose(fp);
    reset_prog_mode();
    refresh();
    return result;
}
void search_forward(const char* term) {
    if (!term || strlen(term) == 0) return;
    for (int i = current_line + 1; i < line_count; i++) {
        if (strstr(lines[i], term)) {
            current_line = i;
            return;
        }
    }
    for (int i = 0; i <= current_line; i++) {
        if (strstr(lines[i], term)) {
            current_line = i;
            return;
        }
    }
}
void search_backward(const char* term) {
    if (!term || strlen(term) == 0) return;
    for (int i = current_line - 1; i >= 0; i--) {
        if (strstr(lines[i], term)) {
            current_line = i;
            return;
        }
    }
    for (int i = line_count - 1; i >= current_line; i--) {
        if (strstr(lines[i], term)) {
            current_line = i;
            return;
        }
    }
}
void process_command() {
    if (strcmp(command_buffer, "q") == 0 || 
        strcmp(command_buffer, "quit") == 0) {
        endwin();
        exit(0);
    }
    else if (strncmp(command_buffer, "s/", 2) == 0) {
        search_mode = 1;
        strncpy(search_buffer, command_buffer + 2, SEARCH_BUFFER_SIZE - 1);
        search_forward(search_buffer);
    }
    command_mode = 0;
    command_buffer[0] = '\0';
}
void draw_status_bar() {
    int x, y;
    getmaxyx(stdscr, y, x);
    attron(COLOR_PAIR(8) | A_BOLD);
    mvhline(y - 2, 0, ' ', x);
    move(y - 2, 0);
    int percent = (int)((float)(current_line) / (line_count - 1) * 100);
    char status_message[MAX_LINE_LENGTH];
    snprintf(status_message, sizeof(status_message),
             " %s | Line %d/%d (%d%%) | ':' cmd | '/' search | 'f' fzf | 'n' next | 'N' prev | 'q' quit",
             filename, current_line + 1, line_count, percent);
    printw("%s", status_message);
    attroff(COLOR_PAIR(8) | A_BOLD);
    attron(COLOR_PAIR(9));
    mvhline(y - 1, 0, ' ', x);
    move(y - 1, 0);
    if (command_mode) {
        printw(":%s", command_buffer);
    } else if (search_mode) {
        printw("/%s", search_buffer);
    }
    attroff(COLOR_PAIR(9));
}
void display_lines() {
    clear();
    int max_display_lines = LINES - 2;  
    for (int i = 0; i < max_display_lines && current_line + i < line_count; i++) {
        move(i, 0);
        highlight_syntax(lines[current_line + i]);
    }
    draw_status_bar();
    refresh();
}
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_name>\n", argv[0]);
        return 1;
    }
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);     
        init_pair(2, COLOR_BLUE, -1);      
        init_pair(3, COLOR_CYAN, -1);      
        init_pair(4, COLOR_RED, -1);       
        init_pair(5, COLOR_YELLOW, -1);    
        init_pair(6, COLOR_MAGENTA, -1);   
        init_pair(7, COLOR_WHITE, -1);     
        init_pair(8, COLOR_BLACK, COLOR_WHITE);  
        init_pair(9, COLOR_GREEN, COLOR_BLACK);  
    }
    if (load_file(argv[1]) == -1) {
        endwin();
        return 1;
    }
    display_lines();
    int ch;
    while (1) {
        ch = getch();
        if (command_mode) {
            if (ch == '\n') {
                process_command();
            } else if (ch == 27) {  
                command_mode = 0;
                command_buffer[0] = '\0';
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                int len = strlen(command_buffer);
                if (len > 0) {
                    command_buffer[len - 1] = '\0';
                }
            } else if (isprint(ch)) {
                int len = strlen(command_buffer);
                if (len < COMMAND_BUFFER_SIZE - 1) {
                    command_buffer[len] = ch;
                    command_buffer[len + 1] = '\0';
                }
            }
        }
        else if (search_mode) {
            if (ch == '\n') {
                search_forward(search_buffer);
                search_mode = 0;
            } else if (ch == 27) {  
                search_mode = 0;
                search_buffer[0] = '\0';
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                int len = strlen(search_buffer);
                if (len > 0) {
                    search_buffer[len - 1] = '\0';
                }
            } else if (isprint(ch)) {
                int len = strlen(search_buffer);
                if (len < SEARCH_BUFFER_SIZE - 1) {
                    search_buffer[len] = ch;
                    search_buffer[len + 1] = '\0';
                }
            }
        }
        else {
            switch (ch) {
                case ':':
                    command_mode = 1;
                    break;
                case '/':
                    search_mode = 1;
                    search_buffer[0] = '\0';
                    break;
                case 'n':
                    if (strlen(search_buffer) > 0) {
                        search_forward(search_buffer);
                    }
                    break;
                case 'N':
                    if (strlen(search_buffer) > 0) {
                        search_backward(search_buffer);
                    }
                    break;
                case 'f':
                    {
                        char *selected_line = run_fzf();
                        if (selected_line != NULL) {
                            for (int i = 0; i < line_count; i++) {
                                if (strcmp(lines[i], selected_line) == 0) {
                                    current_line = i;
                                    break;
                                }
                            }
                            free(selected_line);
                        }
                    }
                    break;
                case KEY_DOWN:
                    if (current_line + LINES - 2 < line_count) {
                        current_line++;
                    }
                    break;
                case KEY_UP:
                    if (current_line > 0) {
                        current_line--;
                    }
                    break;
                case ' ':  
                    if (current_line + LINES - 2 < line_count) {
                        current_line += LINES - 3;
                        if (current_line + LINES - 2 > line_count) {
                            current_line = line_count - (LINES - 2);
                        }
                    }
                    break;
                case 'b':  
                    if (current_line > 0) {
                        current_line -= LINES - 3;
                        if (current_line < 0) {
                            current_line = 0;
                        }
                    }
                    break;
                case 'q':
                    goto cleanup;
            }
        }
        display_lines();
    }
cleanup:
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(filename);
    endwin();
    return 0;
}