#ifndef _EXTPIPE_H
#define _EXTPIPE_H

#include <sys/types.h>
#include <stdio.h>

class cExtPipe
{
private:
    pid_t pid;
    FILE *f;
public:
    cExtPipe(void);
    ~cExtPipe();
    FILE *Out()
    {
        return f;
    }
    operator FILE* () const
    {
        return f;
    }
    bool Open(const char *Command, const char *Mode);
    int Close(int &status);
};

#endif

