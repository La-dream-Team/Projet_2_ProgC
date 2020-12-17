#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

char ** argListWorker (int permier_en_charge, int fdLecture , int fdEcriture); 

void libererArgListWorker(char** list);

#endif
