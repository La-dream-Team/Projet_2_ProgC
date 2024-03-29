#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//
#include <pthread.h>
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

/************************************************************************
 * Calcul numero prime avec les threads
 ************************************************************************/

//Structure traité par les threads
typedef struct ThreadData
{
    int number, div;
    bool * res;
}ThreadData;

//Fonction support des threads
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

//Fonction pour tester si un nombre est premier ou pas avec des threads
void testPrimeWithThreads(int number)
{
    int NB_THREADS = number - 3;

    bool results[NB_THREADS];

    pthread_t tabId[NB_THREADS];

    ThreadData datas[NB_THREADS];

    //Initialisation de la structure de données que va être traité par les threads
    for(int i = 0; i < NB_THREADS; i++)
    {
        datas[i].res = &(results[i]);
        datas[i].div = i+2;
        datas[i].number = number;
    }

    //Lancement des threads
    for(int i = 0; i < NB_THREADS; i++)
    {
        int ret = pthread_create(&(tabId[i]), NULL, codeThread, &(datas[i]));
        myassert(ret == 0, "Erreur lors de la création des threads");
    }

    //Attente de la fin des thread
    for(int i = 0; i < NB_THREADS; i++)
    {
        int ret = pthread_join(tabId[i], NULL);
        myassert(ret == 0, "Erreur lors de l'attente des threads");
    }

    //Pour toutes les cases du tableau resultats on teste si il y a une case qui contient false
    bool prime = true;

    for(int i = 0; i < NB_THREADS; i++)
    {
        if(!(results[i]))
        {
            prime = false;
            break;
        }
    }

    //Affichage du resultat
    if(prime)
    {
        printf("Le nombre %d est un nombre premier.\n", number);
    }
    else
    {
        printf("Le nombre %d n'est pas un nombre premier.\n", number);
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
        key_t key;
        key = ftok(MON_FICHIER, PROJ_ID);
        int semId = semget(key, 2, 0);

        // on attend que la place soit libre
        waitSem(semId, 0);

        //- entrer en section critique :
        //           . pour empêcher que 2 clients communiquent simultanément
        //           . le mutex est déjà créé par le master

        // la place est libre, on bloque pour etre le seul client sur le master
        lockSem(semId, 0);

        //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
        //           . les ouvertures sont bloquantes, il faut s'assurer que
        //             le master ouvre les tubes dans le même ordre
        int ecriture, lecture;
        openPipesClient(&ecriture, &lecture);

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
        unlockSem(semId, 0);

        //- libérer les ressources (fermeture des tubes, ...)
        // fermeture des tubes nommes
        closePipes(lecture, ecriture);

        //- débloquer le master grâce à un second sémaphore (cf. ci-dessous)
        // le programme est fini je debloque la sem
        lockSem(semId, 1);
    }
    return EXIT_SUCCESS;
}
