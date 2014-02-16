#include <http-cache.h>

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <algorithm>

const std::string CACHE_DIR = "cache/";

static std::string createFileNameForURL(const std::string& url)
{
    std::string requestURL = url;
    replace(requestURL.begin(), requestURL.end(), '/', '+');
    std::string file_name = CACHE_DIR + requestURL;
    return file_name;
}

namespace mrm {

HttpCache::HttpCache()
{
    struct stat s;
    int err = stat(CACHE_DIR.c_str(), &s);
    if(-1 == err) 
    {
        if(ENOENT == errno) {
            mkdir(CACHE_DIR.c_str() , 0766);
        } else {
            perror("stat");
        }
    }
}

bool HttpCache::isCached(const std::string& url) const
{
    int fd;
    fd = open(createFileNameForURL(url).c_str(), O_RDONLY);
    
    if (-1 == fd)
    {
        return false;
    }
    
    close(fd);
    return true;
    
}

int HttpCache::getFile(const std::string& url, std::string* contents) const
{
    FILE *stream;
    char *contents_buff;
    size_t fileSize = 0;

    //Open the stream. Note "b" to avoid DOS/UNIX new line conversion.
    stream = fopen(createFileNameForURL(url).c_str(), "rb");

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
    int fd = open(createFileNameForURL(url).c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0)
    {
        return -1;
    }
    return ((write(fd, contents.c_str(), contents.length()) < 0) & close(fd));
}

}