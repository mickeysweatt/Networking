#include "http-client.h"

#include <iostream>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
using namespace std;

HttpClient::HttpClient(std::string h, int p){

  //Formats the hostname that is passed in as a string into a char array
  hostname = new char[h.length() + 1];
  for(int i = 0; i < (h.length()); i++){
    hostname[i] = h[i];
  } 
  hostname[h.length()] = '\0';


  //Formats the port number that is passed in as an integer into a char array  
  string tmp = "";
  
  //reads in port # into stringstream to first convert it into string
  stringstream ss;
  ss << p;
  tmp = ss.str();

  //creates char arr and copies the characters over
  port = new char[tmp.length() + 1];
  for(int i = 0; i < tmp.length(); i++){
    port[i] = tmp[i];
  }
  port[tmp.length()] = '\0';

}

HttpClient::~HttpClient(){
  close(sockfd);

}

int HttpClient::createConnection(){
  int status;

  //used to traverse the linkedlist returned from getaddrinfo function
  struct addrinfo* p;

  
  memset(&hints, 0, sizeof(hints));  // clears the hints structure
  hints.ai_family = AF_UNSPEC;  //connection can be IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // specify TCP connection

  stringstream ss;
  ss << port;

  //checks if there was succesful retrieval of server info
  if(status = getaddrinfo(hostname, port,
                          &hints, &servinfo) != 0) {
    std::cout << "Can't get server info" << endl;
    return -1;
  }
  
  /*
    loops through the servinfo linked list to find a valid entry
    attempts to:
       1) create a socket
       2) connect sockets
  */
   for(p = servinfo; p != NULL; p = p->ai_next){

     //attempts to create socket
     if(sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol) == -1){
       cout << "Can't create socket" << endl;
       continue;
     }
     
     //attempts to connect to the server
     if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
       cout << "Can't connect to server" << endl;
       close(sockfd);
       continue;
     }
     break;
   }

  //None of the entries were valid
  if(p == NULL){
    cout << "Failed to connect" << endl;
    return -1;
  }
  freeaddrinfo(servinfo); //all done with this structure
  return 0;  //returns 0 on successful creation of connection

}

int HttpClient::sendRequest(){
  //number of bytes actually sent
  int numBytes;
  //Receiving buffer
  char recvbuf[512];
  //request
  const char *sendbuf = "GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
  
  //send the request to server
  numBytes = send(sockfd, sendbuf, (int)strlen(sendbuf), 0);
  
  //check if send failed
  if(numBytes == -1){
    cout << "Send failed!!!" << endl;
    close(sockfd);
    return -1;
  }

  //client gets response from server
  do{
    numBytes = recv(sockfd, recvbuf, 512, 0);
    if(numBytes > 0){
      //stores the data received into result string
      string buf(recvbuf, 512);
      response += buf; 
      cout << response << endl;
    }
    else if(numBytes == 0)
      cout << "Connection closed" << endl;
    else
      cout << "Receive failed" << endl;
  } while(numBytes > 0);

  return 0;
}

string HttpClient::getResponse(){
  return response;
}
