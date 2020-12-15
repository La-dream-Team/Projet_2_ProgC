#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

typedef struct worker
{
    int premier_en_charge;
    int lecture;
    int ecriture_master;
    int* ecriture_worker_suivant;
}WorkerData;

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[], WorkerData *data)
{
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
    data->ecriture_worker_suivant = NULL;
    data->premier_en_charge = atoi(argv[1]);
    data->lecture = atoi(argv[2]);
    data->ecriture_master = atoi(argv[3]);
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(WorkerData data)
{
    do
    {
        // boucle infinie :
        //    attendre l'arrivée d'un nombre à tester
        int nombre_a_tester;
        read(data.lecture, &nombre_a_tester, sizeof(int));
        //    si ordre d'arrêt
        if(nombre_a_tester == -1)
        {
            //si il y a un worker suivant, transmettre l'ordre et attendre sa fin
            //sortir de la boucle
            if(data.ecriture_worker_suivant != NULL)
                write(data.ecriture_worker_suivant[1], &nombre_a_tester, sizeof(int));
            break;
        }
        int res = nombre_a_tester % data.premier_en_charge;
        //    sinon c'est un nombre à tester, 4 possibilités :
        
        if(res != 0 /*&& data.premier_en_charge < (nombre_a_tester/2)*/)
        {   
            //- le nombre est premier
            if(data.ecriture_worker_suivant == NULL)
            {
                //- s'il n'y a pas de worker suivant, le créer
                //Création du pipe pour écrire dans le worker que l'on va créer
                int ecriture_worker[2];
                myassert(pipe(ecriture_worker) != -1, "Erreur lors de la création du tube de communication vers le worker suivant");
                
                //Création de la listre d'arguments necessaires pour le nouveau worker
                char* argv [5];
                argv[0] = "./worker";
                sprintf(argv[1], "%d", nombre_a_tester);
                sprintf(argv[2], "%d", data.ecriture_worker_suivant[0]);
                sprintf(argv[3], "%d", data.ecriture_master);
                argv[4] = NULL;

                //Création du nouveau worker
                execv("./worker", argv);

                //Fermeture de la lecture du tube
                close(data.ecriture_worker_suivant[0]);
            }
            else
            {
                //- s'il y a un worker suivant lui transmettre le nombre
                write(data.ecriture_worker_suivant[1], &nombre_a_tester, sizeof(int));
            }
        }
        else
        {
            int ret = 0;
            if(nombre_a_tester == data.premier_en_charge)
            {
                ret = 1;
            }
            //- le nombre n'est pas premier si ret = 0 
            //- le nombre est premier si ret = 1 
            write(data.ecriture_master, &ret, sizeof(int));
        }  
    }while(true);
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    WorkerData data;
    parseArgs(argc, argv, &data);
    
    open(data.ecriture_master);
    open(data.lecture);

    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    int ret = 1;
    write(data.ecriture_master, &ret, sizeof(int));

    loop(data);

    // libérer les ressources : fermeture des files descriptors par exemple
    close(data.ecriture_master);
    close(data.lecture);

    if(data.ecriture_worker_suivant != NULL)
    {
        close(data.ecriture_worker_suivant[1]);
    }

    return EXIT_SUCCESS;
}
