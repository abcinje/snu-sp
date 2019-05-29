#include "cache.h"

void buf_clear(buf_t *buf)
{
	buf->cnt = 0;
}

int buf_fill(buf_t *buf, void *usrbuf, size_t n)
{
	if (buf->cnt + n > MAX_OBJECT_SIZE)
		return -1;

	strncpy(buf->buf + buf->cnt, usrbuf, n);
	buf->cnt += n;

	return 0;
}

node_t *node_new(char *uri, buf_t *buf)
{
	node_t *node = Malloc(sizeof(node_t));
	int size = buf->cnt;

	node->uri = Malloc(MAXURI);
	node->node_buf = Malloc(size);

	node->bufsize = size;
	strncpy(node->uri, uri, strlen(uri));
	strncpy(node->node_buf, buf->buf, size);
	node->next = NULL;

	return node;
}

void node_delete(node_t *node)
{
	free(node->uri);
	free(node->node_buf);
	free(node);
}

void cache_init(cache_t *cache)
{
	cache->size = 0;
	cache->head = NULL;
}

int cache_isempty(cache_t *cache)
{
	if (cache->head)	
		return 0;
	else
		return 1;
}

void cache_enqueue(cache_t *cache, node_t *node)
{
	cache->size += (node->bufsize + MAXURI);
	node->next = cache->head;
	cache->head = node;
}

node_t *cache_dequeue(cache_t *cache)
{
	node_t *node, *temp;

	if (cache_isempty(cache))
		return NULL;

	node = cache->head;
	while (node->next->next)
		node = node->next;

	temp = node->next;
	node->next = NULL;

	cache->size -= (temp->bufsize + MAXURI);
	return temp;
}

void cache_evict(cache_t *cache, int request)
{
	while (cache->size + (request + MAXURI) > MAX_CACHE_SIZE)
		node_delete(cache_dequeue(cache));
}

void cache_write(cache_t *cache, char *uri, buf_t *buf)
{
	node_t *node = node_new(uri, buf);
	cache_evict(cache, node->bufsize);
	cache_enqueue(cache, node);
}

node_t *cache_remove_next(cache_t *cache, node_t *node)
{
	node_t *temp;

	if (!(temp = node->next))
		return NULL;

	node->next = temp->next;
	temp->next = NULL;

	cache->size -= (temp->bufsize + MAXURI);
	return temp;
}

int cache_read(cache_t *cache, char *uri, buf_t *buf)
{
	node_t *prev, *node;

	if (!(prev = cache->head))
		return -1;
	
	node = prev->next;
	while (node) {
		if (!strncmp(node->uri, uri, strlen(uri))) {
			strncpy(buf->buf, node->node_buf, node->bufsize);
			buf->cnt = node->bufsize;
			cache_enqueue(cache, cache_remove_next(cache, prev));
			return node->bufsize;
		}
		prev = node;
		node = node->next;
	}

	return -1;
}

