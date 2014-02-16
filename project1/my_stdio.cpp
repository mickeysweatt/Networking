#include <my_stdio.h>
#include <cstdio>
#include <string.h>
#include <errno.h>

void Perror(const char *input, int old_errno)
{
    char err[100];
    errno = old_errno;
    sprintf(err, "%s%dd: %s", __FILE__, __LINE__, input);
    perror(err);
}