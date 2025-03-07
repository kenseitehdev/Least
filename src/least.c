#include "../include/least.h"
Editor *GLOBAL_EDITOR;
struct SyntaxPattern syntax_patterns[] = {
  {"#include", 1}, {"#define", 1}, {"#ifdef", 1}, {"#ifndef", 1}, {"#endif", 1},
  {".TH", 1}, {".SH", 1}, {"#", 1}, {"{{", 1}, {"}}", 1},
  {"int", 2}, {"char", 2}, {"void", 2}, {"return", 2}, {"for", 2}, {"while", 2},
  {"if", 2}, {"else", 2}, {"struct", 2}, {"enum", 2}, {"typedef", 2}, {"static", 2},
  {"const", 2}, {".B", 2}, {".I", 2}, {">", 2}, {"-", 2}, {"`", 2}, {"ls", 2},
  {"cd", 2}, {"pwd", 2}, {"mkdir", 2}, {"rm", 2}, {"cp", 2}, {"mv", 2}, {"touch", 2},
  {"chmod", 2}, {"chown", 2}, {"cat", 2}, {"grep", 2}, {"find", 2}, {"sed", 2},
  {"awk", 2}, {"tar", 2}, {"gzip", 2}, {"ssh", 2}, {"scp", 2}, {"ping", 2},
  {"netstat", 2}, {"ps", 2}, {"kill", 2}, {"top", 2}, {"df", 2}, {"du", 2},
  {"free", 2}, {"sudo", 2}, {"su", 2}, {"man", 2}, {"whoami", 2}, {"which", 2},
  {"echo", 2}, {"export", 2}, {"source", 2}, {"curl", 2}, {"wget", 2},
  {"size_t", 3}, {"uint32_t", 3}, {"int32_t", 3}, {"bool", 3}, {"float", 3},
  {"double", 3}, {"def", 3}, {"class", 3}, {"import", 3}, {"from", 3}, {"lambda", 3},
  {"try", 3}, {"except", 3}, {"finally", 3}, {"with", 3}, {"True", 3}, {"False", 3},
  {"None", 3}, {".BI", 3}, {"Example:", 3},
  {"public", 4}, {"private", 4}, {"protected", 4}, {"interface", 4}, {"extends", 4},
  {"implements", 4}, {"new", 4}, {"super", 4}, {"local", 4}, {"function", 4},
  {"end", 4}, {"then", 4}, {"elseif", 4}, {".LP", 4}, {".PP", 4}, {"More information:", 4},
  {"function", 5}, {"var", 5}, {"let", 5}, {"async", 5}, {"await", 5}, {"SELECT", 5},
  {"FROM", 5}, {"WHERE", 5}, {"INSERT", 5}, {"UPDATE", 5}, {"DELETE", 5}, {"JOIN", 5},
  {"<!DOCTYPE", 5}, {"<html", 5}, {"<body", 5}, {"<div", 5}, {"color", 5},
  {"background", 5}, {"position", 5},
  {"NOTE:", 6}, {"WARNING:", 6}, {"TODO:", 6}, {"FIXME:", 6}, {".BR", 6}, {".SS", 6},
  {".IP", 6},
  {NULL, 0}
};
void recalculate_wraps(Editor * ed) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return;
  int screen_width = COLS;
  buf -> total_wrapped_lines = 0;
  for (int i = 0; i < buf -> count; i++) {
    calculate_line_wraps( & buf -> lines[i], screen_width);
    buf -> total_wrapped_lines += buf -> lines[i].wrapped_lines;
  }
}
int editor_append_line(Buffer * buf,
  const char * content, int length) {
  if (!buf || buf -> count >= buf -> capacity) return -1;
  buf -> lines[buf -> count].content = strndup(content, length);
  if (!buf -> lines[buf -> count].content) return -1;
  buf -> lines[buf -> count].length = length;
  buf -> lines[buf -> count].allocated = length + 1;
  buf -> lines[buf -> count].wrap_points = NULL;
  buf -> lines[buf -> count].wrap_count = 0;
  buf -> lines[buf -> count].wrapped_lines = 1;
  buf -> lines[buf -> count].matches.matches = NULL;
  buf -> lines[buf -> count].matches.count = 0;
  buf -> lines[buf -> count].matches.capacity = 0;
  buf -> count++;
  return 0;
}
void highlight_syntax(const char * line) {
  int in_string = 0, in_char = 0, in_multiline_comment = 0, in_single_comment = 0;
  char prev_char = '\0';
  for (const char * pos = line;* pos != '\0'; pos++) {
    if (!in_string && !in_char) {
      if (pos[0] == '/' && pos[1] == '*' && !in_single_comment) {
        in_multiline_comment = 1;
        attron(COLOR_PAIR(4));
        addstr("");
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
      if ( * pos == '"' && prev_char != '\\') {
        in_string = !in_string;
        attron(COLOR_PAIR(5));
      }
      if ( * pos == '\'' && prev_char != '\\') {
        in_char = !in_char;
        attron(COLOR_PAIR(5));
      }
    }
    if (in_multiline_comment || in_single_comment) {
      attron(COLOR_PAIR(4));
      addch( * pos);
    } else if (in_string || in_char) {
      attron(COLOR_PAIR(5));
      addch( * pos);
    } else {
      int pattern_matched = 0;
      for (int i = 0; syntax_patterns[i].pattern != NULL; i++) {
        int len = strlen(syntax_patterns[i].pattern);
        if (strncasecmp(pos, syntax_patterns[i].pattern, len) == 0 &&
          (pos == line || !isalnum( * (pos - 1)) || ispunct( * (pos - 1))) &&
          (!isalnum( * (pos + len)) || * (pos + len) == '\0' || ispunct( * (pos + len)))) {
          attron(COLOR_PAIR(syntax_patterns[i].color_pair));
          for (int j = 0; j < len; j++) {
            addch( * (pos + j));
          }
          attroff(COLOR_PAIR(syntax_patterns[i].color_pair));
          pos += len - 1;
          pattern_matched = 1;
          break;
        }
      }
      if (!pattern_matched) {
        if (isdigit( * pos) || ( * pos == '-' && isdigit( * (pos + 1)))) {
          attron(COLOR_PAIR(6));
          addch( * pos);
          attroff(COLOR_PAIR(6));
        } else if (strchr("+-*/%=<>!&|^~", * pos)) {
          attron(COLOR_PAIR(7));
          addch( * pos);
          attroff(COLOR_PAIR(7));
        } else {
          addch( * pos);
        }
      }
    }
    prev_char = * pos;
    if ( * pos == '\n') {
      in_single_comment = 0;
      attroff(COLOR_PAIR(4));
    }
  }
}
void screen_to_file_position(Editor * ed, int screen_line, int * file_line, int * wrap_index) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return;
  int current_screen_line = 0;
  for (int i = 0; i < buf -> count; i++) {
    if (current_screen_line + buf -> lines[i].wrapped_lines > screen_line) {
      * file_line = i;
      * wrap_index = screen_line - current_screen_line;
      return;
    }
    current_screen_line += buf -> lines[i].wrapped_lines;
  }
  * file_line = buf -> count - 1;
  * wrap_index = buf -> lines[ * file_line].wrapped_lines - 1;
}
void display_wrapped_line(const Line * line, int start, int end, int y, int x) {
  move(y, x);
  if (line -> matches.count == 0) {
    char * temp = malloc(end - start + 1);
    if (!temp) return;
    strncpy(temp, line -> content + start, end - start);
    temp[end - start] = '\0';
    highlight_syntax(temp);
    free(temp);
    return;
  }
  int current_pos = start;
  for (int i = 0; i < line -> matches.count; i++) {
    SearchMatch match = line -> matches.matches[i];
    if (match.end <= start) continue;
    if (match.start >= end) break;
    if (current_pos < match.start) {
      int len = match.start - current_pos;
      char * temp = malloc(len + 1);
      if (!temp) continue;
      strncpy(temp, line -> content + current_pos, len);
      temp[len] = '\0';
      highlight_syntax(temp);
      free(temp);
    }
    int match_start = (match.start > start) ? match.start : start;
    int match_end = (match.end < end) ? match.end : end;
    int len = match_end - match_start;
    if (len > 0) {
      char * temp = malloc(len + 1);
      if (!temp) continue;
      strncpy(temp, line -> content + match_start, len);
      temp[len] = '\0';
      attron(COLOR_PAIR(10));
      addstr(temp);
      attroff(COLOR_PAIR(10));
      free(temp);
    }
    current_pos = match_end;
  }
  if (current_pos < end) {
    int len = end - current_pos;
    char * temp = malloc(len + 1);
    if (!temp) return;
    strncpy(temp, line -> content + current_pos, len);
    temp[len] = '\0';
    highlight_syntax(temp);
    free(temp);
  }
}
bool search_forward(Editor * ed,
  const char * term) {
  Buffer * buf = current_buffer(ed);
  if (!buf || !term || strlen(term) == 0) return false;
  for (int i = 0; i < buf -> count; i++) {
    free(buf -> lines[i].matches.matches);
    buf -> lines[i].matches.matches = NULL;
    buf -> lines[i].matches.count = 0;
    buf -> lines[i].matches.capacity = 0;
  }
  regex_t regex;
  int ret = regcomp( & regex, term, REG_EXTENDED | REG_NEWLINE);
  if (ret) {
    mvprintw(LINES - 1, 0, "Invalid regex pattern");
    clrtoeol();
    refresh();
    napms(1000);
    return false;
  }
  regmatch_t pmatch[1];
  bool found = false;
  int first_match_line = -1;
  for (int i = buf -> current_line; i < buf -> count; i++) {
    char * line_content = buf -> lines[i].content;
    int offset = 0;
    while (regexec( & regex, line_content + offset, 1, pmatch, 0) == 0) {
      if (buf -> lines[i].matches.count >= buf -> lines[i].matches.capacity) {
        int new_capacity = buf -> lines[i].matches.capacity == 0 ? 4 : buf -> lines[i].matches.capacity * 2;
        SearchMatch * new_matches = realloc(buf -> lines[i].matches.matches, new_capacity * sizeof(SearchMatch));
        if (!new_matches) break;
        buf -> lines[i].matches.matches = new_matches;
        buf -> lines[i].matches.capacity = new_capacity;
      }
      SearchMatch match = {
        .start = offset + pmatch[0].rm_so,
        .end = offset + pmatch[0].rm_eo
      };
      buf -> lines[i].matches.matches[buf -> lines[i].matches.count++] = match;
      if (!found) {
        found = true;
        first_match_line = i;
      }
      offset += pmatch[0].rm_eo;
      if (pmatch[0].rm_so == pmatch[0].rm_eo) break;
    }
  }
  if (!found) {
    for (int i = 0; i < buf -> current_line; i++) {
      char * line_content = buf -> lines[i].content;
      int offset = 0;
      while (regexec( & regex, line_content + offset, 1, pmatch, 0) == 0) {
        if (buf -> lines[i].matches.count >= buf -> lines[i].matches.capacity) {
          int new_capacity = buf -> lines[i].matches.capacity == 0 ? 4 : buf -> lines[i].matches.capacity * 2;
          SearchMatch * new_matches = realloc(buf -> lines[i].matches.matches, new_capacity * sizeof(SearchMatch));
          if (!new_matches) break;
          buf -> lines[i].matches.matches = new_matches;
          buf -> lines[i].matches.capacity = new_capacity;
        }
        SearchMatch match = {
          .start = offset + pmatch[0].rm_so,
          .end = offset + pmatch[0].rm_eo
        };
        buf -> lines[i].matches.matches[buf -> lines[i].matches.count++] = match;
        if (!found) {
          found = true;
          first_match_line = i;
        }
        offset += pmatch[0].rm_eo;
        if (pmatch[0].rm_so == pmatch[0].rm_eo) break;
      }
    }
  }
  regfree( & regex);
  if (found) {
    buf -> current_line = first_match_line;
    buf -> screen_line = 0;
    for (int j = 0; j < first_match_line; j++) {
      buf -> screen_line += buf -> lines[j].wrapped_lines;
    }
    return true;
  }
  return false;
}
void search_backward(Editor * ed,
  const char * term) {
  Buffer * buf = current_buffer(ed);
  if (!buf || !term || strlen(term) == 0) return;
  for (int i = 0; i < buf -> count; i++) {
    free(buf -> lines[i].matches.matches);
    buf -> lines[i].matches.matches = NULL;
    buf -> lines[i].matches.count = 0;
    buf -> lines[i].matches.capacity = 0;
  }
  regex_t regex;
  int ret = regcomp( & regex, term, REG_EXTENDED | REG_NEWLINE);
  if (ret) {
    mvprintw(LINES - 1, 0, "Invalid regex pattern");
    clrtoeol();
    refresh();
    napms(1000);
    return;
  }
  regmatch_t pmatch[1];
  bool found = false;
  int last_match_line = -1;
  for (int i = buf -> current_line - 1; i >= 0; i--) {
    char * line_content = buf -> lines[i].content;
    int offset = 0;
    int last_match_offset = -1;
    while (regexec( & regex, line_content + offset, 1, pmatch, 0) == 0) {
      if (buf -> lines[i].matches.count >= buf -> lines[i].matches.capacity) {
        int new_capacity = buf -> lines[i].matches.capacity == 0 ? 4 : buf -> lines[i].matches.capacity * 2;
        SearchMatch * new_matches = realloc(buf -> lines[i].matches.matches, new_capacity * sizeof(SearchMatch));
        if (!new_matches) break;
        buf -> lines[i].matches.matches = new_matches;
        buf -> lines[i].matches.capacity = new_capacity;
      }
      SearchMatch match = {
        .start = offset + pmatch[0].rm_so,
        .end = offset + pmatch[0].rm_eo
      };
      buf -> lines[i].matches.matches[buf -> lines[i].matches.count++] = match;
      last_match_offset = offset;
      if (!found) {
        found = true;
        last_match_line = i;
      }
      offset += pmatch[0].rm_eo;
      if (pmatch[0].rm_so == pmatch[0].rm_eo) break;
    }
  }
  if (!found) {
    for (int i = buf -> count - 1; i >= buf -> current_line; i--) {
      char * line_content = buf -> lines[i].content;
      int offset = 0;
      int last_match_offset = -1;
      while (regexec( & regex, line_content + offset, 1, pmatch, 0) == 0) {
        if (buf -> lines[i].matches.count >= buf -> lines[i].matches.capacity) {
          int new_capacity = buf -> lines[i].matches.capacity == 0 ? 4 : buf -> lines[i].matches.capacity * 2;
          SearchMatch * new_matches = realloc(buf -> lines[i].matches.matches, new_capacity * sizeof(SearchMatch));
          if (!new_matches) break;
          buf -> lines[i].matches.matches = new_matches;
          buf -> lines[i].matches.capacity = new_capacity;
        }
        SearchMatch match = {
          .start = offset + pmatch[0].rm_so,
          .end = offset + pmatch[0].rm_eo
        };
        buf -> lines[i].matches.matches[buf -> lines[i].matches.count++] = match;
        last_match_offset = offset;
        if (!found) {
          found = true;
          last_match_line = i;
        }
        offset += pmatch[0].rm_eo;
        if (pmatch[0].rm_so == pmatch[0].rm_eo) break;
      }
    }
  }
  regfree( & regex);
  if (found) {
    buf -> current_line = last_match_line;
    buf -> screen_line = 0;
    for (int j = 0; j < last_match_line; j++) {
      buf -> screen_line += buf -> lines[j].wrapped_lines;
    }
  }
}
void process_command(Editor * ed) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return;
  if (strcmp(ed -> command_buffer, "q") == 0) {
    if (ed -> num_buffers > 1) {
      for (int i = ed -> current_buffer; i < ed -> num_buffers - 1; i++) {
        ed -> buffers[i] = ed -> buffers[i + 1];
      }
      ed -> num_buffers--;
      if (ed -> current_buffer >= ed -> num_buffers) {
        ed -> current_buffer = ed -> num_buffers - 1;
      }
    } else {
      endwin();
      editor_destroy(ed);
      exit(0);
    }
  } else if (strcmp(ed -> command_buffer, "n") == 0) {
    if (ed -> current_buffer < ed -> num_buffers - 1) {
      ed -> current_buffer++;
      clear();
      refresh();
    }
  } else if (strcmp(ed -> command_buffer, "p") == 0) {
    if (ed -> current_buffer > 0) {
      ed -> current_buffer--;
      clear();
      refresh();
    }
  } else if (strcmp(ed -> command_buffer, "l") == 0) {
    buf -> show_line_numbers = !buf -> show_line_numbers;
    clear();
    display_lines(ed);
  } else if (strncmp(ed -> command_buffer, "j", 1) == 0) {
    int line_number = 0;
    if (sscanf(ed -> command_buffer + 1, "%d", & line_number) == 1) {
      if (line_number > 0 && line_number <= buf -> count) {
        buf -> current_line = line_number - 1;
        buf -> screen_line = 0;
        for (int j = 0; j < buf -> current_line; j++) {
          buf -> screen_line += buf -> lines[j].wrapped_lines;
        }
        clear();
        refresh();
      } else {
        mvprintw(LINES - 1, 0, "Invalid line number");
        clrtoeol();
        refresh();
        napms(1000);
      }
    } else {
      mvprintw(LINES - 1, 0, "Invalid command: j requires a line number");
      clrtoeol();
      refresh();
      napms(1000);
    }
  } else if (strncmp(ed -> command_buffer, "s/", 2) == 0) {
    ed -> search_mode = 1;
    strncpy(ed -> search_buffer, ed -> command_buffer + 2, SEARCH_BUFFER_SIZE - 1);
    ed -> search_buffer[SEARCH_BUFFER_SIZE - 1] = '\0';
    if (!search_forward(ed, ed -> search_buffer)) {
      mvprintw(LINES - 1, 0, "Pattern not found");
      clrtoeol();
      refresh();
      napms(1000);
    } else {
      clear();
      refresh();
    }
  } else {
    mvprintw(LINES - 1, 0, "Invalid command");
    clrtoeol();
    refresh();
    napms(1000);
  }
  ed -> command_mode = 0;
  ed -> command_buffer[0] = '\0';
}
int handle_input(Editor * ed, int ch) {
  Buffer * buf = current_buffer(ed);
  if (!buf) return -1;
  if (ed -> command_mode) {
    if (ch == '\n') {
      process_command(ed);
    } else if (ch == 27) {
      ed -> command_mode = 0;
      ed -> command_buffer[0] = '\0';
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      int len = strlen(ed -> command_buffer);
      if (len > 0) ed -> command_buffer[len - 1] = '\0';
    } else if (isprint(ch)) {
      int len = strlen(ed -> command_buffer);
      if (len < COMMAND_BUFFER_SIZE - 1) {
        ed -> command_buffer[len] = ch;
        ed -> command_buffer[len + 1] = '\0';
      }
    }
  } else if (ed -> search_mode) {
    if (ch == '\n') {
      search_forward(ed, ed -> search_buffer);
      ed -> search_mode = 0;
    } else if (ch == 27) {
      ed -> search_mode = 0;
      ed -> search_buffer[0] = '\0';
    } else if (ch == KEY_BACKSPACE || ch == 127) {
      int len = strlen(ed -> search_buffer);
      if (len > 0) ed -> search_buffer[len - 1] = '\0';
    } else if (isprint(ch)) {
      int len = strlen(ed -> search_buffer);
      if (len < SEARCH_BUFFER_SIZE - 1) {
        ed -> search_buffer[len] = ch;
        ed -> search_buffer[len + 1] = '\0';
      }
    }
  } else {
    switch (ch) {
    case ':':
      ed -> command_mode = 1;
      ed -> command_buffer[0] = '\0';
      break;
    case '/':
      ed -> search_mode = 1;
      ed -> search_buffer[0] = '\0';
      break;
    case 'n':
      if (strlen(ed -> search_buffer) > 0) {
        search_forward(ed, ed -> search_buffer);
      }
      break;
    case 'p':
      if (strlen(ed -> search_buffer) > 0) {
        search_backward(ed, ed -> search_buffer);
      }
      break;
    case KEY_DOWN:
      buf -> screen_line++;
      int file_line, wrap_index;
      screen_to_file_position(ed, buf -> screen_line, & file_line, & wrap_index);
      buf -> current_line = file_line;
      break;
    case KEY_UP:
      if (buf -> screen_line > 0) {
        buf -> screen_line--;
        int file_line, wrap_index;
        screen_to_file_position(ed, buf -> screen_line, & file_line, & wrap_index);
        buf -> current_line = file_line;
      }
      break;
    case ' ': {
      int page_size = LINES - 3;
      buf -> screen_line += page_size;
      int file_line, wrap_index;
      screen_to_file_position(ed, buf -> screen_line, & file_line, & wrap_index);
      buf -> current_line = file_line;
    }
    break;
    case 'b': {
      int page_size = LINES - 3;
      if (buf -> screen_line > page_size) {
        buf -> screen_line -= page_size;
      } else {
        buf -> screen_line = 0;
      }
      int file_line, wrap_index;
      screen_to_file_position(ed, buf -> screen_line, & file_line, & wrap_index);
      buf -> current_line = file_line;
    }
    break;
    case 'q':
      return -1;
    case ']':
      strcpy(ed -> command_buffer, "n");
      process_command(ed);
      break;
    case '[':
      strcpy(ed -> command_buffer, "p");
      process_command(ed);
      break;
    }
  }
  return 0;
}
int load_file(Editor * ed,
  const char * fname) {
  Buffer * buf = editor_new_buffer(ed);
  if (!buf) return -1;
  FILE * file = fopen(fname, "r");
  if (!file) {
    ed -> num_buffers--;
    return -1;
  }
  buf -> filename = strdup(fname);
  if (!buf -> filename) {
    fclose(file);
    ed -> num_buffers--;
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
void print_help(const char * prog_name) {
  printf("Usage: %s [OPTIONS] [PIPE_INPUT] | [FILE...]\n", prog_name);
  printf("\nA terminal-based text editor that accepts piped input and files for editing.\n");
  printf("\nOptions:\n");
  printf(" -h, --help Show this help message and exit.\n");
  printf(" -v, --version Display the version information and exit.\n");
  printf("\nArguments:\n");
  printf(" PIPE_INPUT Input provided through a pipe (supports multiple piped inputs).\n");
  printf(" FILE... One or more files to open and edit (provided after the program name).\n");
  printf("\nExamples:\n");
  printf(" cat input.txt | %s Pipe the content of 'input.txt' into the editor.\n", prog_name);
  printf(" %s file1.txt file2.txt\n", prog_name);
  printf(" Edit multiple files: 'file1.txt' and 'file2.txt'.\n");
  printf(" command1 | command2 | %s\n", prog_name);
  printf(" Pipe the output from multiple commands into the editor.\n");
  printf(" %s file1.txt file2.txt | command1 | command2\n", prog_name);
  printf(" A mix of files and piped input from multiple commands.\n");
  printf(" Multi buffer from piped input\n");
  printf(" (command 1; echo -e '\0'; command2; echo -e '\0'; command3;) |");
  printf("\nNotes:\n");
  printf(" - Piped input must appear before the program name (e.g., 'command1 | command2 | %s').\n", prog_name);
  printf(" - Files must be listed after the program name (e.g., '%s file1.txt file2.txt').\n", prog_name);
  printf(" - If no input is provided (no files or pipes), this help message will be shown.\n");
}
void print_version() {
  printf("Least version 0.8\n");
}
char * get_running_command() {
  FILE * fp = fopen("/proc/self/cmdline", "r");
  if (!fp) {
    perror("fopen");
    return NULL;
  }
  char * cmd = malloc(1024);
  if (!cmd) {
    perror("malloc");
    fclose(fp);
    return NULL;
  }
  size_t len = fread(cmd, 1, 1024, fp);
  if (len == 0) {
    free(cmd);
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  cmd[len] = '\0';
  return cmd;
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
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGWINCH, handle_resize);
    int need_reopen_tty = 0;
    int buffers_created = 0;
    if (argc >= 3 && (strcmp(argv[1], "--multi") == 0 || strcmp(argv[1], "-m") == 0)) {
        for (int i = 2; i < argc; i++) {
            FILE *pipe = popen(argv[i], "r");
            if (!pipe) {
                fprintf(stderr, "Failed to execute command: %s\n", argv[i]);
                continue;
            }
            Buffer *cmd_buf = editor_new_buffer(ed);
            if (!cmd_buf) {
                fprintf(stderr, "Failed to create buffer for command: %s\n", argv[i]);
                pclose(pipe);
                continue;
            }
            cmd_buf->filename = strdup(argv[i]);
            if (!cmd_buf->filename) {
                fprintf(stderr, "Failed to allocate memory for filename: %s\n", argv[i]);
                ed->num_buffers--; 
                pclose(pipe);
                continue;
            }
            char line[MAX_LINE_LENGTH];
            int line_count = 0;
            while (fgets(line, sizeof(line), pipe)) {
                if (editor_append_line(cmd_buf, line, strlen(line)) < 0) {
                    fprintf(stderr, "Failed to process command output: %s\n", argv[i]);
                    break;
                }
                line_count++;
            }
            pclose(pipe);
            if (line_count == 0) {
                free(cmd_buf->filename);
                ed->num_buffers--;
                fprintf(stderr, "Command produced no output: %s\n", argv[i]);
            } else {
                buffers_created++;
            }
        }
        if (buffers_created == 0) {
            fprintf(stderr, "No commands produced output\n");
            editor_destroy(ed);
            return 1;
        }
    } else {
        if (!isatty(STDIN_FILENO)) {
            need_reopen_tty = 1;
            Buffer *pipe_buf = NULL;
            char pipe_name[32];
            int pipe_count = 0;
            size_t line_pos = 0;
            int has_content = 0;
            char *line_buffer = malloc(MAX_LINE_LENGTH * 2);
            if (!line_buffer) {
                fprintf(stderr, "Failed to allocate memory for input buffer\n");
                editor_destroy(ed);
                return 1;
            }
            int ch;
            while ((ch = fgetc(stdin)) != EOF) {
                if (ch == '\0') {
                    if (line_pos > 0) {
                        line_buffer[line_pos] = '\0';
                        if (pipe_buf && editor_append_line(pipe_buf, line_buffer, line_pos) < 0) {
                            fprintf(stderr, "Failed to process pipe input\n");
                            break;
                        }
                        line_pos = 0;
                        has_content = 1;
                    }
                    if (!pipe_buf || has_content) {
                        if (pipe_buf) {
                            buffers_created++;
                        }
                        pipe_buf = editor_new_buffer(ed);
                        if (!pipe_buf) {
                            fprintf(stderr, "Failed to create buffer for pipe input\n");
                            free(line_buffer);
                            editor_destroy(ed);
                            return 1;
                        }
                        snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                        pipe_buf->filename = strdup(pipe_name);
                        if (!pipe_buf->filename) {
                            fprintf(stderr, "Failed to allocate memory for pipe name\n");
                            ed->num_buffers--; 
                            free(line_buffer);
                            editor_destroy(ed);
                            return 1;
                        }
                        has_content = 0;
                    }
                    continue;
                }
                line_buffer[line_pos++] = ch;
                if (ch == '\n' || line_pos >= MAX_LINE_LENGTH - 1) {
                    line_buffer[line_pos] = '\0';
                    if (!pipe_buf) {
                        pipe_buf = editor_new_buffer(ed);
                        if (!pipe_buf) {
                            fprintf(stderr, "Failed to create buffer for pipe input\n");
                            free(line_buffer);
                            editor_destroy(ed);
                            return 1;
                        }
                        snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                        pipe_buf->filename = strdup(pipe_name);
                        if (!pipe_buf->filename) {
                            fprintf(stderr, "Failed to allocate memory for pipe name\n");
                            ed->num_buffers--; 
                            free(line_buffer);
                            editor_destroy(ed);
                            return 1;
                        }
                    }
                    if (editor_append_line(pipe_buf, line_buffer, line_pos) < 0) {
                        fprintf(stderr, "Failed to process pipe input\n");
                        free(line_buffer);
                        editor_destroy(ed);
                        return 1;
                    }
                    has_content = 1;
                    line_pos = 0;
                }
            }
            if (line_pos > 0) {
                line_buffer[line_pos] = '\0';
                if (!pipe_buf) {
                    pipe_buf = editor_new_buffer(ed);
                    if (!pipe_buf) {
                        fprintf(stderr, "Failed to create buffer for pipe input\n");
                        free(line_buffer);
                        editor_destroy(ed);
                        return 1;
                    }
                    snprintf(pipe_name, sizeof(pipe_name), "pipe-%d", ++pipe_count);
                    pipe_buf->filename = strdup(pipe_name);
                    if (!pipe_buf->filename) {
                        fprintf(stderr, "Failed to allocate memory for pipe name\n");
                        ed->num_buffers--; 
                        free(line_buffer);
                        editor_destroy(ed);
                        return 1;
                    }
                }
                if (editor_append_line(pipe_buf, line_buffer, line_pos) < 0) {
                    fprintf(stderr, "Failed to process pipe input\n");
                    free(line_buffer);
                    editor_destroy(ed);
                    return 1;
                }
                has_content = 1;
            }
            free(line_buffer);
            if (has_content) {
                buffers_created++;
            }
        }
        if (need_reopen_tty) {
            if (freopen("/dev/tty", "r", stdin) == NULL) {
                fprintf(stderr, "Failed to reopen tty\n");
                editor_destroy(ed);
                return 1;
            }
        }
        for (int i = 1; i < argc; i++) {
            if (load_file(ed, argv[i]) < 0) {
                fprintf(stderr, "Failed to load file %s: %s\n", argv[i], strerror(errno));
            } else {
                buffers_created++;
            }
        }
    }
    if (buffers_created == 0) {
        fprintf(stderr, "No input sources available. Usage:\n");
        print_help(argv[0]);
        editor_destroy(ed);
        return 1;
    }
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
        init_pair(10, COLOR_BLACK, COLOR_YELLOW);
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
