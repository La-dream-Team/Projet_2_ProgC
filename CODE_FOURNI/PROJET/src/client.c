#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
//

#include "myassert.h"

#include "master_client.h"

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<number>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL  "\" : calcul de nombre premier en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char * argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;
    
    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP" : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
             usage(argv[0], "le nombre doit être >= 2");
    }       
    
    return order;
}

typedef struct ThreadData
{
    int number, div;
    bool * res;
}ThreadData;

void * codeThread(void * arg)
{
    ThreadData *data = (ThreadData *) arg;
    if((data->number % data->div) == 0)
    {
        *(data)->res = false;
    }
    else
    {
        *(data)->res = true;
    }
    return NULL;
}

void testPrimeWithThreads(int number)
{
    int NB_THREADS = number - 3;

    bool results[NB_THREADS];

    pthread_t tabId[NB_THREADS];

    ThreadData datas[NB_THREADS];

    for(int i = 0; i < NB_THREADS; i++)
    {
        datas[i].res = &(results[i]);
        datas[i].div = i;
        datas[i].number = number;
    }

    for(int i = 0; i < NB_THREADS; i++)
    {
        int ret = pthread_create(&(tabId[i]), NULL, codeThread, &(datas[i]));
        myassert(ret == 0, "Erreur lors de la création des threads");
    }

    for(int i = 0; i < NB_THREADS; i++)
    {
        int ret = pthread_join(tabId[i], NULL);
        myassert(ret == 0, "Erreur lors de l'attente des threads");
    }

    bool prime = true;

    for(int i = 0; i < NB_THREADS; i++)
    {
        if(!(results[i]))
        {
            prime = false;
            break;
        }
    }

    if(prime)
    {
        printf("On attend que le nombre %d soit un nombre premier", number);
    }
    else
    {
        printf("On n'attend pas que le nombre %d soit un nombre premier", number);
    }
    
    
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);
    printf("%d\n", order); // pour éviter le warning

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    // sinon
    //    - entrer en section critique :
    //           . pour empêcher que 2 clients communiquent simultanément
    //           . le mutex est déjà créé par le master
    //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
    //           . les ouvertures sont bloquantes, il faut s'assurer que
    //             le master ouvre les tubes dans le même ordre
    //    - envoyer l'ordre et les données éventuelles au master
    //    - attendre la réponse sur le second tube
    //    - sortir de la section critique
    //    - libérer les ressources (fermeture des tubes, ...)
    //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
    // 
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.

    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    
    if(order == ORDER_COMPUTE_PRIME_LOCAL)
    {
        testPrimeWithThreads(number);
    }
    else
    {
        key_t key1, key2;
        key1 = ftok(MON_FICHIER, CLIENT_PRESENT);
        key2 = ftok(MON_FICHIER, CLIENT_ECRITURE);
        int semid1 = semget(key1, 1, 0);
        int semid2 = semget(key2, 1, 0);

        // on attend que la place soit libre
        struct sembuf sb = { 0 , 0 , 0 };
        semop(semid1, &sb, 1);

        //- entrer en section critique :
        //           . pour empêcher que 2 clients communiquent simultanément
        //           . le mutex est déjà créé par le master

        // la place est libre, on bloque pour etre le seul client sur le master
        lockSem(semid1);

        // On attend le master pour qu'il nous laisse ecrire
        semop(semid2, &sb, 1);

        // on bloque la semaphore client-master
        lockSem(semid2);

        //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
        //           . les ouvertures sont bloquantes, il faut s'assurer que
        //             le master ouvre les tubes dans le même ordre
        int lecture = open(ECRITURE_MASTER, O_RDONLY);
        myassert(lecture != -1, "Erreur lors de l'ouverture du pipe master->client");
        int ecriture = open(ECRITURE_CLIENT, O_WRONLY);
        myassert(ecriture != -1, "Erreur lors de l'ouverture du pipe master->client");

        //- envoyer l'ordre et les données éventuelles au master
        writeOnPipe(ecriture, order);
        
        if(argc == 3){
            number = strtol(argv[2], NULL, 10);
            writeOnPipe(ecriture, number); //On envoi le numero a tester au master
        }

        //- attendre la réponse sur le second tube
        int res;
        read(lecture, &res, sizeof(int));
        switch (order)
        {
            case ORDER_COMPUTE_PRIME : 
                if(res == 0)
                {
                    printf("Le nombre testé, %d, n'est pas un nombre premier.\n", number);
                }
                else
                {
                    printf("Le nombre testé, %d, est un nombre premier.\n", number);
                }
                break;
            
            case ORDER_HIGHEST_PRIME :
                printf("Le nombre premier le plus grand calculé jusqu'au moment est : %d.\n", res);
                break;
            case ORDER_HOW_MANY_PRIME :
                printf("On a calculé %d nombres premiers.\n", res);
                break;
        }


        //- sortir de la section critique
        unlockSem(semid1);

        //- libérer les ressources (fermeture des tubes, ...)
        // fermeture des tubes nommes
        close(lecture);
        close(ecriture);

        //- débloquer le master grâce à un second sémaphore (cf. ci-dessous)
        // le programme est fini je debloque la sem
        unlockSem(semid2);

        /*
        //Creation de la semaphore de presence du client
        int semid1 = semget(key1, 1, IPC_CREAT | IPC_EXCL | 0641);
        myassert(semid1 != -1, "Erreur lors de la création de la semaphore");

        //Creation de la semaphore d'ecriture du client
        int semid2 = semget(key2, 1, IPC_CREAT | IPC_EXCL | 0641);
        myassert(semid2 != -1, "Erreur lors de la création de la semaphore");

        //Blocage semaphore de presence du client
        ret = semctl(semid1, 0, SETVAL, 1);
        myassert(ret != -1, "Erreur lors de la initialisation de la semaphore");
        
        //Blocage semaphore d'ecriture du client
        ret = semctl(semid2, 0, SETVAL, 1);
        myassert(ret != -1, "Erreur lors de la initialisation de la semaphore"); 
        */
    }
    return EXIT_SUCCESS;
}
