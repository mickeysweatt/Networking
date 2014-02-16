#include <http-server.h>
#include <http-request.h>
#include <http-response.h>
#include <http-client.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
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

int verbosity = 0;
bool verbose         = false;
bool veryVerbose     = false;
bool veryVeryVerbose = false;

int main( int argc, const char* argv[] )
{
   if (argc >= 2)
   {
        verbosity = atoi(argv[1]);
        verbose         = verbosity > 0 ? true : false;
        veryVerbose     = verbosity > 1 ? true : false;
        veryVeryVerbose = verbosity > 2 ? true : false;
   }
   set_up_signal_handler();
    
    mrm::HTTPServer server;
    if (server.startServer() < 0) exit(1);
    // main loop
    while (server.acceptConnection() >= 0);
    


  
    return 0;
}
