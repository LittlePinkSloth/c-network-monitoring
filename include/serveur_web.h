#ifndef SERVEUR_WEB_H
#define SERVEUR_WEB_H

#define PORT_SERVER_WEB "8081"

char *answer(char * port, int (* fun)(int )); //fonction permettant de répondre à un client
int gestion_client(int sock); //fonction callback appelée par answer()


#endif /* SERVEUR_WEB_H */