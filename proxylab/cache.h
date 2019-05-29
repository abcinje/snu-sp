#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAXURI 1024

typedef struct {
	int cnt;
	char buf[MAX_OBJECT_SIZE];
} buf_t;

typedef struct __node {
	int bufsize;
	char *uri;
	char *node_buf;
	struct __node *next;
} node_t;

typedef struct {
	int size;
	node_t *head;
} cache_t;

/* buf_t APIs */
void buf_clear(buf_t *buf);
int buf_fill(buf_t *buf, void *usrbuf, size_t n);

/* cache_t APIs */
void cache_init(cache_t *cache);
int cache_read(cache_t *cache, char *uri, buf_t *buf);
void cache_write(cache_t *cache, char *uri, buf_t *buf);

#endif /* __CACHE_H__ */

