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
class HttpCache
{
    // DATA
    private:
        std::string d_directory;            // the directory in which to store
                                            // the persistent cache
        
    // PUBLIC METHODS
    public:
        // CREATORS
        explicit HttpCache(const std::string& directory = "cache/");
            // Create a HttpCache object using the specified 'directory as the
            // root for all cached files.
        
        // ACCESSORS
        bool 
        isCached(const std::string& url) const;
            // Searches the cache for the file with the specified url.
            // Returns 'true' if the file is in cache, and 'false' otherwise.
        
        int 
        getFile(const std::string& url, std::string* contents) const;
            // Retrieve the file from cache for the specified 'url', and places
            // the contents into the specified 'contents' object. This method 
            // returns the exit status of the 'write()' system call. Behavior
            // undefined unless 'url' references a vaild file and 'contents'
            // points to a valid string.
            
        // MANIPULATORS
        int 
        cacheFile(const std::string& url, const std::string& contents);
            // Stores the specified 'contents' string at the location referenced
            // by the specified 'url'. Behavior is undefined unless 'url' 
            // references a valid location. Note that if a file already exists
            // at the location referenced by 'url', it will be overwritten.
        
        int 
        removeFile(const std::string& url, std::string* contents); 
            // Deletes the file referenced by the specified 'url', placing the
            // contents in the string referenced by the specified 'contents'.
            // Behavior is undefined unless, url references a vaild file in
            // the cache.
};

}

//==============================================================================
//                             INLINE FUNCTION DEFINITIONS   
//==============================================================================

#endif