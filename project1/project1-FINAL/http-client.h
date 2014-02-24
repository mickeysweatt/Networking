//HTTP-client.h
//1) takes in a HTTP GET request from the server
//2) generates a connection between client and remote server
//3) gets response from the remote server and stores it


#ifndef HTTP_CLIENT
#define HTTP_CLIENT

#include <netdb.h>
#include <string>

/*
 This class takes in a HTTP request and gets the response
 back from the server and stores it
*/

class HttpRequest;
class HttpResponse;

class HttpClient{

  private:
    //hostname that we want to connect to
    char* m_hostname;
    
    //port number that we want to connect to
    char* m_port;
   
    //holds the response that results from serving the http request
    HttpResponse *m_response;
   
    //socket that gets open between the server and the client
    int m_sockfd;

  public:
    //constructs the client and initializes it
    HttpClient(std::string hostname, unsigned short port = 80);

    
	//destructs the client
    ~HttpClient();
      
	  
    /*
      initializes the client and creates a connection between
      the client and the server needed to serve the Httprequest
    */
    int createConnection();


    /*
      sends the Httprequest over to the desired server and
      stores the response
    */
    int sendRequest(HttpRequest& request);

	
    /*
      returns the response that is stored in the client after
      the client has received a response back from the server
    */
    HttpResponse& getResponse();
	
	//returns hostname
	std::string getHostname();
};
#endif
