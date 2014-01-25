/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "http-server.h"

#define PORT "14886"  // the port users will be connecting to

#define BACKLOG 20     // how many pending connections queue will hold

#define BUFFER_SIZE 4096

// set up the signal handle to reap all dead processes
#define set_up_signal_handler() \
    sa.sa_handler = sigchld_handler;\
    sigemptyset(&sa.sa_mask); \
    sa.sa_flags = SA_RESTART; \
    if (sigaction(SIGCHLD, &sa, NULL) == -1) { \
        perror("sigaction");        \
        exit(1); \
    }

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(void)
{
    int sockfd;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    HTTPServer server;
    if (server.getStatus() < 0)
    {
        exit(1);
    }
    sockfd = server.getSocketFileDescriptor();
    server.waitForConnection();
    set_up_signal_handler();
    printf("server: waiting for connections...\n");
    while (1) {
        server.acceptConnection();
    }
    
    return 0;
}