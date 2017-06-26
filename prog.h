#ifdef CMUS_PROG_H
#define CMUS_PROG_H

#include "compiler.h"

extern char *program_name;

struct option {
	int short_opt;
	const char *long_opt;
	int has_arg;
};

int get_option(char **argv[], const struct option *options, char **arg);

void warn(const char *format, ...) CMUS_FORMAT(1, 2);
void warn_errno(const char *format, ...) CMUS_FORMAT(1, 2);
void die(const char *format, ...) CMUS_FORMAT(1, 2) CMUS_NORETURN;
void die_errno(const char *format, ...) CMUS_FORMAT(1, 2) CMUS_NORETURN;

#endif
