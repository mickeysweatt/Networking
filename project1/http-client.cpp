#include <http-client.h>
#include <http-request.h>
#include <http-response.h>
#include <http-util.h>
#include <string.h>
#include <errno.h>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>

#define BUFFER_SIZE 8096


//------------------------------------------------------------------------------
//                      GLOBAL VARIABLES
//------------------------------------------------------------------------------

extern bool veryVerbose;
extern bool veryVeryVerbose;
extern bool verbose;

//==============================================================================
//                    LOCAL FUNCTION DEFINITIONS
//==============================================================================

//if there is an error, function will set status code to 404
static void create404(HttpResponse*& response)
{
	if(response == NULL)
	{
		response = new HttpResponse();
	}
	response->SetStatusCode("404");
	return;
}

// =============================================================================
//                          MEMBER FUNCTION DEFINTIONS
// =============================================================================

HttpClient::HttpClient(std::string h, unsigned short p)
{
    //Formats the m_hostname that is passed in as a string into a char array
    m_hostname = new char[h.length() + 1];
    strcpy(m_hostname,h.c_str());
    m_port = new char[5];
    
	
	//Formats the m_port number that is passed in as an integer into a char array  
    if (sprintf(m_port, "%d", p) == -1)
    {
        perror("sprintf");
    }
    
	//Sets socket to -1 to indicate that not initialized
    m_sockfd = -1;
	
	//Sets response to NULL to indicate that its not initialized
	m_response = NULL;
}

HttpClient::~HttpClient() 
{
    delete [] m_hostname;
    delete [] m_port;
}

int HttpClient::createConnection()
{
    //structs that hold the info about the server
	struct addrinfo hints, *servinfo, *p;

	// clears the hints structure
    memset(&hints, 0, sizeof(hints));

	//connection can be IPv4 or IPv6
    hints.ai_family = AF_INET;  
	
	// specify TCP connection
    hints.ai_socktype = SOCK_STREAM; 

    //checks if there was succesful retrieval of server info
    if((getaddrinfo(m_hostname, m_port, &hints, &servinfo)) != 0) 
    {
        perror("getaddressinfo");
		create404(m_response);
        return -1;
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
        if((m_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }
         
         
        //attempts to connect to the server
        if(connect(m_sockfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("connect");
            close(m_sockfd);
            continue;
        }
        break;
    }
  
	//Sets up the recv timeout for socket
    struct timeval tv = {125, 0};
    setsockopt(m_sockfd, 
               SOL_SOCKET, 
               SO_RCVTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
			   
	//Sets up the send timeout for socket
	setsockopt(m_sockfd,
	           SOL_SOCKET,
			   SO_SNDTIMEO, 
               reinterpret_cast<char *>(&tv),
               sizeof(struct timeval));
			   
    //None of the entries were valid
	if(p == NULL)
	{
		printf("Failed to connect\n");
		create404(m_response);
		return -1;
	}
	
	//all done with this structure
	freeaddrinfo(servinfo);
	
	//returns 0 on successful creation of connection
	return 0;  
}

int HttpClient::sendRequest(HttpRequest& request)
{
	//prevents client from sending request if connection is not established
	if(m_sockfd == -1)
	{
		create404(m_response);
		return -1;
	}
	
    //variable to store numBytes actually sent/received
    ssize_t numBytes;
	
    //buffer to store partial response from HTTP server
    char recvbuf[BUFFER_SIZE];
	
	//string to store full response from HTTP server
    std::string response_str = "";
	
    //puts request into a buffer to send to the server
    char* sendbuf = new char[request.GetTotalLength()];
    request.FormatRequest(sendbuf);
    sendbuf[request.GetTotalLength()] = '\0';
	
    if (veryVerbose) printf("\t\tFull request:\n\t\t==============\n%s\n\n",sendbuf);
  
    //attempts to send request to server and error checks
    ssize_t len_req = static_cast<ssize_t>(strlen(sendbuf));
    if ((numBytes = mrm::HttpUtil::sendall(m_sockfd, sendbuf, &len_req)) == -1)
    {
        perror("send");
        close(m_sockfd);
		create404(m_response);
        return -1;
    }  
 
    //the position of the end of the header
    size_t endHeaderPos = -1;
	
    //total number of bytes received
    ssize_t totalBytes = 0;
	
	//length of the response returned by the server
    size_t contentLength = 0; 
	
	//bool to indicate whether or not HTTP header has been received
	bool headerFound = false;
	
    //client gets response from server
    do 
    {
        //error in trying to receive HTTP response from server 
		if ((numBytes = recv(m_sockfd, recvbuf, BUFFER_SIZE, 0)) == -1)
		{
			// try to make a response
			try
			{
				//create the HTTPResponse object
				m_response = new HttpResponse();
					
				//parse the HTTP header
				m_response->ParseResponse(response_str.c_str(), endHeaderPos+4);
				
				// clean-up
				delete [] sendbuf;
				
				// if here the response is valid, return 0
				return 0;
			}
			
			// if not a valid response, create 404
			catch (ParseException e)
			{
				create404(m_response);
				delete [] sendbuf;
				perror("recv");
			}
			return -1;
        }
		
		//client received part of the response back from server
        if (numBytes > 0 && errno != EAGAIN)
        {
            //stores the data received into result string
            std::string buf(recvbuf, numBytes);
            response_str += buf;
            totalBytes += numBytes;

			//if the client has received the whole HTTP header, store the contentLength field
            if(!headerFound && ((endHeaderPos = response_str.find("\r\n\r")) != std::string::npos))
            {
                //create the HTTPResponse object
                m_response = new HttpResponse();
				
				//parse the HTTP header
                m_response->ParseResponse(response_str.c_str(), endHeaderPos+4);
                
				//WHAT DO YOU WANT ME TO DO HERE?
				if(m_response->GetStatusCode() != "200")
				{
                        if (veryVerbose) 
                        {
                            printf("\t\tResponse:\n\t\t--------\n%s\n", response_str.c_str());
                        }

                    delete [] sendbuf;
					return 0;
				}
				
				//use the findheader function to obtain the length
                contentLength = atoi(m_response->FindHeader("Content-Length").c_str());
                headerFound = true;
            }
        }
		//server has closed the connection to the client
        else if(numBytes == 0)
        {
            close(m_sockfd);
            if (verbose) printf("CLIENT: Connection closed\n");
        }

        //checks if server is done sending the response
        if((totalBytes - (endHeaderPos+4)) == contentLength)
        {
            break;
        }
    } 
    while(numBytes > 0);
	
	//checks if response was intialized and sets the body of HTTP response
	if(m_response != NULL)
	{
		//WHAT DO YOU WANT HERE?
		if(response_str == "")
			m_response->SetBody("");
		else
			m_response->SetBody(response_str.substr(endHeaderPos+4));
	}
	//error in obtaining response from server
	else
    {
		create404(m_response);
		return -1;
	}  
	
    delete [] sendbuf;
    if (veryVerbose) printf("\t\tResponse:\n\t\t--------\n%s\n", response_str.c_str());
	
	//on success return 0 to the server
    return 0;
}

HttpResponse& HttpClient::getResponse()
{
    return *m_response;
}

std::string HttpClient::getHostname()
{
	return std::string(m_hostname);
}
