#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
//
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
//
#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin
typedef struct
{
	int nbr_courrant;
	int * nb_premiers_calcules;
	int * max_premier;
}MasterData;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop( struct sembuf sb, int semid, int * Master_workers,
                int * Workers_master/* paramètres */, MasterData data)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    int lecture = open(ECRITURE_CLIENT, O_RDONLY);
    int ecriture = open(ECRITURE_MASTER, O_WRONLY);
    // - attente d'un ordre du client (via le tube nommé)
    sb.sem_op = 0;
    semop(semid, &sb, 1);

    int scan;
    fscanf(lecture, "%d", &scan);
    switch (scan){

        // - si ORDER_STOP
        case ORDER_STOP :
            //. envoyer ordre de fin au premier worker et attendre sa fin
            write(Master_workers[1], &scan, sizeof(int));
            //. envoyer un accusé de réception au client
            fprintf(ecriture, "%d", scan);
            break;

        // - si ORDER_COMPUTE_PRIME
        case ORDER_COMPUTE_PRIME :
            //. récupérer le nombre N à tester provenant du client
            fscanf(lecture, "%d", &data.nbr_courrant);
            //. construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
            //             il faut connaître le plus nombre (M) déjà enovoyé aux workers
            //             on leur envoie tous les nombres entre M+1 et N-1
            //             note : chaque envoie déclenche une réponse des workers
            for(int i = data.max_premier + 1; i < data.nbr_courrant; i++){
                //PIPELINE À CONSTRUIRE
            }
            //. envoyer N dans le pipeline
            write(Master_workers[1], &data.nbr_courrant, sizeof(int));
            //. récupérer la réponse
            int res;
            read(Workers_master[0], &res, sizeof(int));
            //. la transmettre au client
            fprintf(ecriture, "%d", res);
            break;

        // - si ORDER_HOW_MANY_PRIME
        case ORDER_HOW_MANY_PRIME :
            //. la transmettre la réponse au client
            fprintf(ecriture, "%d", data.nb_premiers_calcules);
            break;

        // - si ORDER_HIGHEST_PRIME
        case ORDER_HIGHEST_PRIME :
            //. la transmettre la réponse au client
            fprintf(ecriture, "%d", data.max_premier);
            break;

        default:
            //. envoyer un accusé de réception au client
            fprintf(ecriture, "%d", scan);
            break;

        // - fermer les tubes nommés
        close(lecture); // fermeture de la lecture MASTER-CLIENT
        close(ecriture); // fermeture de l'ecriture MASTER-CLIENT

        // - attendre ordre du client avant de continuer (sémaphore : précédence)
        sb.sem_op = 0;
        semop(semid, &sb, 1); 
    };
    
    
    
    
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1)
        usage(argv[0], NULL);

    //On initialise la structure de sauvgarde de donnees du master
    int init_nbrs_premiers_calcules = 1;
    int init_max_premier = 2;
    MasterData data = {0, &init_nbrs_premiers_calcules, &init_max_premier};

    // - création des sémaphores
    key_t key1, key2;
    key1 = ftok(MON_FICHIER, CLIENT_PRESENT);
    key2 = ftok(MON_FICHIER, CLIENT_ECRITURE);

    struct sembuf sb = {0, 1, 0};
    
    //Creation de la semaphore de presence du client
    int semid1 = semget(key1, 1, IPC_CREAT+S_IRUSR+S_IWUSR+S_IRGRP+S_IXOTH+IPC_EXCL);
    assert(semid1 != -1);

    //Blocage semaphore 1
    semop(semid1, &sb, 1);

    //Creation de la semaphore d'ecriture du client
    int semid2 = semget(key2, 1, IPC_CREAT+S_IRUSR+S_IWUSR+S_IRGRP+S_IXOTH+IPC_EXCL);
    assert(semid2 != -1);

    //Blocage semaphore 1
    semop(semid2, &sb, 1); 

    // - création des tubes nommés
    myassert(mkfifo(ECRITURE_CLIENT, 0600) != -1, "Erreur lors de la creation du tube nome !");
    myassert(mkfifo(ECRITURE_MASTER, 0600) != -1, "Erreur lors de la creation du tube nome !");

    // - création du premier worker
    int Master_workers[2];
    int Workers_master[2];
    assert(pipe(Master_workers) != -1);
	assert(pipe(Workers_master) != -1);

    int work[3] = {2, Master_workers[0], Workers_master[1]};
    execv("./worker", &((char *) work) );
    close(Master_workers[0]); // fermeture de la lecture
    close(Workers_master[1]); // fermeture de l'ecriture

    // le  programme est initialise. il peut recevoir un client
    sb.sem_op = -1;
    semop(semid1, &sb, 1);

    // boucle infinie
    loop(sb, semid2,Master_workers , Workers_master/* paramètres */);


    //Destructions :

    //Destruction des semaphores
    semctl(semid1, -1, IPC_RMID);
    semctl(semid2, -1, IPC_RMID);

    //Destruction des tubes nommes
    unlink(ECRITURE_CLIENT);
    unlink(ECRITURE_MASTER);

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
