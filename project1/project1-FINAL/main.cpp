#include <http-server.h>
#include <http-request.h>
#include <http-response.h>
#include <http-client.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdlib.h>
#include <test_util.h>
#include <cstring>

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
static void test_request();
static void test_client(int testCase = -1);

int main( int argc, const char* argv[] )
{
   int test = argc > 1 ? atoi(argv[1]) : 0;
   if (argc >= 3)
   {
        verbosity = atoi(argv[2]);
        verbose         = verbosity > 0 ? true : false;
        veryVerbose     = verbosity > 1 ? true : false;
        veryVeryVerbose = verbosity > 2 ? true : false;
        std::cout << verbosity << std::endl;
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
     case 3: {
        if (verbose) std::cout << "TEST REQUEST" << std::endl;
        test_request();
     } break;
	 case 4 : {
		if (verbose) std::cout << "TEST CLIENT" << std::endl;
		test_client();
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
            const int     LINE            = DATA[ti].d_line;
            const char   *INPUT_FILE      = DATA[ti].d_input_file.c_str();
            const char   *EXP_OUTPUT_FILE = DATA[ti].d_expected_output_file.c_str();
            
            // read in the files
            const std::string INPUT      = get_file_contents(INPUT_FILE);
            const size_t      LEN_INPUT  = INPUT.length();
            const std::string EXP_OUTPUT = get_file_contents(EXP_OUTPUT_FILE);
            char             *output     = new char[LEN_INPUT];
            HttpResponse rep;
            
            if (veryVeryVerbose) { T_ P(LINE) };
            // parse the input file into a response object
            rep.ParseResponse(INPUT.c_str(), LEN_INPUT);
            
            // output the response back into a string
            rep.FormatResponse(output);
            output[LEN_INPUT] = '\0';
            // compare the output from the Response object to the expected value
            ASSERT(std::string(output) == EXP_OUTPUT);
            
            delete [] output;
        }
}

static void test_request()
{
    static const struct {
            int         d_line;
            std::string d_input;
            std::string d_expected_output;
        } DATA[] = {
    // Check that inputs are normalized
        // LINE  // INPUT                                       // EXP
        {L_,     "GET http://google.com/ HTTP/1.1\r\n\r\n"    , "GET / HTTP/1.1\r\n"
                                                                "Host: google.com\r\n\r\n"},
        {L_,     "GET http://www.google.com/ HTTP/1.1\r\n\r\n", "GET / HTTP/1.1\r\n"
                                                                "Host: google.com\r\n\r\n"},
        {L_,     "GET www.google.com/ HTTP/1.1\r\n\r\n"       , "GET / HTTP/1.1\r\n"
                                                                "Host: google.com\r\n\r\n"},
        {L_,     "GET google.com/ HTTP/1.1\r\n\r\n"           , "GET / HTTP/1.1\r\n"
                                                                "Host: google.com\r\n\r\n"},
    // Check a HEAD Request
        // LINE  // INPUT                                       // EXP
        {L_,     "HEAD http://google.com/ HTTP/1.1\r\n\r\n"   , "HEAD / HTTP/1.1\r\n"
                                                                "Host: google.com\r\n\r\n"},
                                                               
    // Check a request with a PATH
         {L_,    "GET http://cs.ucla.edu" 
                 "/classes/fall13/cs111/news.html " 
                 "HTTP/1.1\r\n\r\n"                           ,    
                                                                "GET /classes/fall13/cs111/news.html HTTP/1.1\r\n"
                                                                "Host: cs.ucla.edu\r\n\r\n"},
    // Check a request with more headers
        {L_,     "GET http://cs.ucla.edu" 
                 "/classes/fall13/cs111/news.html " 
                 "HTTP/1.1\r\n"
                 "Accept-Encoding: identity\r\n"
                 "If-Modified-Since: Sat, 15 Feb 2014 17:00:59 GMT\r\n\r\n",
                                                                
                                                                "GET /classes/fall13/cs111/news.html HTTP/1.1\r\n"
                                                                "Host: cs.ucla.edu\r\n"
                                                                "Accept-Encoding: identity\r\n"
                                                                "If-Modified-Since: Sat, 15 Feb 2014 17:00:59 GMT\r\n\r\n"},
    };
        
    const size_t NUM_DATA = sizeof (DATA) / sizeof (*DATA);
    
    for (size_t ti = 0; ti < NUM_DATA; ++ti) {
        const int     LINE       = DATA[ti].d_line;
        const char   *INPUT      = DATA[ti].d_input.c_str();
        const size_t  LEN_INPUT  = DATA[ti].d_input.length();
        const char   *EXP_OUTPUT = DATA[ti].d_expected_output.c_str();
        
        char         *output     = new char[LEN_INPUT];
        
        if (veryVeryVerbose) { T_ P(LINE) };
        HttpRequest req;
        req.ParseRequest(INPUT, LEN_INPUT);
        req.FormatRequest(output);
        ASSERT(0 == strcmp(output, EXP_OUTPUT))
    }
}

static void test_client(int testCase)
{
	bool loop = testCase == -1 ? true : false;
	int i = testCase == -1 ? 4 : testCase;
	do 
	{
	  switch(i)
	  {
		case 4: {	//test case for sending a good request
			if (verbose) std::cout << "CLIENT TEST 1" << std::endl;
			HttpRequest req;
			std::string request = "GET /index.html HTTP/1.1\r\nHost: cs.ucla.edu\r\n\r\n";
			HttpClient h("cs.ucla.edu", 80);
			h.createConnection();
			req.ParseRequest(request.c_str(), request.length());
			int status = h.sendRequest(req);
			if (veryVerbose) std::cout << "Send Status: " << status << std::endl;
			std::string b = h.getResponse().GetBody();
			if (veryVerbose) std::cout << "Body: " << b << std::endl;
		 } break;
		 case 5: {	//test case for trying to create a bad connection
			if (verbose) std::cout << "CLIENT TEST 2" << std::endl;
			HttpClient h("blah", 80);
			int status = h.createConnection();
			if (veryVerbose) std::cout << "Connection Status: " <<  status << std::endl;
			status = atoi(h.getResponse().GetStatusCode().c_str());
			if (veryVerbose) std::cout << "Response Status Code: " << status  << std::endl;
		 } break;
		 case 6: {	//test case for trying to send a request without initializing connection
			if (verbose) std::cout << "CLIENT TEST 3" << std::endl;
			HttpRequest req;
			HttpClient h("cs.ucla.edu", 80);
			std::string request = "GET /index.html HTTP/1.1\r\nHost: cs.ucla.edu\r\n\r\n";
			req.ParseRequest(request.c_str(), request.length());
			int status = h.sendRequest(req);
			if (veryVerbose) std::cout << "Send Status: " << status << std::endl;
			status = atoi(h.getResponse().GetStatusCode().c_str());
			if (veryVerbose) std::cout << "Response Status Code: " << status << std::endl;
		 } break;
		 case 7: {	//test case for testing socket timeout but with good response
			if (verbose) std::cout << "CLIENT TEST 4" << std::endl;
			HttpRequest req;
			std::string request = "GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
			HttpClient h("www.google.com", 80);
			h.createConnection();
			req.ParseRequest(request.c_str(), request.length());
			int status = h.sendRequest(req);
			if (veryVerbose) std::cout << "Send Status: " << status << std::endl;
			status = atoi(h.getResponse().GetStatusCode().c_str());
			if (veryVerbose) std::cout << "Response Status Code: " << status << std::endl;
			std::string b = h.getResponse().GetBody();
			if (veryVerbose) std::cout << "Body: " << b << std::endl;
		 } break;
		 case 8: {	//test case for testing socket timeout with bad response
			if (verbose) std::cout << "CLIENT TEST 5" << std::endl;
			HttpRequest req;
			std::string request = "GET /index.html HTTP/1.1\r\nHost: thepiratebay.se\r\n\r\n";
			HttpClient h("thepiratebay.se", 80);
			h.createConnection();
			req.ParseRequest(request.c_str(), request.length());
			int status = h.sendRequest(req);
			if (veryVerbose)std::cout << "Send Status: " << status << std::endl;
			status = atoi(h.getResponse().GetStatusCode().c_str());
			if (veryVerbose) std::cout << "Response Status Code: " << status << std::endl;
			std::string b = h.getResponse().GetBody();
			if (veryVerbose) std::cout << "Body: " << b << std::endl;
		 } break;
		 default: {
			return;
		 } break;
	   }
	 }
	 while(loop && ++i < 9);
}
/*
static void test_cache()
{
    static const struct {
            int         d_line;
            std::string d_file_name;
            std::string d_file_contents;
            bool        d_create;
            bool        d_destroy;
        } DATA[] = {
    // LINE     // FILE_NAME  // FILE_CONT      // CREAT        // DESTROY
    // Check that 
        {L_,        "test" ,  "test 1"   ,      true   ,       false      },
        {L_,        "test" ,  "test 1"   ,      false   ,      true      },
    };
    const size_t NUM_DATA = sizeof (DATA) / sizeof (*DATA);
        
    for (size_t ti = 0; ti < NUM_DATA; ++ti) {
        const int         LINE          = DATA[ti].d_line;
        const std::string FILE_NAME     = DATA[ti].d_file_name;
        const std::string FILE_CONTENTS = DATA[ti].d_file_contents;
        const bool        CREATE        = DATA[ti].d_create;
        const bool        DESTROY       = DATA[ti].d_destroy;
    }
}
*/