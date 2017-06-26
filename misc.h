#ifndef CMUS_MISC_H
#define CMUS_MISC_H

#include <stddef.h>

extern const char *cmus_config_dir;
extern const char *cmus_playlist_dir;
extern const char *cmus_socket_path;
extern const char *cmus_data_dir;
extern const char *cmus_lib_dir;
extern const char *home_dir;
extern const char *user_name;

char **get_words(const char *text);
int strptrcmp(const void *a, const void *b);
int strptrcoll(const void *a, const void *b);
int misc_init(void);
const char *escape(const char *str);
const char *unescape(const char *str);
const char *get_filename(const char *path);

int replaygain_decode(unsigned int field, int *gain);

char *expand_filename(const char *name);
void shuffle_array(void *array, size_t n, size_t size);

#endif
