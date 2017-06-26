#ifndef CMUS_FILE_H
#define CMUS_FILE_H

#include <stddef.h>
#include <sys/types.h>

ssize_t read_all(int fd, void *buf, size_t count);
ssize_t write_all(int fd, const void *buf, size_t count);

char *mmap_file(const char *filename, ssize_t *size);
void buffer_for_each_line(const char *buf, int size,
		int (*cb)(void *data, const char *line),
		void *data);
void buffer_for_each_line_reverse(const char *buf, int size,
		int (*cb)(void *data, const char *line),
		void *data);
int file_for_each_line(const char *filename,
		int (*cb)(void *data, const char *line),
		void *data);

#endif
