#ifndef CMUS_XMALLOC_H
#define CMUS_XMALLOC_H

#include "compiler.h"
#ifdef HAVE_CONFIG
#include "config/xmalloc.h"
#endif

#include <stdlib.h>
#include <string.h>

void malloc_fail(void) CMUS_NORETURN;

#define xnew(type, n)		(type *)xmalloc(sizeof(type) * (n))
#define xnew0(type, n)		(type *)xmalloc0(sizeof(type) * (n))
#define xrenew(type, mem, n)	(type *)xrealloc(mem, sizeof(type) * (n))

static inline void * CMUS_MALLOC xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (unlikely(ptr == NULL))
		malloc_fail();
	return ptr;
}

static inline void * CMUS_MALLOC xmalloc0(size_t size)
{
	void *ptr = calloc(1, size);
	
	if (unlikely(ptr == NULL))
		malloc_fail();
	return ptr;
}

static inline void * CMUS_MALLOC xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (unlikely(ptr == NULL))
		malloc_fail();
	return ptr;
}


static inline char * CMUS_MALLOC xstrdup(const char *str)
{
#ifdef HAVE_STRDUP
	char *s = strdup(str);
	if (unlikely(s == NULL))
		malloc_fail();
	return s;
#else
	size_t size = strlen(str) + 1;
	void *ptr = xmalloc(size);
	return (char *)memcpy(ptr, str, size);
#endif
}


#ifdef HAVE_CONFIG
static inline char * CMUS_MALLOC xstrndup(const char *str, size_t n)
{
	char *s = strndup(str, n);
	if (unlikely(s == NULL))
		malloc_fail();
	return s;
}
#else 
char * CMUS_MALLOC xstrndup(const char *str, size_t n);
#endif

static inline void free_str_array(char **array)
{
	int i;
	
	if (array == NULL)
		return;
	for (i = 0; array[i]; i++)
		free(array[i]);
	free(array);
}
#endif
