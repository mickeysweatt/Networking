// HTTPServer.h                                                        -*-C++-*-
#ifndef HTTPSERVER_H
#define HTTPSERVER_H

//@Purpose: Provide an object oriented wrapper to a simple HTTP Server
//
//@Classes: 
//  mrm::HTTPServer: a simple cached http-proxy server
class HttpCache;

namespace mrm {

                    // ================
                    // class HTTPServer
                    // ================
class HTTPServer {
    // This class implements a simple HTTPProxyServer. It listens on specific
    // 'PORT' for incoming TCP connections. It serves at most 20 simultaneous
    // clients. It maintains a cache of all fresh request pages in the 'cache/'
    // directory.
    
    // DATA
    private:
        int d_sockfd;           // the file descriptor for the listening socket       
    
    public:
        // CREATORS
        explicit HTTPServer();
            // Creates a HTTPServer object with the default options.
            
        ~HTTPServer();
            // Destroy this object

        // ACCESSORS
        int getSocketFileDescriptor() const;
            // Return the socket file descriptor of this object.

        // MANIPULATORS
        int startServer(int port = 1025);
            // Starts the server with the information of the the 'localhost',
            // optionally specify a port to which this server should listen for
            // for incoming connections. If no port is specified, 1025 is used.
            // This method return 0 on success, non-zero on failure.
        
        int acceptConnection();
            // This method has the server waiting for incoming requests from
            // client. When a connection is established, this method forks off a
            // new process to serve the client. Note that this method blocks.
};

}   // close of package namespace

// =============================================================================
//                          INLINE FUNCTION DEFINITIONS
// =============================================================================

namespace mrm {

// CREATORS
inline HTTPServer::HTTPServer() : d_sockfd(-1)
{
    /* intentionally left blank */
}

inline HTTPServer::~HTTPServer()
{   
    /* intentionally left blank */
}

// ACCESSORS
inline 
int HTTPServer::getSocketFileDescriptor() const
{
    return d_sockfd;
}

}   // close of package namespace
#endif // HTTPSEVER_H