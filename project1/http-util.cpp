// HttpUtil.cpp                                                        -*-C++-*-
#include <http-util.h>
#include <arpa/inet.h>

//namespace cs118 {

namespace mrm {

int HttpUtil::sendall(int sockfd, const char *buf, ssize_t *len)
{
    ssize_t total = 0;        // how many bytes we've sent
    size_t bytesleft = *len; // how many we have left to send
    ssize_t n = 0;

    while(total < *len) {
        n = send(sockfd, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
} 


} // end of package namespace

//} // end of course namespace

