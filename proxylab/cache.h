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

/* buf_t methods */
void buf_clear(buf_t *buf);
int buf_fill(buf_t *buf, void *usrbuf, size_t n);

/* node_t methods */
node_t *node_new(char *uri, buf_t *buf);
void node_delete(node_t *node);

/* cache_t methods */
void cache_init(cache_t *cache);
int cache_isempty(cache_t *cache);
void cache_enqueue(cache_t *cache, node_t *node);
node_t *cache_dequeue(cache_t *cache);
void cache_evict(cache_t *cache, int request);
void cache_write(cache_t *cache, char *uri, buf_t *buf);
int cache_read(cache_t *cache, char *uri, buf_t *buf);

#endif /* __CACHE_H__ */

