// Oliande, Bryan: 13179240


//LAB 4 SUBMISSION

#include "csapp.h"

FILE *log_file;

int parse_uri(char *uri, char *target_addr, char *path, char  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void doit(int fd, struct sockaddr_in addr);

int main(int argc, char **argv)
{

	int numRequests = 0;
	int listenfd, connfd, clientlen;
    char *port = argv[1];
    struct sockaddr_in clientaddr;

    clientlen = sizeof(clientaddr);

    if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

	signal(SIGPIPE, SIG_IGN);

	listenfd = Open_listenfd(port);

	log_file = Fopen("proxy.log", "a");

	while (1) {
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        doit(connfd, clientaddr);
	}

	exit(0);
}

int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}

	// Host
    hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

    // Port
	strcpy(port, "80");
	if (*hostend == ':')
		strcpy(port, hostend + 1);


	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) {
		pathname[0] = '\0';
	}
	else {
		pathbegin++;
		strcpy(pathname, pathbegin);
	}

	return 0;
}

//As per the lab4 specifications, this function was provided to us.
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}

void doit(int fd, struct sockaddr_in addr)
{

    struct sockaddr_in clientaddr;
    int connfd;
    int serverfd;
    char request[MAXLINE];                  //client's http request
    char *request_uri;              // Start of URI in first HTTP request header line
    char *request_uri_end;          // End of URI in first HTTP request header line
    char *rest_of_request;          // Beginning of second HTTP request header line
    int request_len;                // Total size of HTTP request
    int response_len;               // Total size in bytes of response from end server
	int i;
	int n;

    char hostname[MAXLINE];
    char pathname[MAXLINE];
    char port[MAXLINE];
    char log_entry[MAXLINE];

    rio_t rio;
    char buf[MAXLINE];

    connfd = fd;
    clientaddr = addr;

	//reads the HTTP request into the buffer
    request[0] = '\0';
    request_len = 0;
    Rio_readinitb(&rio, connfd);
    while (1) {
        if ((n = rio_readlineb(&rio, buf, MAXLINE)) <= 0) {
            printf("doIt: Bad request.\n");
            close(connfd);
            return;
        }

        strcat(request, buf);
        request_len += n;

        //check if request is terminated properly
        if (strcmp(buf, "\r\n") == 0)
            break;
    } //end of while



   //if request is non-get return
	if (strncmp(request, "GET ", strlen("GET "))) {
        printf("Error: not GET request\n");
        close(connfd);
        return;
    }
    request_uri = request + 4;

    //extracting URI
    request_uri_end = NULL;
    for (i = 0; i < request_len; i++) {
        if (request_uri[i] == ' ') {
            request_uri[i] = '\0';
            request_uri_end = &request_uri[i];
            break;
        }
    }


	//checking for a request without a terminating blank
    if (i == request_len) {
        printf("Error: URI no end\n");
        close(connfd);
        return;
    }

    //checking for correct HTTP version
    if (strncmp(request_uri_end + 1, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n")) &&
        strncmp(request_uri_end + 1, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"))) {
        printf("Error: HTTP error.\n");
        close(connfd);
        return;
    }


	//forward remaining lines to end server w/o changing anything
    rest_of_request = request_uri_end + strlen("HTTP/1.0\r\n") + 1;

    //parsing the URI
    if (parse_uri(request_uri, hostname, pathname, port) < 0) {
        printf("Error: URI\n");
        close(connfd);
        return;
    }

    // send request to end server
    if ((serverfd = open_clientfd(hostname, port)) < 0) {
        printf("Error: end server\n");
        return;
    }
    rio_writen(serverfd, "GET /", strlen("GET /"));
    rio_writen(serverfd, pathname, strlen(pathname));
    rio_writen(serverfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    rio_writen(serverfd, rest_of_request, strlen(rest_of_request));

    //receive reply from server and forward to client
    Rio_readinitb(&rio, serverfd);
    response_len = 0;
    while ((n = rio_readn(serverfd, buf, MAXLINE)) > 0) {
        response_len += n;
        rio_writen(connfd, buf, n);

        bzero(buf, MAXLINE);
    }

    //Logging the request
    format_log_entry(log_entry, &clientaddr, request_uri, response_len);
    fprintf(log_file, "%s %d\n", log_entry, response_len);
    fflush(log_file);

    close(connfd);
    close(serverfd);
    return;
}
