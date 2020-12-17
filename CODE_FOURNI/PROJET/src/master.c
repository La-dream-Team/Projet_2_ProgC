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
#include <string.h>
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
void loop( int semId, int * Master_workers,
                int * Workers_master, MasterData data)
{
    do
    {
        printf("Je suis rentree dans la boucle\n");
        // boucle infinie :
        // - ouverture des tubes (cf. rq client.c)
        int ecriture, lecture;
        openPipesMaster(&ecriture, &lecture);

        printf("Je lis dans le tube du client\n");

        // - attente d'un ordre du client (via le tube nommé)
        int scan;
        read(lecture, &scan, sizeof(int));
        switch (scan){

            // - si ORDER_STOP
            case ORDER_STOP :
                //. envoyer ordre de fin au premier worker et attendre sa fin
                write(Master_workers[1], &scan, sizeof(int));
                //. envoyer un accusé de réception au client
                write(ecriture, &scan, sizeof(int));
                break;

            // - si ORDER_COMPUTE_PRIME
            case ORDER_COMPUTE_PRIME :
                //. récupérer le nombre N à tester provenant du client
                read(lecture, &data.nbr_courrant, sizeof(int));
                //. construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
                //             il faut connaître le plus nombre (M) déjà enovoyé aux workers
                //             on leur envoie tous les nombres entre M+1 et N-1
                //             note : chaque envoie déclenche une réponse des workers
                if(data.nbr_courrant > *data.max_premier)
                {
                    for(int i = *data.max_premier + 1; i < data.nbr_courrant; i++){
                        //. envoyer N dans le pipeline
                        write(Master_workers[1], &i, sizeof(int));
                        //. récupérer la réponse
                        int res;
                        read(Workers_master[0], &res, sizeof(int));

                        if(res == i){
                            *data.max_premier = i;
                            data.nb_premiers_calcules ++;
                        }
                    }
                    
                    /*// si le nombre a rechercher n'est pas premier on renvoie 0
                    if(data.max_premier != data.nbr_courrant){
                        data.nbr_courrant = 0;
                    }*/
                }
                else
                {
                    write(Master_workers[1], &data.nbr_courrant, sizeof(int));
                
                    read(Workers_master[0], &data.nbr_courrant, sizeof(int));
                }
                /*
                //. la transmettre au client 
                if(data.nbr_courrant != 0)
                {
                    data.nbr_courrant = 1;
                }*/
                write(ecriture, &data.nbr_courrant, sizeof(int));
                break;

            // - si ORDER_HOW_MANY_PRIME
            case ORDER_HOW_MANY_PRIME :
                //. la transmettre la réponse au client
                write(ecriture, &data.nb_premiers_calcules, sizeof(int));
                break;

            // - si ORDER_HIGHEST_PRIME
            case ORDER_HIGHEST_PRIME :
                //. la transmettre la réponse au client
                write(ecriture, &data.max_premier, sizeof(int));
                break;

            default:
                //. envoyer un accusé de réception au client
                write(ecriture, &scan, sizeof(int));
                break;

            // - fermer les tubes nommés
            close(lecture); // fermeture de la lecture MASTER-CLIENT
            close(ecriture); // fermeture de l'ecriture MASTER-CLIENT

            // - attendre ordre du client avant de continuer (sémaphore : précédence)
            unlockSem(semId, 1);
        }
    // - revenir en début de boucle
    }while(true);
    
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
    key_t key;
    key = ftok(MON_FICHIER, PROJ_ID);

    //Creation de la semaphore de presence du client
    int semId = semget(key, 2, IPC_CREAT | IPC_EXCL | 0641);
    myassert(semId != -1, "Erreur lors de la création des semaphores");

    printf("J'ai cree les semaphores \n");

    //Blocage semaphore de presence du client
    int ret = semctl(semId, 0, SETVAL, 1);
    myassert(ret != -1, "Erreur lors de la initialisation de la semaphore 1");
    
    //Blocage semaphore d'ecriture du client
    ret = semctl(semId, 1, SETVAL, 0);
    myassert(ret != -1, "Erreur lors de la initialisation de la semaphore 2"); 

    printf("J'ai initialise les semaphores \n");

    // - création des tubes nommés
    ret = mkfifo(ECRITURE_MASTER, 0600);
    myassert(ret != -1, "Erreur lors de la creation du tube nomme !");
    ret = mkfifo(ECRITURE_CLIENT, 0600);
    myassert(ret != -1, "Erreur lors de la creation du tube nomme !");

    printf("J'ai cree les tubes nommes \n");

    //- creation tubes annonymes
    int Master_workers[2];
    int Workers_master[2];
    myassert(pipe(Master_workers) != -1, "Erreur lors de la creation d'une pipe");
	myassert(pipe(Workers_master) != -1, "Erreur lors de la creation d'une pipe");
    printf("J'ai cree les tubes annonymes\n");


    // - création du premier worker
    if(fork() == 0){ 
        //code fils
        printf("Creation du worker du nombre 2\n");
        // creation de la liste d'argument  
        char** ret_worker = argListWorker(2 , Master_workers[0] , Workers_master[1] );
        
        close(Master_workers[1]); // fermeture de l'ecriture
        close(Workers_master[0]); // fermeture de la lecture
        

        // creation du nouveau worker
        printf("Le worker du nombre 2 est crée\n");
        execv("./worker", ret_worker);

        libererArgListWorker(ret_worker);
       
    }
    else{
        sleep(1);
        //code pere
        printf("Code workers\n");
        myassert(close(Master_workers[0])!= -1 , "Erreur lors de la fermeture"); // fermeture de la lecture
        myassert(close(Workers_master[1])!= -1 , "Erreur lors de la fermeture"); // fermeture de l'ecriture

        // le  programme est initialise. il peut recevoir un client
        unlockSem(semId, 0);

        printf("Le master est pret a recevoir un client\n");

        // boucle infinie
        loop(semId/*DEUXIEME SEMAPHORE == 1*/, Master_workers , Workers_master, data);

        //Destructions :

        //Destruction des semaphores
        semctl(semId, -1, IPC_RMID);

        //Destruction des tubes nommes
        unlink(ECRITURE_CLIENT);
        unlink(ECRITURE_MASTER);

        return EXIT_SUCCESS;
    }
}



// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
