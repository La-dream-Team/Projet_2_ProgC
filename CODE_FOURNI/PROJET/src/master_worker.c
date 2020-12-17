#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_worker.h"

static len 5;

char ** argListWorker (int permier_en_charge, int fdLecture , int fdEcriture){
    char* argv [len] = malloc(5 * sizeof(char *));
    for(int i=0 ; i<len ; i++){
        argv[i] = malloc(10 * sizeof(char));
    }

    argv[0] = "./worker";
    sprintf(argv[1], "%d", nombre_a_tester);
    sprintf(argv[2], "%d", fdEcriture);
    sprintf(argv[3], "%d", fdLecture);
    argv[4] = NULL;

    return argv;
}

void libererArgListWorker(char** list){
    for(int i=0; i<len; i++ ){
        free(list[i]);
    }
    free(list);
    list = NULL;
}

// fonctions éventuelles proposées dans le .h
