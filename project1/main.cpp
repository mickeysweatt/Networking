#include "http-server.h"
#include "http-request.h"
#include "http-response.h"
#include "http-client.h"
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdlib.h>
//#include <pthread.h>

static void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// set up the signal handle to reap all dead processes
#define set_up_signal_handler() \
    struct sigaction sa; \
    sa.sa_handler = sigchld_handler;\
    sigemptyset(&sa.sa_mask); \
    sa.sa_flags = SA_RESTART; \
    if (sigaction(SIGCHLD, &sa, NULL) == -1) { \
        perror("sigaction");        \
        exit(1); \
    }
/*
void *print_message_function(void *m)
{
     printf("Threads: %u \n", static_cast<unsigned int>(pthread_self()));
}
*/
 
    
int main(void)
{
    /*
    PTHREADS TEST
    pthread_t threads [10];
    size_t num_threads = sizeof(threads)/ sizeof(*threads);
    for (size_t i = 0; i < num_threads; ++i) {
        pthread_create(&threads[i], NULL, print_message_function, NULL);
    }
    */


    set_up_signal_handler();
    
    mrm::HTTPServer server;
    if (server.startServer() < 0) exit(1);
    // main loop
    while (server.acceptConnection() >= 0);
    
/* 
    HttpRequest req;
    HttpResponse response;
    std::string r = "GET http://cs.ucla.edu/classes/fall13/cs111/news.html HTTP/1.1 \r\n\r\n";
    req.ParseRequest(r.c_str(), r.length());
    r = req.GetHost();
    unsigned short port = req.GetPort();
    HttpClient client(r, port);
    if (client.createConnection() < 0)
    {
        return -1;
    }
    client.sendRequest(req);    
    response = client.getResponse();
    ssize_t response_size = response.GetTotalLength();
    char *response_str = new char [response_size + 1];
    response.FormatResponse(response_str);
    response_str[response_size] = '\0';
    response_str[response_size] = '\0';
    printf("%s\n", response_str);
  */


  
    return 0;
}
