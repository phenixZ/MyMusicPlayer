#ifndef CMUS_UI_CURSES_H
#define CMUS_UI_CURSES_H

#include "search.h"
#include "compiler.h"
#include "format_print.h"

enum ui_input_mode {
	NORMAL_MODE,
	COMMAND_MODE,
	SEARCH_MODE
};

#include <signal.h>

extern volatile sig_atomic_t cmus_running;
extern int ui_initialized;
extern enum ui_input_mode input_mode;
extern int cur_view;
extern int prev_view;
extern struct searchable *searchable;

extern char *lib_filename;
extern char *lib_ext_filename;
extern char *pl_filename;
extern char *play_queue_filename;
extern char *plqy_queue_ext_filename;

extern char *charset;
extern int using_utf8;

void update_titleline(void);
void update_statusline(void);
void update_filterline(void);
void update_colors(void);
void update_full(void);
void info_msg(const char *format, ...) CMUS_FORMAT(1, 2);
void error_msg(const char *format, ...) CMUS_FORMAT(1, 2);
int yes_no_query(const char *format, ...) CMUS_FORMAT(1, 2);
void search_not_found(void);
void set_view(int view);
void set_client_fd(int fd);
int get_client_fd(void);
void enter_command_mode(void);
void enter_search_mode(void);
void enter_search_backward_mode(void);

int track_format_valid(const char *format);

const char *get_stream_title(void);
const struct format_option *get_global_fopts(void);

int get_track_win_x(void);

#endif
