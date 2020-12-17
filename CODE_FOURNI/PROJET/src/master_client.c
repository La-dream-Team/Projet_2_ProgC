#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_client.h"

// fonctions éventuelles proposées dans le .h

void closePipes(int fd1, int fd2)
{
    close(fd1);
    close(fd2);
}

void writeOnPipe(int fd, int msg)
{
    write(fd, &msg, sizeof(int));
}

void readOnPipe(int fd, int msg)
{
    read(fd, &msg, sizeof(int));
}

void lockSem(int semId)
{
    struct sembuf sb = {0, 1, 0};
    semop(semId, &sb, 1);
}

void unlockSem(int semId)
{
    struct sembuf sb = {0, -1, 0};
    semop(semId, &sb, 1);
}