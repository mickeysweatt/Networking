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

class HttpClient{

  private:
    //hostname that we want to connect to
    char* hostname;
    
    //port number that we want to connect to
    char* port;
   
    //holds the response that results from serving the http request
    std::string response;
   
    //socket that gets open between the server and the client
    int sockfd;

    //structs that hold the info about the server
    struct addrinfo hints, *servinfo;


  public:
    HttpClient(std::string hostname, short port = 80);
      //constructs the client and initializes it
    ~HttpClient();
      //destructs stuff don't know yet ***********
    int createConnection();
      /*
        initializes the client and creates a connection between
        the client and the server needed to serve the Httprequest
      */
    int sendRequest();
      /*
        sends the Httprequest over to the desired server and
        stores the response
      */
    std::string getResponse();
      /*
        returns the response that is stored in the client after
        the client has received a response back from the server
      */
};
#endif
