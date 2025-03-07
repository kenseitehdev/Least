#include "../include/least.h"
extern struct SyntaxPattern syntax_patterns[];
extern Editor * GLOBAL_EDITOR;

void calculate_line_wraps(Line * line, int screen_width) {
  free(line -> wrap_points);
  line -> wrap_points = NULL;
  line -> wrap_count = 0;
  line -> wrapped_lines = 1;
  if (line -> length == 0) return;
  int max_wraps = (line -> length / (screen_width / 2)) + 1;
  line -> wrap_points = malloc(sizeof(int) * max_wraps);
  if (!line -> wrap_points) return;
  int current_width = 0;
  int last_wrap = 0;
  int last_space = -1;
  for (int i = 0; i < line -> length; i++) {
    char c = line -> content[i];
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
      line -> wrap_points[line -> wrap_count++] = wrap_at;
      last_wrap = wrap_at;
      current_width = get_display_width(line -> content + wrap_at, i - wrap_at + 1);
      last_space = -1;
      line -> wrapped_lines++;
    }
  }
}
Buffer * current_buffer(Editor * ed) {
  if (ed -> current_buffer < 0 || ed -> current_buffer >= ed -> num_buffers) {
    return NULL;
  }
  return & ed -> buffers[ed -> current_buffer];
}
void display_lines(Editor * ed) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return;
  clear();
  int max_display_lines = LINES - 2;
  int displayed_lines = 0;
  int file_line, wrap_index;
  screen_to_file_position(ed, buf -> screen_line, & file_line, & wrap_index);
  for (int i = file_line; i < buf -> count && displayed_lines < max_display_lines; i++) {
    Line * line = & buf -> lines[i];
    int start = 0;
    if (buf -> show_line_numbers) {
      move(displayed_lines, 0);
      printw("%4d ", i + 1);
    }
    for (int w = 0; w < line -> wrap_count + 1 && displayed_lines < max_display_lines; w++) {
      int end = (w < line -> wrap_count) ? line -> wrap_points[w] : line -> length;
      if (i == file_line && w < wrap_index) {
        start = end;
        continue;
      }
      display_wrapped_line(line, start, end, displayed_lines, (buf -> show_line_numbers ? 6 : 0));
      start = end;
      displayed_lines++;
    }
  }
  draw_status_bar(ed);
  refresh();
}

