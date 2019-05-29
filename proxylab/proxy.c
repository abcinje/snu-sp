#define CACHE_ENABLED

#include "csapp.h"
#include "cache.h"

#ifdef CACHE_ENABLED
cache_t cache;
#endif

static void *handle_client(void *vargp);
static void proxy(int client_fd);
static int parse_uri(char *uri, char **host, char **port, char **path);

/* main routine */
int main(int argc, char *argv[])
{
	int listenfd, *connfdp;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}

#ifdef CACHE_ENABLED
	cache_init(&cache);
#endif

	/* listening socket */
	listenfd = Open_listenfd(argv[1]);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, handle_client, connfdp);
	}

	return 0;
}

/* client-side thread */
void *handle_client(void *vargp)
{
	int connfd = *((int *)vargp);
	Pthread_detach(Pthread_self());
	Free(vargp);
	proxy(connfd);
	Close(connfd);
	return NULL;
}

/* proxy */
void proxy(int client_fd)
{
	rio_t client_rio, server_rio;
	char buf[MAXBUF], method[16], uri[MAXURI], ver[16], type[32];
	char *host, *port, *path, *temp;
	int server_fd, stat_code;
	ssize_t n, sum;
	
#ifdef CACHE_ENABLED
	char cache_key[MAXURI];
	buf_t cache_buf;
	int cache_buf_failed = 0;
#endif

	/* Get a request */
	Rio_readinitb(&client_rio, client_fd);
	if (!Rio_readlineb(&client_rio, buf, MAXBUF))
		return;

	/* Read the request line */
	sscanf(buf, "%s %s %s", method, uri, ver);

#ifdef CACHE_ENABLED
	/* Read the corresponding data from the cache if it exists */
	strncpy(cache_key, uri, strlen(uri));
	buf_clear(&cache_buf);
	if ((n = cache_read(&cache, cache_key, &cache_buf)) >= 0) {
		Rio_writen(client_fd, cache_buf.buf, n);
		sum = n;
		return;
	}
#endif

	/* Parse the request line */
	if (parse_uri(uri, &host, &port, &path))
		return;

	/* Connect to the server */
	server_fd = Open_clientfd(host, port);
	Rio_readinitb(&server_rio, server_fd);

	printf("%s %s\n", method, uri);

	/* Support only "GET" method */
	if (strncasecmp(method, "GET", 3)) {
		/* TODO: body */
		sprintf(buf, "%d %s %s\r\n", 501, "Not Implemented", ver);
		Rio_writen(client_fd, buf, MAXBUF);
		printf("  ← %d %s %d\n", 501, "text/html", 0);
		return;
	}

	/* Forward the request line */
	sprintf(buf, "GET /%s %s\r\n", path, ver);
	Rio_writen(server_fd, buf, strlen(buf));

	/* Forward the request headers */
	while ((n = Rio_readlineb(&client_rio, buf, MAXBUF))) {
		Rio_writen(server_fd, buf, strlen(buf));
		if (!strncmp(buf, "\r\n", 2))
			break;
	}

	/* Forward the response */
	while ((n = Rio_readlineb(&server_rio, buf, MAXBUF))) {
		Rio_writen(client_fd, buf, n);
		if (!strncmp(buf, "HTTP", 4))
			sscanf(buf, "%s %d", ver, &stat_code);
		else if (!strncmp(buf, "Content-Type", 12)) {
			strtok(buf, " ");
			temp = strtok(NULL, " ");
			strtok(temp, "\r");
			strtok(temp, ";");
			strncpy(type, temp, strlen(temp));
		}
		else if (!strncmp(buf, "\r\n", 2))
			break;
	}
	sum = 0;
	while ((n = Rio_readlineb(&server_rio, buf, MAXBUF))) {
		Rio_writen(client_fd, buf, n);
		sum += n;

#ifdef CACHE_ENABLED
		/* Do not cache the data
		   if the cache buffer is too small to store the data */
		if (buf_fill(&cache_buf, buf, n) < 0)
			cache_buf_failed = 1;
#endif
	}

#ifdef CACHE_ENABLED
	/* Store the data to the cache buffer */
	if (!cache_buf_failed)
		cache_write(&cache, cache_key, &cache_buf);
#endif

	printf("  ← %d %s %ld\n", stat_code, type, sum);
}

/* URI parser */
int parse_uri(char *uri, char **host, char **port, char **path)
{
	char *scheme = "http://";
	static char *http_wkport = "80";

	if (strncasecmp(uri, scheme, strlen(scheme)))
		return 1;

	/* host */
	*host = uri + strlen(scheme);

	/* path */
	strtok(*host, "/");
	*path = *host + strlen(*host) + 1;

	/* port */
	strtok(*host, ":");
	if (!(*port = strtok(NULL, ":")))
		*port = http_wkport;

	return 0;
}

