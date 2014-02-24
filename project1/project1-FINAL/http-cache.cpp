#include <http-cache.h>
#include <my_stdio.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <algorithm>

static std::string createFileNameForURL(const std::string& directory, 
                                        const std::string& url)
{
    std::string requestURL = url;
    replace(requestURL.begin(), requestURL.end(), '/', '+');
    std::string file_name = directory + requestURL;
    return file_name;
}

namespace mrm {

HttpCache::HttpCache(const std::string& directory)
{
    struct stat s;
    d_directory = directory; 
    int err = stat(d_directory.c_str(), &s);
    if(-1 == err) 
    {
        if(ENOENT == errno) {
            mkdir(d_directory.c_str() , 0766);
        } else {
            Perror("stat", errno);
        }
    }
}

bool HttpCache::isCached(const std::string& url) const
{
    struct stat s;
    int mode = stat(createFileNameForURL(d_directory, url).c_str(), &s);
    if(-1 == mode) 
    {
        return false;
        printf("\nHERE\n");
    }
    return S_ISREG(s.st_mode);
    
}

int HttpCache::getFile(const std::string& url, std::string* contents) const
{
    FILE *stream;
    char *contents_buff;
    size_t fileSize = 0;

    //Open the stream. Note "b" to avoid DOS/UNIX new line conversion.
    stream = fopen(createFileNameForURL(d_directory, url).c_str(), "rb");

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

int HttpCache::cacheFile(const std::string& url, const std::string& contents)
{
    int fd = open(createFileNameForURL(d_directory, url).c_str(),
                                       O_CREAT | O_RDWR,
                                       0666);
    if (fd < 0)
    {
        return -1;
    }
    return ((write(fd, contents.c_str(), contents.length()) < 0) || close(fd));
}

int HttpCache::removeFile(const std::string& url, std::string* contents)
{
    return getFile(url, contents) 
        || remove(createFileNameForURL(d_directory, url).c_str());
}

}

