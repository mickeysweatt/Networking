#include "http-client.h"
#include "http-request.h"
#include "http-response.h"
#include "http-util.h"
#include <iostream>
#include <string.h>
#include <errno.h>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>

#define BUFFER_SIZE 8096

extern bool veryVerbose;
extern bool veryVeryVerbose;
extern bool verbose;

HttpClient::HttpClient(std::string h, unsigned short p)
{
    //Formats the hostname that is passed in as a string into a char array
    hostname = new char[h.length() + 1];
    strcpy(hostname,h.c_str());
    port = new char[5];
    //Formats the port number that is passed in as an integer into a char array  
    if (sprintf(port, "%d", p) == -1)
    {
        perror("sprintf");
    }
    
    sockfd = -1;
}

HttpClient::~HttpClient() 
{
    delete [] hostname;
    delete [] port;
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
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
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
  
    struct timeval tv = {125, 0};

    setsockopt(sockfd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
    //None of the entries were valid
  if(p == NULL)
  {
    printf("Failed to connect\n");
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
    sendbuf[request.GetTotalLength()] = '\0';
    if (veryVerbose) printf("\t\tFull request:\n\t\t==============\n%s\n\n",sendbuf);
  
    //send the request to server
    ssize_t len_req = static_cast<ssize_t>(strlen(sendbuf));
  
    if ((numBytes = mrm::HttpUtil::sendall(sockfd, sendbuf, &len_req)) == -1)
    {
        perror("send");
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
   
    do 
    {
       
		if ((numBytes = recv(sockfd, recvbuf, BUFFER_SIZE, 0)) == -1)
		{
			//if there is already a response object created and theres an error
			if(response == NULL)
			{
				response = new HttpResponse();
			}
			//set status code to 404 to signal error
			response->SetStatusCode("404");
			delete [] sendbuf;
            perror("recv");
			return -1;
        }
        if (numBytes > 0 && errno != EAGAIN)
        {
            //stores the data received into result string
            std::string buf(recvbuf, numBytes);
            response_str += buf;
            totalBytes += numBytes;

            if(!headerFound && ((endHeaderPos = response_str.find("\r\n\r")) != std::string::npos))
            {
                //create the HTTPResponse object
                //call ParseResponse on the header
                //use the findheader function to obtain the length
                response = new HttpResponse();
                response->ParseResponse(response_str.c_str(), endHeaderPos+4);
                
				if(response->GetStatusCode() != "200")
				{
                        if (veryVerbose) 
                        {
                            printf("\t\tResponse:\n\t\t--------\n%s\n", response_str.c_str());
                       }
                    delete [] sendbuf;
					return 0;
				}
                contentLength = atoi(response->FindHeader("Content-Length").c_str());
                headerFound = true;
            }
        }
        else if(numBytes == 0)
        {
            close(sockfd);
            if (verbose) printf("CLIENT: Connection closed\n");
        }
        else
        {
            if (verbose) printf("CLIENT: Receive failed\n");
        }

        //checks if server is done sending the response
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
    {
	  return -1;
	}  
    delete [] sendbuf;
    if (veryVerbose) printf("\t\tResponse:\n\t\t--------\n%s\n", response_str.c_str());
    return 0;
}

HttpResponse& HttpClient::getResponse()
{
    return *response;
}
