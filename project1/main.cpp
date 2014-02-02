/*
** server.c -- a stream socket server demo
*/
#include "http-server.h"
#include "http-request.h"
#include <iostream>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

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

int main(void)
{
    // HttpRequest r;
    // std::string request = "GET google.com/index.html HTTP/1.1\r\n\r\n";
    // r.ParseRequest(request.c_str(), request.length());
    // char buff [100];
    // r.FormatRequest(buff);
    // std::cout << buff << std::endl;
    set_up_signal_handler();
    mrm::HTTPServer server;
    if (server.startServer() < 0) exit(1);
    // main loop
    while (server.acceptConnection() >= 0);


    return 0;
}