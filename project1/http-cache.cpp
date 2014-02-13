#include "http-cache.h"
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

const std::string CACHE_DIR = "cache/";

HttpCache::HttpCache()
{
    struct stat s;
    int err = stat("cache/", &s);
    if(-1 == err) 
    {
        if(ENOENT == errno) {
            mkdir("cache/", 0766);
        } else {
            perror("stat");
        }
    }
}

bool HttpCache::isCached(const std::string& url) const
{
    int fd;
    std::string file_name = CACHE_DIR + url;
    fd = open(file_name.c_str(), O_RDONLY);
    FILE *file = fdopen(fd, "r");
    if (NULL == file)
    {
        return false;
    }
    // add check for staleness
    fclose(file);
    return true;
    
}

int HttpCache::getFile(const std::string& url, std::string* contents) const
{
    FILE *stream;
    char *contents_buff;
    size_t fileSize = 0;

    //Open the stream. Note "b" to avoid DOS/UNIX new line conversion.
    std::string file_name = CACHE_DIR + url;
    stream = fopen(file_name.c_str(), "rb");

    //Steak to the end of the file to determine the file size
    fseek(stream, 0L, SEEK_END);
    fileSize = ftell(stream);
    fseek(stream, 0L, SEEK_SET);

    //Allocate enough memory (add 1 for the \0, since fread won't add it)
    contents_buff = static_cast<char *>(malloc(fileSize));

    //Read the file 
    fread(contents_buff, fileSize, 1, stream);
    contents_buff[fileSize] = '\0';
    *contents = std::string(contents_buff);  
    
    free(contents_buff);
    // Close the file
    return fclose(stream);    
}

int HttpCache::cacheFile(const std::string& url, std::string& contents) const
{
    std::string file_name = CACHE_DIR + url;
    int fd = open(file_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0)
    {
        return -1;
    }
    return write(fd, contents.c_str(), contents.length()) < 0 & close(fd);
}
