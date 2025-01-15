
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
#include <regex.h>
#define MAX_LINES 100000
#define MAX_LINE_LENGTH 2048
#define COMMAND_BUFFER_SIZE 256
#define SEARCH_BUFFER_SIZE 256
#define MAX_BUFFERS 100
#define TAB_SIZE 8
typedef struct {
    char *content;
    int length;
    int allocated;
    int *wrap_points;    
    int wrap_count;      
    int wrapped_lines;  
} Line;
typedef struct {
    Line *lines;
    int count;
    int capacity;
    char *filename;
    int current_line;    
    int screen_line;    
    int top_line;        
    int total_wrapped_lines;
    int show_line_numbers;
} Buffer;
typedef struct {
    Buffer *buffers;
    int current_buffer;
    int num_buffers;
    char command_buffer[COMMAND_BUFFER_SIZE];
    char search_buffer[SEARCH_BUFFER_SIZE];
    int command_mode;
    int search_mode;
    int last_search_direction;
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
    {NULL, 0}  
};
void handle_resize(int sig);
Editor *GLOBAL_EDITOR = NULL;
void draw_status_bar(Editor *ed);
Buffer *current_buffer(Editor *ed) {
    if (ed->current_buffer < 0 || ed->current_buffer >= ed->num_buffers) {
        return NULL;
    }
    return &ed->buffers[ed->current_buffer];
}
Editor *editor_create() {
    Editor *ed = calloc(1, sizeof(Editor));
    if (!ed) return NULL;
    ed->buffers = calloc(MAX_BUFFERS, sizeof(Buffer));
    if (!ed->buffers) {
        free(ed);
        return NULL;
    }
    ed->current_buffer = 0;
    ed->num_buffers = 0;
    ed->command_mode = 0;
    ed->search_mode = 0;
    ed->last_search_direction = 1;
    return ed;
}
Buffer *editor_new_buffer(Editor *ed) {
    if (ed->num_buffers >= MAX_BUFFERS) return NULL;
    Buffer *buf = &ed->buffers[ed->num_buffers];
    buf->lines = calloc(MAX_LINES, sizeof(Line));
    if (!buf->lines) return NULL;
    buf->capacity = MAX_LINES;
    buf->count = 0;
    buf->current_line = 0;
    buf->screen_line = 0;
    buf->top_line = 0;
    buf->total_wrapped_lines = 0;
    ed->num_buffers++;
    return buf;
}
void editor_destroy(Editor *ed) {
    if (!ed) return;
    for (int b = 0; b < ed->num_buffers; b++) {
        Buffer *buf = &ed->buffers[b];
        for (int i = 0; i < buf->count; i++) {
            free(buf->lines[i].content);
            free(buf->lines[i].wrap_points);
        }
        free(buf->lines);
        free(buf->filename);
    }
    free(ed->buffers);
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
    Buffer *buf = current_buffer(ed);
    if (!buf) return;
    int screen_width = COLS;
    buf->total_wrapped_lines = 0;
    for (int i = 0; i < buf->count; i++) {
        calculate_line_wraps(&buf->lines[i], screen_width);
        buf->total_wrapped_lines += buf->lines[i].wrapped_lines;
    }
}
int editor_append_line(Buffer *buf, const char *content, int length) {
    if (!buf || buf->count >= buf->capacity) return -1;
    buf->lines[buf->count].content = strndup(content, length);
    if (!buf->lines[buf->count].content) return -1;
    buf->lines[buf->count].length = length;
    buf->lines[buf->count].allocated = length + 1;
    buf->lines[buf->count].wrap_points = NULL;
    buf->lines[buf->count].wrap_count = 0;
    buf->lines[buf->count].wrapped_lines = 1;
    buf->count++;
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
    Buffer *buf = current_buffer(ed);
    if (!buf) return;
    int current_screen_line = 0;
    for (int i = 0; i < buf->count; i++) {
        if (current_screen_line + buf->lines[i].wrapped_lines > screen_line) {
            *file_line = i;
            *wrap_index = screen_line - current_screen_line;
            return;
        }
        current_screen_line += buf->lines[i].wrapped_lines;
    }
    *file_line = buf->count - 1;
    *wrap_index = buf->lines[*file_line].wrapped_lines - 1;
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
void display_lines(Editor *ed) {
    Buffer *buf = current_buffer(ed);
    if (!buf) return;
    clear();  
    int max_display_lines = LINES - 2;  
    int displayed_lines = 0;  
    int file_line, wrap_index;
    screen_to_file_position(ed, buf->screen_line, &file_line, &wrap_index);
    for (int i = file_line; i < buf->count && displayed_lines < max_display_lines; i++) {
        Line *line = &buf->lines[i];
        int start = 0;
        if (buf->show_line_numbers) {
            move(displayed_lines, 0);  
            printw("%4d ", i + 1);  
        }
        for (int w = 0; w < line->wrap_count + 1 && displayed_lines < max_display_lines; w++) {
            int end = (w < line->wrap_count) ? line->wrap_points[w] : line->length;
            if (i == file_line && w < wrap_index) {
                start = end;
                continue;
            }
            display_wrapped_line(line, start, end, displayed_lines, (buf->show_line_numbers ? 6 : 0));
            start = end;
            displayed_lines++;  
        }
    }
    draw_status_bar(ed);  
    refresh();  
}
bool search_forward(Editor *ed, const char* term) {
    Buffer *buf = current_buffer(ed);
    if (!buf || !term || strlen(term) == 0) return false;

    regex_t regex;
    int ret = regcomp(&regex, term, REG_EXTENDED | REG_NEWLINE);
    if (ret) {
        mvprintw(LINES - 1, 0, "Invalid regex pattern");
        clrtoeol();
        refresh();
        napms(1000);
        return false;
    }

    for (int i = buf->current_line + 1; i < buf->count; i++) {
        if (regexec(&regex, buf->lines[i].content, 0, NULL, 0) == 0) {
            buf->current_line = i;
            buf->screen_line = 0;
            for (int j = 0; j < i; j++) {
                buf->screen_line += buf->lines[j].wrapped_lines;
            }
            regfree(&regex);
            return true;
        }
    }

    for (int i = 0; i <= buf->current_line; i++) {
        if (regexec(&regex, buf->lines[i].content, 0, NULL, 0) == 0) {
            buf->current_line = i;
            buf->screen_line = 0;
            for (int j = 0; j < i; j++) {
                buf->screen_line += buf->lines[j].wrapped_lines;
            }
            regfree(&regex);
            return true;
        }
    }

    regfree(&regex);
    return false;
}
void search_backward(Editor *ed, const char* term) {
    Buffer *buf = current_buffer(ed);
    if (!buf || !term || strlen(term) == 0) return;

    regex_t regex;
    int ret = regcomp(&regex, term, REG_EXTENDED | REG_NEWLINE);
    if (ret) {
        mvprintw(LINES - 1, 0, "Invalid regex pattern");
        clrtoeol();
        refresh();
        napms(1000);
        return;
    }

    for (int i = buf->current_line - 1; i >= 0; i--) {
        if (regexec(&regex, buf->lines[i].content, 0, NULL, 0) == 0) {
            buf->current_line = i;
            buf->screen_line = 0;
            for (int j = 0; j < i; j++) {
                buf->screen_line += buf->lines[j].wrapped_lines;
            }
            regfree(&regex);
            return;
        }
    }

    for (int i = buf->count - 1; i >= buf->current_line; i--) {
        if (regexec(&regex, buf->lines[i].content, 0, NULL, 0) == 0) {
            buf->current_line = i;
            buf->screen_line = 0;
            for (int j = 0; j < i; j++) {
                buf->screen_line += buf->lines[j].wrapped_lines;
            }
            regfree(&regex);
            return;
        }
    }

    regfree(&regex);
}
void process_command(Editor *ed) {
    Buffer *buf = current_buffer(ed);
    if (!buf) return;
    if (strcmp(ed->command_buffer, "q") == 0) {
        if (ed->num_buffers > 1) {
            for (int i = ed->current_buffer; i < ed->num_buffers - 1; i++) {
                ed->buffers[i] = ed->buffers[i + 1];
            }
            ed->num_buffers--;
            if (ed->current_buffer >= ed->num_buffers) {
                ed->current_buffer = ed->num_buffers - 1;
            }
        } else {
            endwin();
            editor_destroy(ed);
            exit(0);
        }
    }
    else if (strcmp(ed->command_buffer, "n") == 0) {
        if (ed->current_buffer < ed->num_buffers - 1) {
            ed->current_buffer++;
            clear();
            refresh();
        }
    }
    else if (strcmp(ed->command_buffer, "p") == 0) {
        if (ed->current_buffer > 0) {
            ed->current_buffer--;
            clear();
            refresh();
        }
    }
else if (strcmp(ed->command_buffer, "l") == 0) {
    buf->show_line_numbers = !buf->show_line_numbers;
    clear();  
    display_lines(ed);  
}
    else if (strncmp(ed->command_buffer, "j", 1) == 0) {
        int line_number = 0;
        if (sscanf(ed->command_buffer + 1, "%d", &line_number) == 1) {
            if (line_number > 0 && line_number <= buf->count) {
                buf->current_line = line_number - 1;  
                buf->screen_line = 0;
                for (int j = 0; j < buf->current_line; j++) {
                    buf->screen_line += buf->lines[j].wrapped_lines;
                }
                clear();
                refresh();
            } else {
                mvprintw(LINES-1, 0, "Invalid line number");
                clrtoeol();
                refresh();
                napms(1000);
            }
        } else {
            mvprintw(LINES-1, 0, "Invalid command: j requires a line number");
            clrtoeol();
            refresh();
            napms(1000);
        }
    }
    else if (strncmp(ed->command_buffer, "s/", 2) == 0) {
        ed->search_mode = 1;
        strncpy(ed->search_buffer, ed->command_buffer + 2, SEARCH_BUFFER_SIZE - 1);
        ed->search_buffer[SEARCH_BUFFER_SIZE - 1] = '\0';
        if (!search_forward(ed, ed->search_buffer)) {
            mvprintw(LINES-1, 0, "Pattern not found");
            clrtoeol();
            refresh();
            napms(1000);
        } else {
            clear();
            refresh();
        }
    }
    else {
        mvprintw(LINES-1, 0, "Invalid command");
        clrtoeol();
        refresh();
        napms(1000);
    }
    ed->command_mode = 0;
    ed->command_buffer[0] = '\0';
}
void draw_status_bar(Editor *ed) {
    Buffer *buf = current_buffer(ed);
    if (!buf) return;
    int x, y;
    getmaxyx(stdscr, y, x);
    attron(COLOR_PAIR(8) | A_BOLD);
    mvhline(y - 2, 0, ' ', x);
    move(y - 2, 0);
    int percent = (buf->count <= 1) ? 100 :
                 (buf->current_line >= buf->count - 1) ? 100 :
                 (int)((float)(buf->current_line + 1) / buf->count * 100);
    char status_message[MAX_LINE_LENGTH];
    snprintf(status_message, sizeof(status_message),
             " [%d/%d] %s | Line %d/%d (%d%%) | ':n' next | ':p' prev | ':q' close | '/' search",
             ed->current_buffer + 1, ed->num_buffers, buf->filename,
             buf->current_line + 1, buf->count, percent);
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
int handle_input(Editor *ed, int ch) {
    Buffer *buf = current_buffer(ed);
    if (!buf) return -1;
    if (ed->command_mode) {
        if (ch == '\n') {
            process_command(ed);
        } else if (ch == 27) {
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
        } else if (ch == 27) {
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
                ed->command_buffer[0] = '\0';
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
            case 'p':
                if (strlen(ed->search_buffer) > 0) {
                    search_backward(ed, ed->search_buffer);
                }
                break;
            case KEY_DOWN:
                    buf->screen_line++;
                    int file_line, wrap_index;
                    screen_to_file_position(ed, buf->screen_line, &file_line, &wrap_index);
                    buf->current_line = file_line;
                break;
            case KEY_UP:
                if (buf->screen_line > 0) {
                    buf->screen_line--;
                    int file_line, wrap_index;
                    screen_to_file_position(ed, buf->screen_line, &file_line, &wrap_index);
                    buf->current_line = file_line;
                }
                break;
            case ' ':
                {
                    int page_size = LINES - 3;
                        buf->screen_line += page_size;
                    int file_line, wrap_index;
                    screen_to_file_position(ed, buf->screen_line, &file_line, &wrap_index);
                    buf->current_line = file_line;
                }
                break;
            case 'b':
                {
                    int page_size = LINES - 3;
                    if (buf->screen_line > page_size) {
                        buf->screen_line -= page_size;
                    } else {
                        buf->screen_line = 0;
                    }
                    int file_line, wrap_index;
                    screen_to_file_position(ed, buf->screen_line, &file_line, &wrap_index);
                    buf->current_line = file_line;
                }
                break;
            case 'q':
                return -1;
            case KEY_RIGHT:  
                strcpy(ed->command_buffer, "n");
                process_command(ed);
                break;
            case KEY_LEFT:  
                strcpy(ed->command_buffer, "p");
                process_command(ed);
                break;
        }
    }
    return 0;
}
int load_file(Editor *ed, const char *fname) {
    Buffer *buf = editor_new_buffer(ed);
    if (!buf) return -1;
    FILE *file = fopen(fname, "r");
    if (!file) {
        ed->num_buffers--;
        return -1;
    }
    buf->filename = strdup(fname);
    if (!buf->filename) {
        fclose(file);
        ed->num_buffers--;
        return -1;
    }
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (editor_append_line(buf, buffer, strlen(buffer)) < 0) {
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}
void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] [PIPE_INPUT] | [FILE...]\n", prog_name);
    printf("\nA terminal-based text editor that accepts piped input and files for editing.\n");
    printf("\nOptions:\n");
    printf("  -h, --help            Show this help message and exit.\n");
    printf("  -v, --version         Display the version information and exit.\n");
    printf("\nArguments:\n");
    printf("  PIPE_INPUT            Input provided through a pipe (supports multiple piped inputs).\n");
    printf("  FILE...               One or more files to open and edit (provided after the program name).\n");
    printf("\nExamples:\n");
    printf("  cat input.txt | %s         Pipe the content of 'input.txt' into the editor.\n", prog_name);
    printf("  %s file1.txt file2.txt\n", prog_name);
    printf("                         Edit multiple files: 'file1.txt' and 'file2.txt'.\n");
    printf("  command1 | command2 | %s\n", prog_name);
    printf("                         Pipe the output from multiple commands into the editor.\n");
    printf("  %s file1.txt file2.txt | command1 | command2\n", prog_name);
    printf("                         A mix of files and piped input from multiple commands.\n");
    printf("\nNotes:\n");
    printf("  - Piped input must appear before the program name (e.g., 'command1 | command2 | %s').\n", prog_name);
    printf("  - Files must be listed after the program name (e.g., '%s file1.txt file2.txt').\n", prog_name);
    printf("  - If no input is provided (no files or pipes), this help message will be shown.\n");
}
void print_version() {
    printf("Least version 0.7\n");
}
int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
    }
    Editor *ed = editor_create();
    if (!ed) {
        fprintf(stderr, "Failed to initialize editor\n");
        return 1;
    }
    GLOBAL_EDITOR = ed;
    int need_reopen_tty = 0;
    if (!isatty(STDIN_FILENO)) {
        need_reopen_tty = 1;
        Buffer *pipe_buf = NULL;
        char pipe_name[32];
        int pipe_count = 0;
        int ch;
        char line[MAX_LINE_LENGTH];
        size_t line_pos = 0;
        int found_null = 0;
        while ((ch = fgetc(stdin)) != EOF) {
            if (ch == '\0') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    if (pipe_buf && editor_append_line(pipe_buf, line, line_pos) < 0) {
                        fprintf(stderr, "Failed to process pipe input\n");
                        editor_destroy(ed);
                        return 1;
                    }
                    line_pos = 0;
                }
                pipe_buf = editor_new_buffer(ed);
                if (!pipe_buf) {
                    fprintf(stderr, "Failed to create buffer for pipe input\n");
                    editor_destroy(ed);
                    return 1;
                }
                snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                pipe_buf->filename = strdup(pipe_name);
                found_null = 1;
                continue;
            }
            if (line_pos >= MAX_LINE_LENGTH - 1) {
                line[line_pos] = '\0';
                if (!pipe_buf) {
                    pipe_buf = editor_new_buffer(ed);
                    if (!pipe_buf) {
                        fprintf(stderr, "Failed to create buffer for pipe input\n");
                        editor_destroy(ed);
                        return 1;
                    }
                    snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                    pipe_buf->filename = strdup(pipe_name);
                }
                if (editor_append_line(pipe_buf, line, line_pos) < 0) {
                    fprintf(stderr, "Failed to process pipe input\n");
                    editor_destroy(ed);
                    return 1;
                }
                line_pos = 0;
            }
            if (ch == '\n') {
                if (!pipe_buf) {
                    pipe_buf = editor_new_buffer(ed);
                    if (!pipe_buf) {
                        fprintf(stderr, "Failed to create buffer for pipe input\n");
                        editor_destroy(ed);
                        return 1;
                    }
                    snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                    pipe_buf->filename = strdup(pipe_name);
                }
                line[line_pos] = '\0';
                if (editor_append_line(pipe_buf, line, line_pos) < 0) {
                    fprintf(stderr, "Failed to process pipe input\n");
                    editor_destroy(ed);
                    return 1;
                }
                line_pos = 0;
            } else {
                line[line_pos++] = ch;
            }
        }
        if (line_pos > 0) {
            line[line_pos] = '\0';
            if (!pipe_buf) {
                pipe_buf = editor_new_buffer(ed);
                if (!pipe_buf) {
                    fprintf(stderr, "Failed to create buffer for pipe input\n");
                    editor_destroy(ed);
                    return 1;
                }
                snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                pipe_buf->filename = strdup(pipe_name);
            }
            if (editor_append_line(pipe_buf, line, line_pos) < 0) {
                fprintf(stderr, "Failed to process pipe input\n");
                editor_destroy(ed);
                return 1;
            }
        }
        if (ed->num_buffers == 0) {
            fprintf(stderr, "No input received from pipe\n");
            editor_destroy(ed);
            return 1;
        }
    }
    for (int i = 1; i < argc; i++) {
        if (load_file(ed, argv[i]) < 0) {
            fprintf(stderr, "Failed to load file %s: %s\n", argv[i], strerror(errno));
            continue;
        }
    }
    if (need_reopen_tty) {
        if (freopen("/dev/tty", "r", stdin) == NULL) {
            fprintf(stderr, "Failed to reopen tty\n");
            editor_destroy(ed);
            return 1;
        }
    }
    if (ed->num_buffers == 0) {
        fprintf(stderr, "Usage: %s [file_name ...] < [pipe_input]\n", argv[0]);
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
    while (1) {
        Buffer *buf = current_buffer(ed);
        if (!buf) break;
        display_lines(ed);
        int ch = getch();
        if (handle_input(ed, ch) < 0) break;
    }
    editor_destroy(ed);
    endwin();
    return 0;
}
