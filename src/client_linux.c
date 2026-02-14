/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

#define _GNU_SOURCE //pour ne plus avoir l'alerte VSCode "pointer or reference to incomplete type "struct addrinfo" is not allowed "

#include "../include/protocol.h"
#include "../include/utils_gpu.h"
#include "../include/utils_linux.h"

//----MAIN----

int main(int argc, char *argv[]) {
    //on définit des valeurs par défaut pour pouvoir être lancé sans argument (par défaut on est en localhost)
    char ip_cible[64] = "127.0.0.1";
    char port_cible[16] = "8080";
    char *type_machine = "1"; // 1 pc, 2 rpi

    //on parse les arguments à la recherche de -s (ip cible) ou -t (type)
    if (argc > 1) {
        //on vérifie déjà si le nombre d'argument est logique (3 ou 5)
        if(argc == 3 || argc == 5) {
            for (int i = 1; i < (argc-1); i++){ //on s'arrête à argc-1 et pas argc car si l'option est le dernier élément, il n'y a rien à récupérer derrirèe donc c'est invalide
                if(strcmp(argv[i], "-s") == 0){
                    //ici, on veut récupérer IP ou IP:PORT selon ce que l'utilisateur a entré
                    char *arg = argv[i+1];
                    char *doublepoint = strchr(arg, ':'); //on vérifie si présence de ":"
                    if(doublepoint){
                        // IP
                        size_t ip_len = doublepoint - arg;
                        if(ip_len >= sizeof(ip_cible)) ip_len = sizeof(ip_cible)-1;

                        strncpy(ip_cible, arg, ip_len);
                        ip_cible[ip_len] = '\0';

                        // PORT
                        strncpy(port_cible, doublepoint+1, sizeof(port_cible)-1);
                        port_cible[sizeof(port_cible)-1] = '\0';
                    }
                    else {
                        strncpy(ip_cible, arg, sizeof(ip_cible)-1);
                        ip_cible[sizeof(ip_cible)-1] = '\0';
                    }

                    i++;
                }

                else if(strcmp(argv[i], "-t") == 0 && i+1 < argc){
                    type_machine = argv[i+1];
                    i++;
                }
            }
        }
        else{
            fprintf(stderr, "Usage : %s [-s ip[:port]] [-t type]\n", argv[0]);
            return 1;
        }
    }

    //on vérifie que le type de machine est bon (1 ou 2), sinon on remet la valeur à 1 par défaut
    //on pourrait aussi envoyer une erreur, c'est un choix à faire
    if(strcmp(type_machine, "1") != 0 && strcmp(type_machine, "2") != 0) type_machine = "1";
    //conversion en int pour usage interne plus propre
    int type_machine_int = (strcmp(type_machine, "1") == 0) ? TYPE_PC_LINUX : TYPE_RASPBERRY;

    if(type_machine_int == TYPE_PC_LINUX) printf("--- CLIENT PC LINUX ---\n");
    else printf("--- CLIENT Rpi ---\n");

    printf("Connexion vers %s:%s ...\n", ip_cible, port_cible);

    //connexion au serveur avec dial
    int sock = dial(ip_cible, port_cible);
    
    if (sock < 0) {
        fprintf(stderr, "Erreur : impossible de contacter le serveur.\n");
        return 1;
    }

    printf("Connexion établie : début d'envoi des données...\n");

    //initialisation des static CPU : premier appel
    get_cpu_usage();
    sleep(1);

    while(1) {
        //Remplissage du pqt réseau
        PaquetReseau pqt;
        memset(&pqt, 0, sizeof(pqt));
        
        pqt.magic_nbr = MAGIC_NUMBER;
        pqt.machine_type = type_machine_int; 

        gethostname(pqt.machine_nom, 64); //on récupère le vrai nom du PC

        pqt.cpu_usage = get_cpu_usage();// la fonction calcule les deltas entre 2 appels grace aux variables static
        pqt.temperature = get_temperature(type_machine_int);
        pqt.ram_usage = get_ram_usage();


        //on récupère les données spécifiques à un client linux :
        pqt.spec.ordi.nombre_process = get_nombre_process();
        //on ne cherche le GPU que pour le type PC
        if(pqt.machine_type == TYPE_PC_LINUX) pqt.spec.ordi.gpu_usage = get_gpu_usage();
        else pqt.spec.ordi.gpu_usage = -1.0;

        //Envoidu paquet
        if (send(sock, &pqt, sizeof(pqt), 0) <= 0) {
            printf("Serveur perdu.\n");
            close(sock);
            sock = -1;
            break;//sortie de la boucle; fin du programme
        }

        printf("[Envoyé] CPU: %5.1f%% | RAM: %5.1f%% | Temp: %4.1f°C | GPU: %4.1f%% | NProc: %4.1d\n", 
               pqt.cpu_usage, pqt.ram_usage, pqt.temperature, pqt.spec.ordi.gpu_usage, pqt.spec.ordi.nombre_process);

        sleep(1); //cadencement de 1 seconde 
    }

    return 0;
}