#include <http-server.h>
#include <http-request.h>
#include <http-response.h>
#include <http-client.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdlib.h>
#include <test_util.h>

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

static void test_response();

int main( int argc, const char* argv[] )
{
   int test = argc > 1 ? atoi(argv[1]) : 0;
   if (argc >= 3)
   {
        verbosity = atoi(argv[2]);
        verbose         = verbosity > 0 ? true : false;
        veryVerbose     = verbosity > 1 ? true : false;
        veryVeryVerbose = verbosity > 2 ? true : false;
   }
   set_up_signal_handler();
   
   if (verbose) std::cout << "TEST " << __FILE__ << " CASE " << test << std::endl;
   
   switch (test) { case 0:
     case 1: {
        mrm::HTTPServer server;
        if (server.startServer() < 0) exit(1);
        // main loop
        while (server.acceptConnection() >= 0);
     } break;
     case 2: {
        if (verbose) std::cout << "TEST RESPONSE" << std::endl;
        test_response();
     } break;
     default: {
        std::cerr << "WARNING: CASE `" << test << "' NOT FOUND." << std::endl;
        testStatus = -1;
      }
    }
  
    return testStatus;
}

static void test_response()
{
    static const struct {
            int         d_line;
            std::string d_input_file;
            std::string d_expected_output_file;
        } DATA[] = {
            // LINE             // INPUT            //EXP
            {L_,                "testFiles/200",    "testFiles/200"},
            {L_,                "testFiles/400",    "testFiles/400"},
            {L_,                "testFiles/304",    "testFiles/304"},
        };
        
        const size_t NUM_DATA = sizeof (DATA) / sizeof (*DATA);
        
        for (size_t ti = 0; ti < NUM_DATA; ++ti) {
            const char *INPUT_FILE      = DATA[ti].d_input_file.c_str();
            const char *EXP_OUTPUT_FILE = DATA[ti].d_expected_output_file.c_str();
            
            // read in the files
            const std::string INPUT      = get_file_contents(INPUT_FILE);
            const std::string EXP_OUTPUT = get_file_contents(EXP_OUTPUT_FILE);
            char             *output     = new char[INPUT.length()];
            HttpResponse rep;
            
            // parse the input file into a response object
            rep.ParseResponse(INPUT.c_str(), INPUT.length());
            
            // output the response back into a string
            rep.FormatResponse(output);
            output[INPUT.length()] = '\0';
            // compare the output from the Response object to the expected value
            ASSERT(std::string(output) == EXP_OUTPUT);
            
            delete [] output;
        }
}