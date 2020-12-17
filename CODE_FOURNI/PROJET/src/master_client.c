#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <fcntl.h>

#include "myassert.h"

#include "master_client.h"

// fonctions éventuelles proposées dans le .h

void closePipes(int fd1, int fd2)
{
    close(fd1);
    close(fd2);
}

void openPipesMaster(int *fd_ecriture, int *fd_lecture)
{
    *fd_ecriture = open(ECRITURE_MASTER, O_WRONLY);
    myassert(*fd_lecture != -1, "Erreur lors de l'overture du tuve master->client");
    *fd_lecture = open(ECRITURE_CLIENT, O_RDONLY);
    myassert(*fd_lecture != -1, "Erreur lors de l'overture du tuve client->master");
}

void openPipesClient(int *fd_ecriture, int *fd_lecture)
{
    *fd_lecture = open(ECRITURE_MASTER, O_RDONLY);
    myassert(*fd_lecture != -1, "Erreur lors de l'overture du tuve master->client");
    *fd_ecriture = open(ECRITURE_CLIENT, O_WRONLY);
    myassert(*fd_lecture != -1, "Erreur lors de l'overture du tuve client->master");
}

void writeOnPipe(int fd, int msg)
{
    write(fd, &msg, sizeof(int));
}

void readOnPipe(int fd, int msg)
{
    read(fd, &msg, sizeof(int));
}

void lockSem(int semId, int id)
{
    struct sembuf sb = {id, -1, 0};
    semop(semId, &sb, 1);
}

void unlockSem(int semId, int id)
{
    struct sembuf sb = {id, 1, 0};
    semop(semId, &sb, 1);
}

void waitSem(int semId, int id)
{
    struct sembuf sb = {id, 0, 0};
    semop(semId, &sb, 1);
}