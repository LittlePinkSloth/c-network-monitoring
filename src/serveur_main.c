/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

#include <pthread.h> //pour les thread séparés du serveur web
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>       //pour poll
#include "../include/protocol.h"
#include "../include/serveur_web.h" //permet de récpérer answer
#include "../include/serveur.h" //constant MAX_CLIENTS, SuiviClient

//définition des variables globales de serveur.h
SuiviClient clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

//variables globales du serveur pour poll
struct pollfd fds[MAX_CLIENTS + 1]; //les 10 socket clients +1 pour le socket d'écoute (serveur)
int max_fds = 0;              //cases utilisées (taille effective du tableau fds)


/*
Cette fonction est juste un wrapper pour pouvoir lancer la fonction answer du serveur_web dans un thread séparé, afin d'alléger le code du main.
*/
void *thread_web_server(void *arg) {
    (void)arg;  //supprime l'avertissement "paramètre inutilisé"

    //affichage pour monitorer les différentes étapes dans le terminal
    printf("--- SERVEUR WEB DEMARRE SUR PORT %s ---\n", PORT_SERVER_WEB);
    answer(PORT_SERVER_WEB, gestion_client); 
    return NULL;
}


int main() {
    //1.mise en place du tableau des clients : toutes les places sont libres
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = -1; //-1 = place libre
        clients[i].nouveau_client = 0; //on n'a pas encore reçu d'info
    }

    //2.mise en place du socket d'ecoute (serveur)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("Socket d'écoute échouée"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_SERVER);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind raté."); exit(1);
    }

    // on accepte une file d'attente de 3 clients avant de les renvoyer
    if (listen(server_fd, 3) < 0) { perror("Listen raté."); exit(1); }

    printf("Serveur démarré sur le port %d...\n", PORT_SERVER);

    //3.configuration initiale de POLL
    //fds[0] = socket d'écoute (pour récupérer les nouveaux clients)
    fds[0].fd = server_fd;
    fds[0].events = POLLIN; //quand on peut lire (POLLIN)
    max_fds = 1;      //pour le moment on a un seul socket à surveiller = fds[0] pour l'écoute

    /* LANCEMENT DU SERVEUR WEB */
    pthread_t web_thread;
    if (pthread_create(&web_thread, NULL, thread_web_server, NULL) != 0) {
        perror("Erreur : Echec de creation thread web...");
    }

    //----BOUCLE DECOUTE PRINCIPALE----
    while (1) {
        //on met à jour le tableau des file descriptosr à chaque tour avec les clients effectifs à chaque tour
        int fds_index = 1; //commence à 1 car 0 est pour écouter
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket_fd != -1) { // -1 = place libre donc on ne l'inclut pas dans le tbl
                fds[fds_index].fd = clients[i].socket_fd;
                fds[fds_index].events = POLLIN; //on écoute le fd indiqué
                fds_index++;
            }
        }
        
        //appel bloquant : on attend jusqu'à l'arrivée d'un paquet sur un des fd
        int appel = poll(fds, fds_index, -1); //-1 = TO infini
        if (appel < 0) { perror("Erreur de poll"); break; }

        //écoute sur le socket d'ecoute : déclenché si arrivée de nouveau client
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in nouvelle_adresse;
            socklen_t addrlen = sizeof(nouvelle_adresse);
            int nouveau_socket = accept(server_fd, (struct sockaddr*)&nouvelle_adresse, &addrlen);

            if (nouveau_socket >= 0) {
                //vérifie s'il a une place libre dans tableau clients[]
                int libre = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].socket_fd == -1) {
                        clients[i].socket_fd = nouveau_socket;
                        clients[i].addr = nouvelle_adresse;
                        clients[i].nouveau_client = 0;
                        printf("--> Nouvelle connexion: %s (place %d)\n", inet_ntoa(nouvelle_adresse.sin_addr), i);
                        libre = 1;
                        break;
                    }
                }
                if (!libre) {
                    printf("Impossible de rejoindre le serveur : plus de place\n");
                    close(nouveau_socket);
                }
            }
        }

        //récupérer les données envoyées par les clients
        for (int i = 1; i < fds_index; i++) { //i=1 pour sauter le socket d'ecoute
            if (fds[i].revents & POLLIN) {
                int socket_courant = fds[i].fd;
                PaquetReseau pqt;
                int valread = read(socket_courant, &pqt, sizeof(pqt));

                //il faut comparer le socket courant avec les sockets des clients qui sont dans le tableau clients[]
                int client_idx = -1;
                for(int k=0; k<MAX_CLIENTS; k++) {
                    if(clients[k].socket_fd == socket_courant) { 
                        client_idx = k; break; 
                    }
                }

                if (valread <= 0) {
                    //déconnexion du client ou erreur de lecture : valread=EOF
                    pthread_mutex_lock(&clients_mutex); // verrou mutex pendnat la modification du client
                    close(socket_courant);
                    if(client_idx != -1) {
                        clients[client_idx].socket_fd = -1; 
                        clients[client_idx].nouveau_client = 0; //reset du flag
                    }
                    pthread_mutex_unlock(&clients_mutex); //on dévérouille
                    
                } else {
                    //des données ont été lues
                    if (valread == sizeof(PaquetReseau) && pqt.magic_nbr == MAGIC_NUMBER) { //vérifie qu'il s'agit bien des données attendues
                        if(client_idx != -1) {
                            //on stocke le paquet entier dans la mémoire du serveur
                            //PaquetReseau est packed,  on copie "brute" vers la structure de stockage (client.dernier_envoi) avec memcpy
                            memcpy(&clients[client_idx].dernier_envoi, &pqt, sizeof(PaquetReseau));
                            clients[client_idx].nouveau_client = 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}