#ifndef CMUS_PATH_H
#define CMUS_PATH_H

const char *get_extension(const char *filename);
const char *path_basename(const char *path);
void path_strip(char *str);
char *path_absolute_cwd(const char *src, const char *cwd);
char *path_absolute(const char *src);

#endif
