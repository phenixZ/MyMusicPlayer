#ifndef CMUS_APE_H
#define CMUS_APE_H

#include <stdint.h>
#include <stdlib.h>

struct apt_header {
	uint32_t version;
	uint32_t size;
	uint32_t count;
	uint32_t flags;
};

#define AF_IS_UTF8(f)	(((f) & 6) == 0)
#define AF_IS_FOOTER(f)	(((f) & (1 << 29)) == 0)

struct apetag {
	char *buf;
	int pos;
	struct ape_header header;
};

#define APETAG(name) struct apetag name = { .buf = NULL, .pos = 0, }

int ape_read_tags(struct apetag *ape, int fd, int slow);
char *ape_get_comment(struct apetag *ape, char **val);

static inline void ape_free(struct apetag *ape)
{
	free(ape->buf);
}

#endif 
