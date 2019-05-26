#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void *handle_client(void *vargp);
void proxy(int fd);
int parse_uri(char *uri, char **host, char **port, char **path);

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

	listenfd = Open_listenfd(argv[1]);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, handle_client, connfdp);
	}

	return 0;
}

void *handle_client(void *vargp)
{
	int connfd = *((int *)vargp);
	Pthread_detach(Pthread_self());
	Free(vargp);
	proxy(connfd);
	Close(connfd);
	return NULL;
}

void proxy(int client_fd)
{
	rio_t client_rio, server_rio;
	char buf[MAXBUF], method[16], uri[MAXBUF], ver[16], type[32];
	char *host, *port, *path, *temp;
	int server_fd, stat_code;
	ssize_t n, sum;

	/* Get a request */
	Rio_readinitb(&client_rio, client_fd);
	if (!Rio_readlineb(&client_rio, buf, MAXBUF))
		return;

	/* Read the request line */
	sscanf(buf, "%s %s %s", method, uri, ver);
	if (parse_uri(uri, &host, &port, &path))
		return;

	/* Connect to the server */
	server_fd = Open_clientfd(host, port);
	Rio_readinitb(&server_rio, server_fd);

	printf("%s %s\n", method, uri);

	/* Support only "GET" method */
	if (strcasecmp(method, "GET")) {
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
		if (!strcmp(buf, "\r\n"))
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
		else if (!strcmp(buf, "\r\n"))
			break;
	}

	sum = 0;
	while ((n = Rio_readlineb(&server_rio, buf, MAXBUF))) {
		Rio_writen(client_fd, buf, n);
		sum += n;
	}
	printf("  ← %d %s %ld\n", stat_code, type, sum);
}

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
