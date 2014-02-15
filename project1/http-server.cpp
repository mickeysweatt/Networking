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
#include <arpa/inet.h>
#include <ctime>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 20      // how many pending connections queue will hold
#define MAX_CLIENTS 100 // how many outgoing connections we can have
#define BUFFER_SIZE 8096


//------------------------------------------------------------------------------
//                      GLOBAL VARIABLES
//------------------------------------------------------------------------------
extern bool verbose;
extern bool veryVerbose;
extern bool veryVeryVerbose;

//------------------------------------------------------------------------------
//                      LOCAL FUNCTION DECLARATIONS
//------------------------------------------------------------------------------
static void *get_in_addr(struct sockaddr *sa);
    // get sockaddr, IPv4 or IPv6:

static int requestFromServer(HttpRequest&   req, 
                             HttpClient   **client_ptr, 
                             HttpResponse  *response);

static bool isFresh(HttpResponse& response);

static int initializeClientConnection(int sockfd);

static int sendResponse(const HttpResponse& response, 
                        const int           sockfd, 
                        bool                addToCache,
                        HttpCache*          cache_p,
                        std::string*        reqURL_p)
{
    // create HTTPResponeObject
    ssize_t response_size = response.GetTotalLength();
    char *response_str = new char[response_size];
    response.FormatResponse(response_str);
    response_str[response_size] = '\0';
    
    // store to cache
    if (cache_p && reqURL_p && addToCache)
    {
        std::string r(response_str);
        cache_p->cacheFile(*reqURL_p, r);
    }
    if(veryVeryVerbose) 
        printf("\t\tSERVER: Response to client:\n%s\n" , response_str);
    
    // return response
    if (mrm::HttpUtil::sendall(sockfd,
                               response_str,
                              &response_size) == -1) 
    {
         char err[100];
         sprintf(err, "%s%d: send", __FILE__, __LINE__);
         perror(err);
         delete [] response_str;
         return -1;
    }
    return 0;
}

//==============================================================================
//                    LOCAL FUNCTION DEFINITIONS
//==============================================================================

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(reinterpret_cast<struct sockaddr_in*>(sa)->sin_addr);
    }

    return &(reinterpret_cast<struct sockaddr_in6*>(sa)->sin6_addr);
}

static int requestFromServer(HttpRequest&   req, 
                             HttpClient   **client_ptr, 
                             HttpResponse  *response)
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
    struct tm   expire_time;                              // A structure to hold
                                                          // the expiration date
    std::string expireHeader = response.FindHeader("Expires");
    
    const char html_date_format_string[] = 
                                    "%a, %d %b %Y %X %Z"; // a format string for 
                                                          // the HTTP Style 
                                                          // time-date stamp
    
    // if no expire header found
    if ("" == expireHeader)
    {
        // we check for a last modified time exists
        // if one does than we can feasibly make a
        // request from the server for a new copy
        // and thus we return false, otherwise
        // a conditional get makes no sense
        // and we true true
        return ("" != response.FindHeader("Last-Modified-Time"));
    }
        
    memset(&expire_time, 0, sizeof(expire_time));
    
    // parse the expiration time from the response into a time object
    strptime(expireHeader.c_str(),
             html_date_format_string, 
            &expire_time);
    time_t now_t = time(NULL);
    struct tm * now = gmtime(&now_t);
    double diff = difftime(mktime(now), mktime(&expire_time));
    char buf[256];
    strftime (buf, 
              256, 
              html_date_format_string,
              now);
    if (veryVeryVerbose) printf("\t\tNow: %s\n",buf);
    strftime (buf, 
              256, 
              html_date_format_string,
              &expire_time);
    if (veryVeryVerbose) printf("\t\tExpire: %s\n", buf);
    if (veryVeryVerbose) printf("\t\tTime diff: %f\n", diff);
    // return the current_time - expirationation_time > 0
    return (diff < 0);
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
        char err[100];
        sprintf(err, "%s%d: accept", __FILE__, __LINE__);
        perror(err);
        return -1;
    }
    
    inet_ntop(their_addr.ss_family,
              get_in_addr(reinterpret_cast<struct sockaddr *>(&their_addr)),
              s, 
              sizeof(s));
    if (verbose) printf("server: got connection from %s\n", s);
    
    struct timeval tv = {12, 500000};
    // send timeout for sending and receiving from the client
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
            char err[100];
            sprintf(err, "%s:%d - socket", __FILE__, __LINE__);
            perror(err);
            continue;
        }

        if (setsockopt(sockfd, 
                       SOL_SOCKET, 
                       SO_REUSEADDR, 
                       &yes, 
                       sizeof(int)) == -1) 
        {
            char err[100];
            sprintf(err, "%s:%d - setsockopt", __FILE__, __LINE__);
            perror(err);
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            char err[100];
            sprintf(err, "%s:%d - bind", __FILE__, __LINE__);
            perror(err);
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
    
    struct timeval tv = {10, 500000};

    // set the timeouts
    setsockopt(sockfd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
               
    setsockopt(sockfd, 
               SOL_SOCKET, 
               SO_SNDTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
               
    d_sockfd = sockfd >= 0 ? sockfd : -1;
    return d_sockfd >= 0 ? 0 : -1;
}

int HTTPServer::acceptConnection()
{
    ssize_t request_size;        // the number of bytes of the
                                // client's request
                                              
    char buff[BUFFER_SIZE];     // the buffer to hold the request
    
    int new_fd;                             
    
    memset(buff,0, BUFFER_SIZE);    
    
    if (verbose) printf("server: waiting for connections...\n");
    
    if (listen(d_sockfd, BACKLOG) == -1) 
    {
        char err[100];
        sprintf(err, "%s:%d - listen", __FILE__, __LINE__);
        perror(err);
        exit(1);
    }
    
    new_fd = initializeClientConnection(d_sockfd);
    if (-1 == new_fd)
    {
        return -1;
    }
    if (!fork()) 
    { 
        // this is the child process
        close(d_sockfd); // child doesn't need the listener
        // this is where we actually get the request from the server and 
        // start to try to handle it
        size_t maxClientsPerServer = MAX_CLIENTS/BACKLOG;
        HttpRequest  req;        
        HttpResponse response;
        HttpClient* client = NULL;
        HttpCache cache;
        std::string reqURL;
        
        bool   addToCache = true;
        size_t clientCount = 0;
        while (1) 
        {
            if ((request_size = recv(new_fd, buff, BUFFER_SIZE, 0)) == -1)
            {
                char err[100];
                sprintf(err, "%s:%d - recv", __FILE__, __LINE__);
                perror(err);
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
                    // parse users request
                    req.ParseRequest(buff, request_size);
                    reqURL = req.GetRequestURL();
                    
                    // if invalid request, return invalid response
                    if (req.GetMethod() == HttpRequest::UNSUPPORTED)
                    {
                        response.SetStatusCode("501");
                        if (-1 == sendResponse(response, 
                                               new_fd, 
                                               false, 
                                               NULL, 
                                               NULL))
                        {
                            fprintf(stderr, "%s:%d - Could not send\n", __FILE__,
                                                                        __LINE__);
                        }
                    }
                    
                    // if in local cache
                    else if (cache.isCached(reqURL))
                    {
                        if (veryVerbose) printf("IM CACHED\n");
                        
                        // get response from cache
                        std::string cache_response;
                        cache.getFile(req.GetRequestURL(), &cache_response);
                        response.ParseResponse(cache_response.c_str(), 
                                               cache_response.length());

                       // Stale copy in cache 
                       if (!isFresh(response))
                       {
                            HttpResponse conditionalGetResponse; 
                            std::string lastModifiedDate = 
                                           response.FindHeader("Last-Modified");
                            req.AddHeader(std::string("If-Modified-Since"), 
                                          lastModifiedDate);
                            addToCache = true;
                            
                            requestFromServer(req, 
                                             &client,
                                             &conditionalGetResponse);
                            
                            // we get a new page set  the response to this page
                            if ("200" == conditionalGetResponse.GetStatusCode())
                            {
                                if (veryVerbose) printf("Refreshing cache\n");
                                response = conditionalGetResponse;
                                // refresh the cache
                                addToCache = true;
                            }
                            
                            // if not unexpectec resutl
                            else if ("304" != conditionalGetResponse.GetStatusCode())
                            {
                                fprintf(stderr, 
                                        "\t\tUNEXPECTED STATUS CODE %s\n",
                                        conditionalGetResponse.GetStatusCode().c_str());
                                response = conditionalGetResponse;
                            }
                            // otherwise the response is still the cached copy
                        }
                        // Fresh copy in cache
                        else
                        {                       
                            if (veryVerbose) 
                                printf("Fresh! Returning cached copy\n");
                            addToCache = false;
                        }                       
                    }
                    // otherwise get to from origin server
                    else
                    {                       
                        if (veryVerbose) printf("Not cached\n");
                        addToCache = true;
                        if (clientCount < maxClientsPerServer)
                        {
                            clientCount++;
                            requestFromServer(req,&client, &response);
                        }
                    }
                    // return the respose to the client
                    if (-1 == sendResponse(response, 
                                           new_fd, 
                                           addToCache,
                                          &cache,
                                          &reqURL))
                    {
                        fprintf(stderr, "%s:%d - Could not send\n", __FILE__,
                                                                  __LINE__);
                    }
                }
                // if an exception is passed return a 400 error to the user
                catch (ParseException e)
                {
                    response.SetBody("");
                    response.SetStatusCode("400");
                    if (sendResponse(response, 
                                      new_fd, 
                                      addToCache,
                                     &cache,
                                     &reqURL) == -1)
                    {
                        fprintf(stderr, "%s:%d - Could not send\n", __FILE__,
                                                                  __LINE__);
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
                    char err[100];
                    sprintf(err, "%s%d - send", __FILE__, __LINE__);
                    perror(err);
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