// HTTPServer.cpp                                                      -*-C++-*-
// Beej's Guide to Network Programming used as a background template
// http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
#include "http-server.h"
#include "http-request.h"
#include "http-response.h"
#include "http-headers.h"
#include "http-client.h"
#include "http-cache.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>

#include <iostream>

#define BACKLOG 20     // how many pending connections queue will hold

#define BUFFER_SIZE 8096
//==============================================================================
//                    LOCAL FUNCTION DEFINITIONS
//==============================================================================

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(reinterpret_cast<struct sockaddr_in*>(sa)->sin_addr);
    }

    return &(reinterpret_cast<struct sockaddr_in6*>(sa)->sin6_addr);
}

static int sendall(int sockfd, const char *buf, ssize_t *len)
{
    ssize_t total = 0;        // how many bytes we've sent
    size_t bytesleft = *len; // how many we have left to send
    ssize_t n;

    while(total < *len) {
        n = send(sockfd, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 

namespace mrm {
                        // ----------
                        // HTTPServer
                        // ----------

// MANIPULATORS
int HTTPServer::startServer(int port)
{
    int sockfd;  // listen on sock_fd
    struct addrinfo hints, *servinfo;
    int yes=1, rv;
    char PORT[] = "xxxx";
    
    if (sprintf(PORT,"%d", port) < 0) exit(1);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags    = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, 
                      p->ai_socktype,
                      p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, 
                       SOL_SOCKET, 
                       SO_REUSEADDR, 
                       &yes, 
                       sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        if (NULL == p->ai_next)
        {
            fprintf(stderr, "server: failed to bind\n");
            return -2;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure
    
    struct timeval tv;

    tv.tv_sec = 10;
    tv.tv_usec = 500000;

    setsockopt(sockfd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
               
    d_sockfd = sockfd;
    return 0;
}

int HTTPServer::acceptConnection()
{
    printf("server: waiting for connections...\n");
    if (listen(d_sockfd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }
    int status = -1;
    socklen_t sin_size;
    struct sockaddr_storage their_addr; // connector's address information
    int new_fd; // new connection on new_fd
    char s[INET6_ADDRSTRLEN];
    ssize_t request_size;
    char buff[BUFFER_SIZE];
    memset(buff,0, BUFFER_SIZE);
     
    sin_size = sizeof(their_addr);
    // blocks!!!
    size_t fail_ctr = 0;
    do
    {
        new_fd = accept(d_sockfd, 
                        reinterpret_cast<struct sockaddr *>(&their_addr), 
                        &sin_size);
        if (-1 == new_fd) 
        {
            perror("accept");
            fail_ctr++;
        }
        if (fail_ctr >= 5)
        {
            return -1;
        }
    }
    while (fail_ctr < 5 && -1 == new_fd);
    
    struct timeval tv;

    tv.tv_sec  = 10;
    tv.tv_usec = 500000;
    setsockopt(new_fd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
    
    inet_ntop(their_addr.ss_family,
              get_in_addr(reinterpret_cast<struct sockaddr *>(&their_addr)),
              s, 
              sizeof(s));
    printf("server: got connection from %s\n", s);

    if (!fork()) { // this is the child process
        close(d_sockfd); // child doesn't need the listener
        // this is where we actually get the request from the server and 
        // start to try to handle it
        HttpRequest  req;        
        HttpResponse response;
        HttpClient* client = NULL;
        HttpCache cache;
        std::string reqURL;
        char *response_str;
        bool addToCache = true;
        while (1) {
            if ((request_size = recv(new_fd, buff, BUFFER_SIZE, 0)) == -1)
            {
                perror("SERVER: recv");
                close(new_fd);
                if (client)
                {
                    delete client;
                    client = NULL;
                }
                exit(1);
            }
            
            if (request_size > 0)// && errno != EAGAIN)
            {
                try 
                {
                    req.ParseRequest(buff, request_size);
                    reqURL = req.GetRequestURL();
                    // validate the request
                    if (req.GetMethod() == HttpRequest::UNSUPPORTED)
                    {
                        response.SetStatusCode("501");
                        ssize_t response_size   = response.GetTotalLength();
                        response_str = new char [response_size];
                        response.FormatResponse(response_str);
                        if (sendall(new_fd,
                                    response_str,
                                    &response_size) == -1) 
                        {
                             perror("send");
                        }
                        delete [] response_str;
                    }
                    // if in local cache
                    else if (cache.isCached(reqURL))
                    {
                        addToCache = false;
                        std::string cache_response;
                        cache.getFile(req.GetRequestURL(), &cache_response);
                        response.ParseResponse(cache_response.c_str(), 
                                               cache_response.length());
                    }
                    
                    // otherwise get to from origin server
                    else
                    {                        
                        addToCache = true;
                        req.FormatRequest(buff);
                        std::cout << "Full request: " << buff << std::endl;
                        
                        // create an HTTPClient Object
                        if (NULL == client)
                        {
                            client = new HttpClient(req.GetHost(), req.GetPort());
                            if ((status = client->createConnection()) != 0)
                            {
                                if (client)
                                {
                                    delete client;
                                    client = NULL;
                                }
                                exit(status);
                            }    
                        }
                        // pass in HTTPRequest, and have get the page
                        if ((status = client->sendRequest(req)) != 0)
                        {
                            if (client)
                            {
                                delete client;
                                client = NULL;
                            }
                            exit(status);
                        }
                        response = client->getResponse();
                    }
                    // create HTTPResponeObject
                    ssize_t response_size = response.GetTotalLength();
                    response_str = new char [response_size];
                    response.FormatResponse(response_str);
                    if (addToCache)
                    {
                        response_str[response_size] = '\0';
                        std::string r(response_str);
                        cache.cacheFile(reqURL, r);
                    }
                    // return response
                    if (sendall(new_fd,
                                response_str,
                                &response_size) == -1) 
                    {
                         perror("send");
                    }
                    delete [] response_str;
                }
                catch (ParseException e)
                {
                    std::string err = "Error: ";
                    err.append(e.what());
                    err.append("\n");
                    ssize_t msg_size = err.length();
                    if (sendall(new_fd, err.c_str(), &msg_size) == -1) 
                    {
                        perror("send");
                    }
                    printf("server: closed connection from %s\n", s);
                    close(new_fd);
                    if (client)
                    {
                        delete client;
                        client = NULL;
                    }
                    exit(1);
                }
            }
            else 
            {
                strcpy(buff,"\nserver: Connection timed out\n");   
                request_size = strlen(buff);
                if (sendall(new_fd, buff,&request_size) == -1)
                {
                    perror("send");
                }
                if (client)
                {
                    delete client;
                    client = NULL;
                }
                close(new_fd);
                exit(0);
            }
        }
    }
    close(new_fd);
    return 0;
}

} // end of package namespace
