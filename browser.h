#ifndef CMUS_BROWSER_H
#define CMUS_BROWSER_H

#include "list.h"
#include "window.h"
#include "search.h"

struct browser_entry {
	struct list_head node;

	enum { BROWSER_ENTRY_DIR, BROWSER_ENTRY_FILE, BROWSER_ENTRY_PLLINE } type;
	char name[];
};

static inline struct browser_entry *iter_to_browser_entry(struct iter *iter)
{
	return iter->data1;
}

extern struct window *browser_win;
extern char *browser_dir;
extern struct searchable *browser_searchable;

void browser_init(void);
void browser_exit(void);
int browser_chdir(const char *dir);
char *browser_get_sel(void);
void browser_up(void);
void browser_enter(void);
void browser_delete(void);
void browser_reload(void);
void browser_toggle_show_hidden(void);

#endif
