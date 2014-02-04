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
  hints.ai_family = AF_UNSPEC;  //connection can be IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // specify TCP connection

  //checks if there was succesful retrieval of server info
  if((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) 
  {
    std::cout << "Can't get server info" << std::endl;
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
       std::cout << "Can't connect to server" << std::endl;
       close(sockfd);
       continue;
     }
     break;
   }

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
  char recvbuf[512];
  //request
  char* sendbuf = new char[request.GetTotalLength()];
  request.FormatRequest(sendbuf);
  //const char *sendbuf = "GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
  
  //send the request to server
  if ((numBytes = send(sockfd, sendbuf, strlen(sendbuf), 0)) == -1)
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
  do{
    numBytes = recv(sockfd, recvbuf, 512, 0);
    if(numBytes > 0){
      //stores the data received into result string
      std::string buf(recvbuf, 512);
      response += buf; 
      std::cout << response << std::endl;
    }
    else if(numBytes == 0)
      std::cout << "Connection closed" << std::endl;
    else
      std::cout << "Receive failed" << std::endl;
  } while(numBytes > 0);

  return 0;
}

std::string HttpClient::getResponse()
{
  return response;
}
