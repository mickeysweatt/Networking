#include "http-server.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>

#define PORT "14886"  // the port users will be connecting to

#define BACKLOG 20     // how many pending connections queue will hold

#define BUFFER_SIZE 4096

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


HTTPServer::HTTPServer() : d_sockfd(-1)
{
}

int HTTPServer::startServer()
{
    int sockfd;  // listen on sock_fd
    struct addrinfo hints, *servinfo;
    int yes=1, rv;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        if (p->ai_next == NULL)
        {
            fprintf(stderr, "server: failed to bind\n");
            return -2;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    d_sockfd = sockfd;
    return 0;
}

int HTTPServer::getSocketFileDescriptor() const
{
    return d_sockfd;
}

void HTTPServer::waitForConnection() const
{
    if (listen(d_sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
}

int HTTPServer::acceptConnection() const
{
    socklen_t sin_size;
    struct sockaddr_storage their_addr; // connector's address information
    int new_fd; // new connection on new_fd
    char s[INET6_ADDRSTRLEN];
    ssize_t response_size;
    char buff[BUFFER_SIZE];
     
    sin_size = sizeof their_addr;
    new_fd = accept(d_sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
        perror("accept");
        return -1;
    }

    inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) { // this is the child process
        close(d_sockfd); // child doesn't need the listener
        // this is where we actually get the request from the server and 
        // start to try to handle it
        if (send(new_fd, "Write me a message: ", 21, 0) == -1)
        {
            perror("send");
        }
        if ((response_size = recv(new_fd, buff, BUFFER_SIZE, 0)) == -1)
        {
            perror("recv");
        }
        printf("From host @%s: %s",s, buff);
        close(new_fd);
        exit(0);
    }
    close(new_fd);
    return 0;
}