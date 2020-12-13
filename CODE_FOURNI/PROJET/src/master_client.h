#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)
#define MON_FICHIER "master_client.h"
#define CLIENT_PRESENT 10
#define CLIENT_ECRITURE 20

#define ECRITURE_CLIENT "Client-master"
#define ECRITURE_MASTER "Master-client"
// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation


#endif
