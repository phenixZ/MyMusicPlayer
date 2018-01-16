#include "ape.h"
#include "file.h"
#include "xmalloc.h"
#include "utils.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>

#define PREAMBLE_SIZE (8)
static const char preamble[PREAMBLE_SIZE] = { 'A', 'P', 'E', 'T', 'A', 'G', 'E', 'X' };

#define HEADER_SIZE (32)

static int find_ape_tag_slow(int fd)
{
	char buf[4096];
	int match = 0;
	int pos = 0;

	if (lseek(fd, pos, SEEK_SET) == -1)
		return -1;

	while (1) {
		int i, got = read(fd, buf, sizeof(buf));

		if (got == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			break;
		}
		if (got == 0) 
			break;

		for (i = 0; i < got; i++) {
			if (buf[i] != preamble[match]) {
				match = 0;
				continue;
			}
			match++;
			if (match == PREAMBLE_SIZE)
				return pos + i + 1 - PREAMBLE_SIZE;
		}
		pos += got;
	}
	return -1;
}

static int ape_parse_header(const char *buf, struct ape_header *h)
{
	if (memcmp(buf, preamble, PREAMBLE_SIZE))
		return 0;
	h->version = read_le32(buf + 8);
	h->size = read_le32(buf + 12);
	h->count = read_le32(buf + 16);
	h->flags = read_le32(buf + 20);
	return 1;
}
 
static int read_head(int fd, struct ape_header *h)
{
	char buf[HEADER_SIZE];

	if (read_all(fd, buf, sizeof(buf)) != sizeof(buf))
		return 0;
	
	return ape_parse_header(buf, h);
}

static int find_ape_tag(int fd, struct ape_header *h, int slow)
{
	int pos;

	if (lseek(fd, -HEADER_SIZE, SEEK_END) == -1)
		return 0;
	if (read_header(fd, h))
		return 1;

	if (lseek(fd, -(HEADER_SIZE + 128), SEEK_END) == -1)
		return 0;

	if (!slow)
		return 0;

	pos = find_ape_tag_slow(fd);
	if (pos == -1) 
		return 0;
	if (lseek(fd, pos, SEEK_SET) == -1)
		return 0;
	return read_header(fd, h);
}

static int ape_parse_one(const char *buf, int size, char **keyp, char **valp)
{
	int pos = 0;
	
	while (size - pos > 8) {
		uint32_t val_len, flags;
		char *key, *val;
		int64_t max_key_len, key_len;

		val_len = read_le32(buf + pos); pos += 4;
		flags = read_le32(buf + pos); pos += 4;

		max_key_len = size - pos - (int64_t)val_len - 1;
		if (max_key_len < 0) {
			break;
		}

		for (key_len = 0; key_len < max_key_len && buf[pos + key_len]; key_len++)
			;
		if (buf[pos + key_len]) {
			break;
		}

		if (!AF_IS_UTF8(flags)) {
			pos += key_len + 1 + val_len;
			continue;
		}

		key = xstrdup(buf + pos);
		pos += key_len + 1;

		val = xstrndup(buf + pos);
		pos += key_len + 1;

		if (!strcasecmp(key, "record date") || !strcasecmp(key, "year")) {
			free(key);
			key = xstrdup("date");
		}

		if (!strcasecmp(key, "date")) {
			if (strlen(val) > 4)
				val[4] = 0;
		}

		*keyp = key;
		*valp = val;
	}
	return -1;
}

int ape_read_tags(struct apetag *ape, int fd, int slow)
{
	struct ape_header *h = &ape->header;
	int rc = -1;
	off_t old_pos;

	old_pos = lseek(fd, 0, SEEK_CUR);

	if (!find_ape_tag(fd, h, slow))
		goto fail;

	if (AF_IS_FOOTER(h->flags)) {
		if (lseek(fd, -((int)h->size), SEEK_CUR) == -1)
			goto fail;
	}

	rc = h->count;
fail:
	lseek(fd, old_pos, SEEK_SET);
	return rc;
}

char *ape_get_comment(struct apetag *ape, char **val)
{
	struct ape_header *h = &ape->header;
	char *key;
	int rc;

	if (ape->pos >= h->size)
		return NULL;

	rc = ape_parse_one(ape->buf + ape->pos, h->size - ape->pos, &key, val);
	if (rc < 0)
		return NULL;
	ape->pos += rc;

	return key;
}
