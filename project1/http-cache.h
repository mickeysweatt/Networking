// HTTPCache.h                                                         -*-C++-*-
#ifndef INCLUDED_HTTP_CACHE_H
#define INCLUDED_HTTP_CACHE_H

#ifndef INCLUDED_STRING_H
#include <string>
#endif
//@Purpose: Provide a class to manage the cache of HTTP response pages
//
//@Classes: 
//  mrm::HTTPCache: a cache that manages HTTP responses on disk

namespace mrm {

                    // ===============
                    // class HTTPCache
                    // ===============
// 

class HttpCache
{
    public:
        HttpCache();
        
        bool 
        isCached(const std::string& url) const;
        
        int 
        getFile(const std::string& url, std::string* contents) const;
        
        int 
        cacheFile(const std::string& url, std::string& contents) const;
        
};

}

//==============================================================================
//                             INLINE FUNCTION DEFINITIONS   
//==============================================================================

#endif