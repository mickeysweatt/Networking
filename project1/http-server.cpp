// HTTPServer.cpp                                                      -*-C++-*-
// Beej's Guide to Network Programming used as a background template
// http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
#include "http-server.h"
#include "http-request.h"
#include "http-response.h"
#include "http-headers.h"
#include "http-client.h"
#include "http-cache.h"
#include "http-util.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <ctime>
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

static int requestFromServer(HttpRequest&   req, 
                             HttpClient   **client_ptr, 
                             HttpResponse  *response
                            )
{
    HttpClient *client = *client_ptr;
    int status = -1;
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
            return status;
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
        return status;
    }
    *response = client->getResponse();
    *client_ptr = client;
    return status;
}

static bool isFresh(HttpResponse& response)
{
    std::string expireHeader            = response.FindHeader("Expires");
    const char html_date_format_string[] = "%a, %d %h %G %X %Z";
    // never expires
    if ("" == expireHeader)
    {
        return true;
    }
    
    struct tm expire_time;

    memset(&expire_time, 0, sizeof(expire_time));
           
    strptime(expireHeader.c_str(),
             html_date_format_string, 
            &expire_time);
    
    return (difftime(time(0), mktime(&expire_time)) > 0);
}

static int initializeClientConnection(int sockfd)
{
    socklen_t sin_size;                       // the size of the socket 
                                              // IN Address
                                              
    struct sockaddr_storage their_addr;       // connector's address information
    
    int new_fd;                               // new connection on new_fd
    
    char s[INET6_ADDRSTRLEN];                 // an array to hold the address of
                                              // of the requesting client
                                                        
    sin_size = sizeof(their_addr);
    
    size_t fail_ctr = 0;                     // a counter to break-out of accept
                                             // loop if fails
    
    // This loop is in place to resolve issue of attempting to open socket
    // before the OS has finished closing.
    do
    {
        new_fd = accept(sockfd, 
                        reinterpret_cast<struct sockaddr *>(&their_addr), 
                        &sin_size);
    }
    while (fail_ctr++ < 3 && -1 == new_fd);
    
    // if after the loop completion we still have error state
    if (-1 == new_fd) 
    {
        perror("accept");
        return -1;
    }
    
    inet_ntop(their_addr.ss_family,
              get_in_addr(reinterpret_cast<struct sockaddr *>(&their_addr)),
              s, 
              sizeof(s));
    printf("server: got connection from %s\n", s);
    
    struct timeval tv = {12, 500000};
    // send timeout for sending a receiving from the client
    setsockopt(new_fd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
    setsockopt(new_fd, 
               SOL_SOCKET, 
               SO_SNDTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
    return new_fd;
}                     
                            // ----------
                            // HTTPServer
                            // ----------
// =============================================================================
//                          MEMBER FUNCTION DEFINTIONS
// =============================================================================

namespace mrm {

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
    ssize_t request_size;        // the number of bytes of the
                                // client's request
                                              
    char buff[BUFFER_SIZE];     // the buffer to hold the request
    
    int new_fd;                             
    
    memset(buff,0, BUFFER_SIZE);    
    
    printf("server: waiting for connections...\n");
    
    if (listen(d_sockfd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }
    
    new_fd = initializeClientConnection(d_sockfd);
    if (-1 == new_fd)
    {
        return -1;
    }
    if (!fork()) 
    { // this is the child process
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
        while (1) 
        {
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
            // normal cases
            if (request_size > 0 && errno != EAGAIN)
            {
                try 
                {
                    req.ParseRequest(buff, request_size);
                    reqURL = req.GetRequestURL();
                    // if invalid request, return invalid response
                    if (req.GetMethod() == HttpRequest::UNSUPPORTED)
                    {
                        response.SetStatusCode("501");
                        ssize_t response_size   = response.GetTotalLength();
                        response_str = new char [response_size];
                        response.FormatResponse(response_str);  
                        if (HttpUtil::sendall(new_fd,
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
                        printf("IM CACHED\n");
                        addToCache = false;
                        std::string cache_response;
                        cache.getFile(req.GetRequestURL(), &cache_response);
                        response.ParseResponse(cache_response.c_str(), 
                                               cache_response.length());
                         

                       // Stale copy in cache 
                       if (!isFresh(response))
                       {
                            std::string lastModifiedDate = 
                                           response.FindHeader("Last-Modified");
                            req.AddHeader(std::string("If-Modified-Since"), 
                                          lastModifiedDate);
                            requestFromServer(req, 
                                              &client, 
                                              &response);
                            addToCache = true;
                            requestFromServer(req, &client, &response);
                                           // otherwise get to from origin server
                        }
                        // Fresh copy in cache
                        else
                        {                       
                            std::cout << "Fresh! Returning cached copy" << std::endl;
                            addToCache = false;
                        }                       
                    }
                    // otherwise get to from origin server
                    else
                    {                       
                        std::cout << "Not cached" << std::endl;
                        addToCache = true;
                        requestFromServer(req,&client, &response);
                    }
                    // create HTTPResponeObject
                    ssize_t response_size = response.GetTotalLength();
                    response_str = new char[response_size];
                    response.FormatResponse(response_str);
                    response_str[response_size] = '\0';
                    
                    // store to cache
                    if (addToCache)
                    {
                        std::string r(response_str);
                        cache.cacheFile(reqURL, r);
                    }
                    // return response
                    if (HttpUtil::sendall(new_fd,
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
                    if (HttpUtil::sendall(new_fd, err.c_str(), &msg_size) == -1) 
                    {
                        perror("send");
                    }
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
                if (HttpUtil::sendall(new_fd, buff,&request_size) == -1)
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