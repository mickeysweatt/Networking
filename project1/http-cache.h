#ifndef HTTP_CACHE_H
#define HTTP_CACHE_H

#include <string>

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

//==============================================================================
//                             INLINE FUNCTION DEFINITIONS   
//==============================================================================



#endif