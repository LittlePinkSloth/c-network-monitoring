#ifndef SERVEUR_H
#define SERVEUR_H

#include <pthread.h>
#include <netinet/in.h> //  struct sockaddr_in
#include "protocol.h"   //  PaquetReseau

#define MAX_CLIENTS 10
#define PORT_SERVER 8080 //port du serveur_main

// structure suivi client doit être publique car utilisée dans le serveur_web pour la création du json
typedef struct {
    int socket_fd;           // fd de la socket du client (-1 si libre)
    struct sockaddr_in addr; // adresse ip du client
    PaquetReseau dernier_envoi; // stockage du dernier paquet reçu
    int nouveau_client;      // flag (0 ou 1)
} SuiviClient;

//variables globales en "extern" pour que les autres fichiers (donc serveur_web) puissent y accéder
extern SuiviClient clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

#endif