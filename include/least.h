
#ifndef LEAST_H
#define LEAST_H

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
  int start;
  int end;
} SearchMatch;

typedef struct {
  SearchMatch * matches;
  int count;
  int capacity;
} LineMatches;

typedef struct {
  char * content;
  int length;
  int allocated;
  int * wrap_points;
  int wrap_count;
  int wrapped_lines;
  LineMatches matches;
} Line;

typedef struct {
  Line * lines;
  int count;
  int capacity;
  char * filename;
  int current_line;
  int screen_line;
  int top_line;
  int total_wrapped_lines;
  int show_line_numbers;
} Buffer;

typedef struct {
  Buffer * buffers;
  int current_buffer;
  int num_buffers;
  char command_buffer[COMMAND_BUFFER_SIZE];
  char search_buffer[SEARCH_BUFFER_SIZE];
  int command_mode;
  int search_mode;
  int last_search_direction;
} Editor;

struct SyntaxPattern {
  char * pattern;
  int color_pair;
};

/* Changed from definition to declaration with extern */

void handle_resize(int sig);
void draw_status_bar(Editor * ed);
void display_lines(Editor * ed);
void display_wrapped_line(const Line * line, int start, int end, int y, int x);
void screen_to_file_position(Editor * ed, int screen_line, int * file_line, int * wrap_index);
int get_display_width(const char * str, int len);
Buffer * current_buffer(Editor * ed);
void recalculate_wraps(Editor * ed);
void editor_destroy(Editor * ed);
Buffer * editor_new_buffer(Editor * ed);
void calculate_line_wraps(Line * line, int screen_width);
Editor * editor_create();

#endif /* LEAST_H */
