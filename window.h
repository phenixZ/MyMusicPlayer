#ifndef CMUS_WINDOW_H
#define CMUS_WINDOW_H

#include "iter.h"

struct window {

	struct iter head;
	struct iter top;
	struct iter sel;
	int nr_rows;
	unsigned changed : 1;

	int (*get_prev)(struct iter *iter);
	int (*get_next)(struct iter *iter);
	int (*selectable)(struct iter *iter);
	void (*sel_changed)(void);
};

struct window *window_new(int (*get_prev)(struct iter*), int (*get_next)(struct iter *));
void window_free(struct window *win);
void window_set_empty(struct window *win);
void window_set_contents(struct window *win, void *head);
void window_changed(struct window *win);
void window_row_vanished(struct window *win, struct iter *iter);
int window_get_top(struct window *win, struct iter *iter);
int window_get_sel(struct window *win, struct iter *iter);
int window_get_prev(struct window *win, struct iter *iter);
int window_get_next(struct window *win, struct iter *iter);

void window_set_sel(struct window *win, struct iter *iter);
void window_set_nr_rows(struct window *win, int nr_rows);
void window_up(struct window *win, int rows);
void window_down(struct window *win, int rows);
void window_goto_top(struct window *win);
void window_goto_bottom(struct window *win);
void window_page_up(struct window *win);
void window_half_page_up(struct window *win);
void window_page_down(struct window *win);
void window_half_page_down(struct window *win);
void window_scroll_down(struct window *win);
void window_scroll_up(struct window *win);
void window_page_top(struct window *win);
void window_page_bottom(struct window *win);
void window_page_middle(struct window *win);
int window_get_nr_rows(struct window *win);

#endif
