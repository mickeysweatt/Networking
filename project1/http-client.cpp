#include "http-client.h"
#include <iostream>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "http-request.h"
#include "http-response.h"

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
  close(sockfd);
}

int HttpClient::createConnection()
{
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
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
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
    struct timeval tv = {10, 0};

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
  //number of bytes actually sent
  ssize_t numBytes;
  //Receiving buffer
  char recvbuf[8096];
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
  
  //check if send failed
  if(numBytes == -1){
    std::cout << "Send failed!!!" << std::endl;
    close(sockfd);
    return -1;
  }

  //client gets response from server
  do 
  {
       if ((numBytes = recv(sockfd, recvbuf, 8096, 0)) == -1)
       {
            perror("recv");
        }
        if (numBytes > 0 && errno != EAGAIN)
        {
            //stores the data received into result string
            std::string buf(recvbuf, numBytes);
            response_str += buf; 
            //std::cout << response << std::endl;
        }
        else if(numBytes == 0)
        {
            std::cout << "CLIENT: Connection closed" << std::endl;
        }
        else
        {
            std::cout << "CLIENT: Receive failed" << std::endl;
        }
  } 
  while(numBytes > 0);//.&& numBytes == 8096);
  response = new HttpResponse();
  //std::cout << response_str << std::endl;
  // parses headers and returns pointer to beginning of body
  response->ParseResponse(response_str.c_str(), response_str.length());
  return 0;
}

HttpResponse& HttpClient::getResponse()
{
  return *response;
}
