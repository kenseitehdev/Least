#include "../include/least.h"
extern struct SyntaxPattern syntax_patterns[];
extern Editor * GLOBAL_EDITOR;

Editor * editor_create() {
  Editor * ed = calloc(1, sizeof(Editor));
  if (!ed) return NULL;
  ed -> buffers = calloc(MAX_BUFFERS, sizeof(Buffer));
  if (!ed -> buffers) {
    free(ed);
    return NULL;
  }
  ed -> current_buffer = 0;
  ed -> num_buffers = 0;
  ed -> command_mode = 0;
  ed -> search_mode = 0;
  ed -> last_search_direction = 1;
  return ed;
}
Buffer * editor_new_buffer(Editor * ed) {
  if (ed -> num_buffers >= MAX_BUFFERS) return NULL;
  
  // Create buffer at the current index
  Buffer * buf = & ed -> buffers[ed -> num_buffers];
  
  // Initialize buffer
  buf -> lines = calloc(MAX_LINES, sizeof(Line));
  if (!buf -> lines) return NULL;  // If allocation fails, don't increment counter
  
  // Initialize other fields
  buf -> capacity = MAX_LINES;
  buf -> count = 0;
  buf -> current_line = 0;
  buf -> screen_line = 0;
  buf -> top_line = 0;
  buf -> total_wrapped_lines = 0;
  buf -> filename = NULL;  // Ensure filename is initialized to NULL
  
  // Only increment counter if everything succeeded
  ed -> num_buffers++;
  
  return buf;
}
void editor_destroy(Editor * ed) {
  if (!ed) return;
  for (int b = 0; b < ed -> num_buffers; b++) {
    Buffer * buf = & ed -> buffers[b];
    for (int i = 0; i < buf -> count; i++) {
      free(buf -> lines[i].content);
      free(buf -> lines[i].wrap_points);
    }
    free(buf -> lines);
    free(buf -> filename);
  }
  free(ed -> buffers);
  free(ed);
}
int get_display_width(const char * str, int len) {
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
void draw_status_bar(Editor * ed) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return;
  int x, y;
  getmaxyx(stdscr, y, x);
  attron(COLOR_PAIR(8) | A_BOLD);
  mvhline(y - 2, 0, ' ', x);
  move(y - 2, 0);
  int percent = (buf -> count <= 1) ? 100 : (buf -> current_line >= buf -> count - 1) ? 100 : (int)((float)(buf -> current_line + 1) / buf -> count * 100);
  char status_message[MAX_LINE_LENGTH];
  snprintf(status_message, sizeof(status_message), " [%d/%d] %s | Line %d/%d (%d%%) | ':n' next | ':p' prev | ':q' close | '/' search", ed -> current_buffer + 1, ed -> num_buffers, buf -> filename, buf -> current_line + 1, buf -> count, percent);
  addstr(status_message);
  attroff(COLOR_PAIR(8) | A_BOLD);
  attron(COLOR_PAIR(9));
  mvhline(y - 1, 0, ' ', x);
  move(y - 1, 0);
  if (ed -> command_mode) {
    printw(":%s", ed -> command_buffer);
  } else if (ed -> search_mode) {
    printw("/%s", ed -> search_buffer);
  }
  attroff(COLOR_PAIR(9));
}
void handle_resize(int sig) {
  (void) sig;
  if (GLOBAL_EDITOR) {
    endwin();
    refresh();
    clear();
    recalculate_wraps(GLOBAL_EDITOR);
    display_lines(GLOBAL_EDITOR);
  }
}

