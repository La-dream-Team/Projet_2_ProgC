#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)
#define MON_FICHIER "master_client.h"
#define CLIENT_PRESENT 5
#define CLIENT_ECRITURE 10

#define ECRITURE_CLIENT "pipe_cl2ma"
#define ECRITURE_MASTER "pipe_ma2cl"
// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

void closePipes(int fd1, int fd2); //Fonction pour fermer deux tubes


void writeOnPipe(int fd, int msg); //Fonction pour ecrire dans un tube


void readOnPipe(int fd, int msg); //Fonction pour lire depuis un tube


void lockSem(int semId); //Fonction pour bloquer une semaphore


void unlockSem(int semId); //Fonction pour debloquer une semaphore

#endif
