#include "http-client.h"
#include <iostream>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sstream>
#include "http-request.h"
#include "http-response.h"

#define BUFFER_SIZE 8096

HttpClient::HttpClient(std::string h, unsigned short p){
  //Formats the hostname that is passed in as a string into a char array
  hostname = new char[h.length() + 1];
  strcpy(hostname,h.c_str());
  port = new char[5];
  //Formats the port number that is passed in as an integer into a char array  
  if (sprintf(port, "%d", p) == -1)
  {
    perror("sprintf");
  }
  //Initializes sockfd
  sockfd = -1;

  //Initializes response
  response = NULL;
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

HttpClient::~HttpClient() 
{
    delete [] hostname;
    delete [] port;
}

int HttpClient::createConnection()
{
  //CHECK IF THIS IS RIGHT!!!
  //if(sockfd > 0)
  //  return 0;

  int status;

  //used to traverse the linkedlist returned from getaddrinfo function
  struct addrinfo* p;
  
  memset(&hints, 0, sizeof(hints));  // clears the hints structure
  hints.ai_family = AF_INET;  //connection can be IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // specify TCP connection
  
  //checks if there was succesful retrieval of server info
  if((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) 
  {
    perror("getaddressinfo");
    return status;
  }
  
  /*
    loops through the servinfo linked list to find a valid entry
    attempts to:
       1) create a socket
       2) connect sockets
  */
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        //attempts to create socket
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            std::cout << "Can't create socket" << std::endl;
            continue;
        }
         
         
        //attempts to connect to the server
        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("connect");
            close(sockfd);
            continue;
        }
        break;
    }

    
    struct timeval tv = {50, 0};

    setsockopt(sockfd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
  
    //None of the entries were valid
  if(p == NULL){
    std::cout << "Failed to connect" << std::endl;
    return -1;
  }
  freeaddrinfo(servinfo); //all done with this structure
  return 0;  //returns 0 on successful creation of connection

}

int HttpClient::sendRequest(HttpRequest& request)
{
  //std::cout << "SENDREQUEST IS GETTING CALLED!!!!!" << std::endl;
  //number of bytes actually sent
  ssize_t numBytes;
  //Receiving buffer
  char recvbuf[BUFFER_SIZE];
  std::string response_str;
  //request
  char* sendbuf = new char[request.GetTotalLength()];
  request.FormatRequest(sendbuf);
  //const char *sendbuf = "GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
  
  //send the request to server
  ssize_t len_req = static_cast<ssize_t>(strlen(sendbuf));
  if ((numBytes = sendall(sockfd, sendbuf, &len_req)) == -1)
  {
    perror("send");
  }

  //std::cout << "GETS PASSED FIRST CHECKPOINT" << std::endl;
  
  //check if send failed
  if(numBytes == -1){
    std::cout << "Send failed!!!" << std::endl;
    close(sockfd);
    return -1;
  }
 
  //the position of the end of the header
  size_t endHeaderPos = -1;
  //total number of bytes received
  ssize_t totalBytes = 0;
  size_t contentLength = 0; 
  bool headerFound = false;

  //client gets response from server
  // NOTE TO MATT: We could change the HttPesponse to have a method "ParseHeaders (mostly implemented)
  // We would need to add a method to "find A header"
  // And then just use set body to parse the rest of it
  // so in the bdoy when you find the end of the header, call ParseHeaders
  // then search for teh content length header
  // then use that value to do your thing
  // later, when you have the body, put it in with SetBody outside the loop
  do 
  {
       //std::cout << "GETS PASSED SECOND CHECKPOINT!!!!!" << std::endl;
       if ((numBytes = recv(sockfd, recvbuf, BUFFER_SIZE, 0)) == -1)
       {
            perror("recv");
        }
        if (numBytes > 0 && errno != EAGAIN)
        {
            //stores the data received into result string
            std::string buf(recvbuf, numBytes);
            response_str += buf;
            totalBytes += numBytes;
            
            //header is found
            if(!headerFound && ((endHeaderPos = response_str.find("\r\n\r")) != std::string::npos))
            {
                //create the HTTPResponse object
                //call ParseResponse on the header
                //use the findheader function to obtain the length
                response = new HttpResponse();
                response->ParseResponse(response_str.c_str(), endHeaderPos+4);
                contentLength = atoi(response->FindHeader("Content-Length").c_str());
                headerFound = true;
            }
        }
        else if(numBytes == 0)
        {
            close(sockfd);
            std::cout << "CLIENT: Connection closed" << std::endl;
        }
        else
        {
            std::cout << "CLIENT: Receive failed" << std::endl;
        }

        //std::cout << "Response_str: " << response_str << std::endl;
        //std::cout << "numBytes: " << numBytes << std::endl;

        //checks if server is done sending the response
        //std::cout << "This is totalBytes: " << totalBytes << std::endl;
        //std::cout << "This is endHeaderPos: " << endHeaderPos << std::endl;
        //std::cout << "This is contentLength: " << contentLength << std::endl;
        if((totalBytes - (endHeaderPos+4)) == contentLength)
        {
            break;
        }
  } 
  while(numBytes > 0);

  //checks if response was intialized
  if(response != NULL)
  {
      response->SetBody(response_str.substr(endHeaderPos+4));
  }
  else
      return -1;

  delete [] sendbuf;
  return 0;
}

HttpResponse& HttpClient::getResponse()
{
  return *response;
}
