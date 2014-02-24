// HttpUtil.h                                                          -*-C++-*-
#ifndef INCLUDED_MRM_HTTPUTIL_H
#define INCLUDED_MRM_HTTPUTIL_H

#ifndef INCLUDED_SYS_TYPE_H
#include <sys/types.h>
#endif

//@Purpose: Encapsulate utility functions used by Http client/server

//@Classes:
//  mrm::HttpUtil: a utility class to encapsulate helper methods for Http agents

// Forward Declarations


//namespace cs118 {

namespace mrm {

struct HttpUtil
{
    public: 
        static int sendall(int sockfd, const char *buf, ssize_t *len);
};

} // end of package namespace

//} // end of course namespace

#endif

