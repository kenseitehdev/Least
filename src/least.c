#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>

#define MAX_LINES 100000
#define MAX_LINE_LENGTH 2048
#define COMMAND_BUFFER_SIZE 256
#define SEARCH_BUFFER_SIZE 256
#define TAB_SIZE 8

typedef struct {
    char *content;
    int length;
    int allocated;
    int *wrap_points;    // Array of indices where lines wrap
    int wrap_count;      // Number of wrap points
    int wrapped_lines;   // Total number of screen lines after wrapping
} Line;

typedef struct {
    Line *lines;
    int count;
    int capacity;
    char *filename;
    int current_line;    // Logical line number
    int screen_line;     // Visual line number (accounting for wraps)
    int top_line;        // Top visible line
    char command_buffer[COMMAND_BUFFER_SIZE];
    char search_buffer[SEARCH_BUFFER_SIZE];
    int command_mode;
    int search_mode;
    int last_search_direction;
    int total_wrapped_lines;  // Total number of screen lines
} Editor;

struct SyntaxPattern {
    char *pattern;
    int color_pair;
};

struct SyntaxPattern syntax_patterns[] = {
    {"#include", 1}, {"#define", 1}, {"#ifdef", 1}, {"#ifndef", 1}, {"#endif", 1},
    {"int", 2}, {"char", 2}, {"void", 2}, {"return", 2}, {"for", 2}, {"while", 2},
    {"if", 2}, {"else", 2}, {"struct", 2}, {"enum", 2}, {"typedef", 2}, {"static", 2},
    {"const", 2}, {"size_t", 3}, {"uint32_t", 3}, {"int32_t", 3}, {"bool", 3},
    {"float", 3}, {"double", 3}, {"def", 4}, {"class", 4}, {"import", 4}, {"from", 4},
    {"lambda", 4}, {"try", 4}, {"except", 4}, {"finally", 4}, {"with", 4}, {"True", 4},
    {"False", 4}, {"None", 4}, {"public", 5}, {"private", 5}, {"protected", 5},
    {"interface", 5}, {"extends", 5}, {"implements", 5}, {"new", 5}, {"super", 5},
    {"function", 6}, {"var", 6}, {"let", 6}, {"async", 6}, {"await", 6},
    {NULL, 0}  // Sentinel value to mark end of array
};

// Forward declarations
void handle_resize(int sig);
void recalculate_wraps(Editor *ed);
Editor *GLOBAL_EDITOR = NULL;  // For signal handler

Editor *editor_create() {
    Editor *ed = calloc(1, sizeof(Editor));
    if (!ed) return NULL;
    
    ed->lines = calloc(MAX_LINES, sizeof(Line));
    if (!ed->lines) {
        free(ed);
        return NULL;
    }
    
    ed->capacity = MAX_LINES;
    ed->count = 0;
    ed->current_line = 0;
    ed->screen_line = 0;
    ed->top_line = 0;
    ed->command_mode = 0;
    ed->search_mode = 0;
    ed->last_search_direction = 1;
    ed->total_wrapped_lines = 0;
    
    return ed;
}

void editor_destroy(Editor *ed) {
    if (!ed) return;
    
    for (int i = 0; i < ed->count; i++) {
        free(ed->lines[i].content);
        free(ed->lines[i].wrap_points);
    }
    free(ed->lines);
    free(ed->filename);
    free(ed);
}

int get_display_width(const char *str, int len) {
    int width = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\t') {
            width += TAB_SIZE - (width % TAB_SIZE);
        } else if (isprint(str[i])) {
            width++;
        }
    }
    return width;
}

void calculate_line_wraps(Line *line, int screen_width) {
    free(line->wrap_points);
    line->wrap_points = NULL;
    line->wrap_count = 0;
    line->wrapped_lines = 1;

    if (line->length == 0) return;

    int max_wraps = (line->length / (screen_width / 2)) + 1;
    line->wrap_points = malloc(sizeof(int) * max_wraps);
    if (!line->wrap_points) return;

    int current_width = 0;
    int last_wrap = 0;
    int last_space = -1;

    for (int i = 0; i < line->length; i++) {
        char c = line->content[i];
        
        if (c == '\t') {
            current_width += TAB_SIZE - (current_width % TAB_SIZE);
        } else if (isprint(c)) {
            current_width++;
        }

        if (isspace(c)) {
            last_space = i;
        }

        if (current_width >= screen_width) {
            int wrap_at;
            
            if (last_space > last_wrap && last_space - last_wrap < screen_width) {
                wrap_at = last_space;
            } else {
                wrap_at = i;
            }

            line->wrap_points[line->wrap_count++] = wrap_at;
            last_wrap = wrap_at;
            current_width = get_display_width(line->content + wrap_at, i - wrap_at + 1);
            last_space = -1;
            line->wrapped_lines++;
        }
    }
}

void recalculate_wraps(Editor *ed) {
    int screen_width = COLS;
    ed->total_wrapped_lines = 0;
    
    for (int i = 0; i < ed->count; i++) {
        calculate_line_wraps(&ed->lines[i], screen_width);
        ed->total_wrapped_lines += ed->lines[i].wrapped_lines;
    }
}

int editor_append_line(Editor *ed, const char *content, int length) {
    if (ed->count >= ed->capacity) return -1;
    
    ed->lines[ed->count].content = strndup(content, length);
    if (!ed->lines[ed->count].content) return -1;
    
    ed->lines[ed->count].length = length;
    ed->lines[ed->count].allocated = length + 1;
    ed->lines[ed->count].wrap_points = NULL;
    ed->lines[ed->count].wrap_count = 0;
    ed->lines[ed->count].wrapped_lines = 1;
    
    ed->count++;
    return 0;
}

void highlight_syntax(const char *line) {
    int in_string = 0, in_char = 0, in_multiline_comment = 0, in_single_comment = 0;
    char prev_char = '\0';

    for (const char *pos = line; *pos != '\0'; pos++) {
        if (!in_string && !in_char) {
            if (pos[0] == '/' && pos[1] == '*' && !in_single_comment) {
                in_multiline_comment = 1;
                attron(COLOR_PAIR(4));
                addstr("/*");
                pos++;
                continue;
            }
            if (pos[0] == '*' && pos[1] == '/' && in_multiline_comment) {
                in_multiline_comment = 0;
                addstr("*/");
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
            addch(*pos);
        } else if (in_string || in_char) {
            attron(COLOR_PAIR(5));
            addch(*pos);
        } else {
            int pattern_matched = 0;
            for (int i = 0; syntax_patterns[i].pattern != NULL; i++) {
                int len = strlen(syntax_patterns[i].pattern);
                if (strncmp(pos, syntax_patterns[i].pattern, len) == 0 &&
                    (pos == line || !isalnum(*(pos - 1))) &&
                    !isalnum(*(pos + len))) {
                    attron(COLOR_PAIR(syntax_patterns[i].color_pair));
                    for (int j = 0; j < len; j++) {
                        addch(*(pos + j));
                    }
                    attroff(COLOR_PAIR(syntax_patterns[i].color_pair));
                    pos += len - 1;
                    pattern_matched = 1;
                    break;
                }
            }
            if (!pattern_matched) {
                if (isdigit(*pos) || (*pos == '-' && isdigit(*(pos + 1)))) {
                    attron(COLOR_PAIR(6));
                    addch(*pos);
                    attroff(COLOR_PAIR(6));
                } else if (strchr("+-*/%=<>!&|^~", *pos)) {
                    attron(COLOR_PAIR(7));
                    addch(*pos);
                    attroff(COLOR_PAIR(7));
                } else {
                    addch(*pos);
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

void screen_to_file_position(Editor *ed, int screen_line, int *file_line, int *wrap_index) {
    int current_screen_line = 0;
    
    for (int i = 0; i < ed->count; i++) {
        if (current_screen_line + ed->lines[i].wrapped_lines > screen_line) {
            *file_line = i;
            *wrap_index = screen_line - current_screen_line;
            return;
        }
        current_screen_line += ed->lines[i].wrapped_lines;
    }
    
    *file_line = ed->count - 1;
    *wrap_index = ed->lines[*file_line].wrapped_lines - 1;
}

void display_wrapped_line(const Line *line, int start, int end, int y, int x) {
    move(y, x);
    
    int current_x = x;
    char *temp = malloc(end - start + 1);
    if (!temp) return;
    
    strncpy(temp, line->content + start, end - start);
    temp[end - start] = '\0';
    
    highlight_syntax(temp);
    free(temp);
}

void draw_status_bar(Editor *ed) {
    int x, y;
    getmaxyx(stdscr, y, x);
    
    attron(COLOR_PAIR(8) | A_BOLD);
    mvhline(y - 2, 0, ' ', x);
    move(y - 2, 0);
    
    int percent;
    if (ed->count <= 1) {
        percent = 100;
    } else if (ed->current_line >= ed->count - 1) {
        percent = 100;
    } else {
        percent = (int)((float)(ed->current_line + 1) / ed->count * 100);
    }
    
    char status_message[MAX_LINE_LENGTH];
    snprintf(status_message, sizeof(status_message),
             " %s | Line %d/%d (%d%%) | ':' cmd | '/' search | 'n' next | 'N' prev | 'q' quit",
             ed->filename, ed->current_line + 1, ed->count, percent);
    addstr(status_message);
    attroff(COLOR_PAIR(8) | A_BOLD);
    
    attron(COLOR_PAIR(9));
    mvhline(y - 1, 0, ' ', x);
    move(y - 1, 0);
    if (ed->command_mode) {
        printw(":%s", ed->command_buffer);
    } else if (ed->search_mode) {
        printw("/%s", ed->search_buffer);
    }
    attroff(COLOR_PAIR(9));
}

void display_lines(Editor *ed) {
    clear();
    int max_display_lines = LINES - 2;
    
    int current_screen_line = 0;
    int displayed_lines = 0;
    
    int file_line, wrap_index;
    screen_to_file_position(ed, ed->screen_line, &file_line, &wrap_index);
    
    for (int i = file_line; i < ed->count && displayed_lines < max_display_lines; i++) {
        Line *line = &ed->lines[i];
        
        int start = 0;
        for (int w = 0; w < line->wrap_count + 1 && displayed_lines < max_display_lines; w++) {
            int end = (w < line->wrap_count) ? line->wrap_points[w] : line->length;
            
            if (i == file_line && w < wrap_index) {
                start = end;
                continue;
            }
            
            display_wrapped_line(line, start, end, displayed_lines, 0);
            displayed_lines++;
            start = end;
        }
    }
    
    draw_status_bar(ed);
    refresh();
}

void handle_resize(int sig) {
    (void)sig;
    if (GLOBAL_EDITOR) {
        endwin();
        refresh();
        clear();
        recalculate_wraps(GLOBAL_EDITOR);
        display_lines(GLOBAL_EDITOR);
    }
}

void search_forward(Editor *ed, const char* term) {
    if (!term || strlen(term) == 0) return;
    
    for (int i = ed->current_line + 1; i < ed->count; i++) {
        if (strstr(ed->lines[i].content, term)) {
            ed->current_line = i;
            return;
        }
    }
    
    for (int i = 0; i <= ed->current_line; i++) {
        if (strstr(ed->lines[i].content, term)) {
            ed->current_line = i;
            return;
        }
    }
}

void search_backward(Editor *ed, const char* term) {
    if (!term || strlen(term) == 0) return;
    
    for (int i = ed->current_line - 1; i >= 0; i--) {
        if (strstr(ed->lines[i].content, term)) {
            ed->current_line = i;
            return;
        }
    }
    
    for (int i = ed->count - 1; i >= ed->current_line; i--) {
        if (strstr(ed->lines[i].content, term)) {
            ed->current_line = i;
            return;
        }
    }
}

void process_command(Editor *ed) {
    if (strcmp(ed->command_buffer, "q") == 0 || strcmp(ed->command_buffer, "quit") == 0) {
        endwin();
        editor_destroy(ed);
        exit(0);
    } else if (strncmp(ed->command_buffer, "s/", 2) == 0) {
        ed->search_mode = 1;
        strncpy(ed->search_buffer, ed->command_buffer + 2, SEARCH_BUFFER_SIZE - 1);
        search_forward(ed, ed->search_buffer);
    }
    ed->command_mode = 0;
    ed->command_buffer[0] = '\0';
}

int handle_input(Editor *ed, int ch) {
    if (ed->command_mode) {
        if (ch == '\n') {
            process_command(ed);
        } else if (ch == 27) { // ESC
            ed->command_mode = 0;
            ed->command_buffer[0] = '\0';
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            int len = strlen(ed->command_buffer);
            if (len > 0) ed->command_buffer[len - 1] = '\0';
        } else if (isprint(ch)) {
            int len = strlen(ed->command_buffer);
            if (len < COMMAND_BUFFER_SIZE - 1) {
                ed->command_buffer[len] = ch;
                ed->command_buffer[len + 1] = '\0';
            }
        }
    } else if (ed->search_mode) {
        if (ch == '\n') {
            search_forward(ed, ed->search_buffer);
            ed->search_mode = 0;
        } else if (ch == 27) { // ESC
            ed->search_mode = 0;
            ed->search_buffer[0] = '\0';
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            int len = strlen(ed->search_buffer);
            if (len > 0) ed->search_buffer[len - 1] = '\0';
        } else if (isprint(ch)) {
            int len = strlen(ed->search_buffer);
            if (len < SEARCH_BUFFER_SIZE - 1) {
                ed->search_buffer[len] = ch;
                ed->search_buffer[len + 1] = '\0';
            }
        }
    } else {
        switch (ch) {
            case ':':
                ed->command_mode = 1;
                break;
            case '/':
                ed->search_mode = 1;
                ed->search_buffer[0] = '\0';
                break;
            case 'n':
                if (strlen(ed->search_buffer) > 0) {
                    search_forward(ed, ed->search_buffer);
                }
                break;
            case 'N':
                if (strlen(ed->search_buffer) > 0) {
                    search_backward(ed, ed->search_buffer);
                }
                break;
            case KEY_DOWN:
                if (ed->screen_line < ed->total_wrapped_lines - 1) {
                    ed->screen_line++;
                    int file_line, wrap_index;
                    screen_to_file_position(ed, ed->screen_line, &file_line, &wrap_index);
                    ed->current_line = file_line;
                }
                break;
            case KEY_UP:
                if (ed->screen_line > 0) {
                    ed->screen_line--;
                    int file_line, wrap_index;
                    screen_to_file_position(ed, ed->screen_line, &file_line, &wrap_index);
                    ed->current_line = file_line;
                }
                break;
            case ' ': // Page Down
                {
                    int page_size = LINES - 3;
                    if (ed->screen_line + page_size < ed->total_wrapped_lines) {
                        ed->screen_line += page_size;
                        int file_line, wrap_index;
                        screen_to_file_position(ed, ed->screen_line, &file_line, &wrap_index);
                        ed->current_line = file_line;
                    } else {
                        ed->screen_line = ed->total_wrapped_lines - 1;
                        ed->current_line = ed->count - 1;
                    }
                }
                break;
            case 'b': // Page Up
                {
                    int page_size = LINES - 3;
                    if (ed->screen_line > page_size) {
                        ed->screen_line -= page_size;
                    } else {
                        ed->screen_line = 0;
                    }
                    int file_line, wrap_index;
                    screen_to_file_position(ed, ed->screen_line, &file_line, &wrap_index);
                    ed->current_line = file_line;
                }
                break;
            case 'q':
                return -1;
        }
    }
    return 0;
}

int load_file(Editor *ed, const char *fname) {
    FILE *file = fopen(fname, "r");
    if (!file) {
        return -1;
    }

    ed->filename = strdup(fname);
    if (!ed->filename) {
        fclose(file);
        return -1;
    }

    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (editor_append_line(ed, buffer, strlen(buffer)) < 0) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int main(int argc, char *argv[]) {
    Editor *ed = editor_create();
    if (!ed) {
        fprintf(stderr, "Failed to initialize editor\n");
        return 1;
    }

    GLOBAL_EDITOR = ed;  // For signal handler

    int using_pipe = !isatty(STDIN_FILENO);
    if (using_pipe) {
        char buffer[MAX_LINE_LENGTH];
        while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            if (editor_append_line(ed, buffer, strlen(buffer)) < 0) {
                fprintf(stderr, "Failed to process input\n");
                editor_destroy(ed);
                return 1;
            }
        }
        if (ed->count == 0) {
            fprintf(stderr, "No input received from pipe\n");
            editor_destroy(ed);
            return 1;
        }
        freopen("/dev/tty", "r", stdin);
        ed->filename = strdup("stdin");
    } else if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_name>\n", argv[0]);
        editor_destroy(ed);
        return 1;
    } else if (load_file(ed, argv[1]) < 0) {
        fprintf(stderr, "Failed to load file: %s\n", strerror(errno));
        editor_destroy(ed);
        return 1;
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGWINCH, handle_resize);
    
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

    recalculate_wraps(ed);
    display_lines(ed);

    while (1) {
        int ch = getch();
        if (handle_input(ed, ch) < 0) break;
        display_lines(ed);
    }

    editor_destroy(ed);
    endwin();
    return 0;
}
