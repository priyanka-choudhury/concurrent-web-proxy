/*
 * A Concurrent Web Proxy (Threaded Version)
 *
 * This multithreaded web proxy handles requests from multiple clients simultaneously.
 * When the proxy receives a connection request from a client, it accepts the connection,
 * creates a thread to handle the request, and then continues to listen for more requests.
 * The thread will read the request, verify that the request is a valid HTTP request, and
 * then parse it to determine the server that the request was directed for. Afterwards, the
 * thread will open a connection to that server, send it the request, receive the reply,
 * and forward the reply to the browser.
 */ 

#include "csapp.h"
#include <pthread.h>

/* Name of the proxy's log file */
#define PROXY_LOG "proxy.log"

/* Struct for defining key properties of a HTTP request */
typedef struct {
    int connfd;                    /* Connection file descriptor */
    struct sockaddr_in clientaddr; /* Client's IP address */
} connection;

/* Global Variables */
FILE *log_file;                   /* Log file of HTTP requests from client */
pthread_mutex_t log_mutex;        /* Mutex to synchronize access to the log file */

/*
 * Function prototypes
 */
void *thread(void *vargp);
void process_request(connection* conn);
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen); 
void Rio_writen_w(int fd, void *usrbuf, size_t n);
int parse_uri(char *uri, char *target_addr, char *path, int *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char** argv) {
    int port;                 /* Port where proxy is listening on */   
    int listenfd;             /* Proxy's listening descriptor */ 

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    /* Create a listening descriptor */
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);

    /* Open log file and initialize mutex */
    log_file = Fopen(PROXY_LOG, "a");
    pthread_mutex_init(&log_mutex, NULL);

    connection* conn = NULL;   /* Argument struct passed to each thread */
    int clientlen;             /* Size of the client's socket address in bytes */
    pthread_t tid;             /* Thread ID */

    /* Wait for processing of requested client connections by listening on server */
    while(1) {
        conn = (connection*)Malloc(sizeof(connection));
        clientlen = sizeof(conn->clientaddr);
        conn->connfd = Accept(listenfd, (SA*)&conn->clientaddr, (socklen_t*)&clientlen);

        /* Create a new thread for each new connection */
        pthread_create(&tid, NULL, thread, conn);
    }
    exit(0);
}

/* thread - Routine for threads */ 
void *thread(void *vargp) {
    connection* conn = (connection*)vargp;
    pthread_detach(pthread_self());
    process_request(conn);
    close(conn->connfd);
    free(vargp);
    return NULL;
}
   
/* process_request - Routine for threads 
 * For each received HTTP request from a client, each thread reads the request
 * and forwards it to the server. Then, it waits for the response and forwards 
 * the response back to the client.
 */
void process_request(connection* conn) {
    int serverfd;            /* Socket descriptor for communication with server */
    char *request;           /* HTTP request from client */
    char *request_uri_start; /* Beginning of URI in HTTP request header */
    char *request_uri_end;   /* End of URI in HTTP request header */
    char *request_uri_body;  /* Rest of URI in HTTP request header */
    int requestlen;          /* Total size of HTTP request in bytes */
    int responselen;         /* Total size of response from server in bytes */
    int realloc_factor;      /* Factor used for increasing size of request buffer */
    int port;                /* Port number extracted from request URI */
    char hostname[MAXLINE];  /* Hostname extracted from request URI */
    char pathname[MAXLINE];  /* Pathname extracted from request URI */
    char log_entry[MAXLINE]; /* Formatted log entry */
    char buffer[MAXLINE];    /* General I/O buffer */
    rio_t rio;               /* Rio buffer for calls to buffered rio_readlineb_w */
    int index, i;            /* Index and counting variables */

    /* Read the HTTP request into the request buffer line by line */
    request = (char *)Malloc(MAXLINE);
    request[0] = '\0';
    realloc_factor = 2;
    requestlen = 0;
    Rio_readinitb(&rio, conn->connfd);
    while (1) {
        if ((index = Rio_readlineb_w(&rio, buffer, MAXLINE)) <= 0) {
            printf("Client generated a bad request (1).\n");
            close(conn->connfd);
            free(request);
            return;
        }

        /* Utilize realloc() to increase the size of the request buffer as needed */
        if ((requestlen + index + 1) > MAXLINE) {
            if ((request = (char *)realloc(request, realloc_factor * MAXLINE)) == NULL) {
                printf("Error in realloc()\n");
                close(conn->connfd);
                free(request);
                return;
            }
            realloc_factor++;
        }
        strcat(request, buffer);
        requestlen += index;

        /* A HTTP request is always terminated by a blank line */
        if (strcmp(buffer, "\r\n") == 0) {
            break;
        }
    }

    /* Make sure client generated a GET request */
    if (strncmp(request, "GET ", strlen("GET "))) {
        printf("Received non-GET request.\n");
        close(conn->connfd);
        free(request);
        return;
    }

    request_uri_start = request + 4;

    /* Extract URI from the HTTP request */
    request_uri_end = NULL;
    for (i = 0; i < requestlen; i++) {
        if (request_uri_start[i] == ' ') {
            request_uri_start[i] = '\0';
            request_uri_end = &request_uri_start[i];
            break;
        }
    }

    printf("Request URI: %s\n", request_uri_start);

    /* Handle case of when end of HTTP request does not have a terminating blank */
    if (i == requestlen) {
        printf("Unable to find end of URI.\n");
        close(conn->connfd);
        free(request);
        return;
    }

    /* Make sure that the HTTP version field follows the URI */
    if (strncmp(request_uri_end + 1, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n")) &&
        strncmp(request_uri_end + 1, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"))) {
        printf("Client generated a bad request (2).\n");
        close(conn->connfd);
        free(request);
        return;
    }

    request_uri_body = request_uri_end + strlen("HTTP/1.0\r\n") + 1;

    /* Parse URI into its hostname, pathname, and port. */
    if (parse_uri(request_uri_start, hostname, pathname, &port) < 0) {
        printf("Unable to parse URI.\n");
        close(conn->connfd);
        free(request);
        return;
    }

    /* Forward HTTP request from client to server */
    if ((serverfd = Open_clientfd(hostname, port)) < 0) {
        printf("Unable to connect to server.\n");
        free(request);
        return;
    }

    Rio_writen_w(serverfd, "GET /", strlen("GET /"));
    Rio_writen_w(serverfd, pathname, strlen(pathname));
    Rio_writen_w(serverfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
    Rio_writen_w(serverfd, request_uri_body, strlen(request_uri_body));

    /* Obtain response from server and forward it to client */
    Rio_readinitb(&rio, serverfd);
    responselen = 0;
    while ((index = Rio_readn_w(serverfd, buffer, MAXLINE)) > 0) {
        responselen += index;
        Rio_writen_w(conn->connfd, buffer, index);
        bzero(buffer, MAXLINE);
    }

    /* Log the HTTP request to the disk file */
    format_log_entry(log_entry, &conn->clientaddr, request_uri_start, responselen); 
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "%s\n", log_entry);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    /* Clean up file descriptors and free HTTP request to avoid memory leaks */
    close(conn->connfd);
    close(serverfd);
    free(request);
    return;
}

/*
 * Rio_readn_w - A wrapper function for rio_readn which
 * prints a warning message when a read fails instead of terminating
 * the process.
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) {
    ssize_t n;
    
    if ((n = rio_readn(fd, ptr, nbytes)) < 0) {
        printf("rio_readn failed!\n");
        return 0;
    }    
    return n;
}

/*
 * Rio_readlineb_w - A wrapper function for rio_readlineb which
 * prints a warning message when a read fails instead of terminating 
 * the process.
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        printf("rio_readlineb failed!\n");
        return 0;
    }
    return rc;
} 

/*
 * Rio_writen_w - A wrapper function for rio_writen which
 * prints a warning message when a write fails instead of terminating
 * the process.
 */
void Rio_writen_w(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n) {
        printf("rio_writen failed!\n");
    }      
}

/* 
 * parse_uri - URI parser
 * In a given URI from a HTTP proxy GET request, the host name,
 * path name, and port are extracted. 
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port) {
    char *hoststart;
    char *hostend;
    char *pathstart;
    int len;

    /* Check for any errors */
    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
   
    /* Extract the host name */
    hoststart = uri + 7;
    hostend = strpbrk(hoststart, " :/\r\n\0");
    if (hostend == 0) {
        fprintf(stderr, "Error: Hostend = 0\n");
        return -1;
    }
    len = hostend - hoststart;
    strncpy(hostname, hoststart, len);
    hostname[len] = '\0';
   
    /* Extract the port number */
    *port = 80; /* default port number */
    if (*hostend == ':') {
        *port = atoi(hostend + 1);
    }
   
    /* Extract the path */
    pathstart = strchr(hoststart, '/');
    if (pathstart == NULL) {
        pathname[0] = '\0';
    } else {
        pathstart++; 
        strcpy(pathname, pathstart);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size) {
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}
