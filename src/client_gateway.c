/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

#define _GNU_SOURCE //pour ne plus avoir l'alerte VSCode "pointer or reference to incomplete type "struct addrinfo" is not allowed "

#include <stdlib.h> //exit failure
#include "../include/protocol.h"
#include "../include/utils_linux.h"
#include "../include/utils_serial.h"

//----MAIN----

int main(int argc, char *argv[]) {
    //on définit des valeurs par défaut pour pouvoir être lancé sans argument (par défaut on est en localhost)
    char ip_cible[64] = "127.0.0.1";
    char port_cible[16] = "8080";
    char *type_machine = "1"; //1 pour robot voiture, 2 pour robot bras
    char *port_serie = "/dev/ttyUSB0";

    //on parse les arguments à la recherche de -s (ip cible) ou -t (type)
    if (argc > 1) {
        //on vérifie déjà si le nombre d'argument est logique (3 ou 5)
        if(argc == 3 || argc == 5 || argc == 7) {
            for (int i = 1; i < (argc-1); i++){ //on s'arrête à argc-1 et pas argc car si l'option est le dernier élément, il n'y a rien à récupérer derrirèe donc c'est invalide
                if(strcmp(argv[i], "-s") == 0){
                    //ici, on veut récupérer IP ou IP:PORT selon ce que l'utilisateur a entré
                    char *arg = argv[i+1];
                    char *doublepoint = strchr(arg, ':'); //on vérifie si présence de ":"
                    if(doublepoint){
                        //récup IP
                        size_t ip_len = doublepoint - arg;
                        if(ip_len >= sizeof(ip_cible)) ip_len = sizeof(ip_cible)-1;

                        strncpy(ip_cible, arg, ip_len);
                        ip_cible[ip_len] = '\0';

                        //récup PORT
                        strncpy(port_cible, doublepoint+1, sizeof(port_cible)-1);
                        port_cible[sizeof(port_cible)-1] = '\0';
                    }
                    else {
                        //pas de port, juste récup IP
                        strncpy(ip_cible, arg, sizeof(ip_cible)-1);
                        ip_cible[sizeof(ip_cible)-1] = '\0';
                    }
                    i++;
                }
                else if (strcmp(argv[i], "-t") == 0){
                    type_machine = argv[i+1];
                    i++;
                }
                else if (strcmp(argv[i], "-p") == 0){
                    port_serie = argv[i+1];
                    i++;
                }
            }
        }
        else{
            fprintf(stderr, "Usage : %s [-s <ip>] [-t <type>]  [-p <port serie>] \n", argv[0]);
        }
    }

    //on vérifie que le type de machine est bon (1 ou 2), sinon on remet la valeur à 1 par défaut
    //on pourrait aussi envoyer une erreur, c'est un choix à faire
    if(strcmp(type_machine, "1") != 0 && strcmp(type_machine, "2") != 0) type_machine = "1";
    //conversion en int pour usage interne plus propre
    int type_machine_int = (strcmp(type_machine, "1") == 0) ? TYPE_ROBOT_VOITURE : TYPE_ROBOT_BRAS;

    if(type_machine_int == TYPE_ROBOT_VOITURE) printf("--- CLIENT PASSERELLE ROBOT VOITURE ---\n");
    else printf("--- CLIENT PASSERELLE ROBOT BRAS ---\n");

    //on vérifie le port série qui commence normalement par "/dev/tty" : si ce n'est pas le cas, on remet la caleur par défaut pour éviter tout problème
    char *prefix = "/dev/tty";
    if(strncmp(port_serie, prefix, strlen(prefix)) !=0) port_serie = "/dev/ttyUSB0"; //prt remis par défaut

    printf("Connexion vers %s:%s ...\n", ip_cible, port_cible);

    //connexion au serveur avec dial
    int sock = dial(ip_cible, port_cible);
    
    if (sock < 0) {
        fprintf(stderr, "Erreur : impossible de contacter le serveur.\n");
        return 1;
    }

    printf("Connexion établie : début d'envoi des données...\n");

    //ouverture du port série
    int sfd = serial_init(port_serie);
    if (sfd < 0) {
        perror("serial_init");
        return EXIT_FAILURE;
    }

    char buffer[128]; //allocation sur la pile pour éviter toute fuite de mémoire avec malloc/free

    while(1) {
        //lecture du port série
        int nb_recu = serial_lecture(sfd, buffer, 128);
        if (nb_recu == 0) continue; //on n'a rien reçu, on passe au prochain tour sans envoyer de paquet

        PaquetReseau pqt;
        memset(&pqt, 0, sizeof(pqt));
        
        pqt.magic_nbr = MAGIC_NUMBER;
        pqt.machine_type = type_machine_int; //type du robot pour prévenir de serveur de la machine appelante

        //création du nom machine de la forum hostname:GATEWAY ou hostname selon la taille de hostname (max 64 char)
        char nom_hote[64]; //buffer

        gethostname(nom_hote, sizeof(nom_hote)); //nom du pc
        nom_hote[sizeof(nom_hote)-1] = '\0';  //sécurité pour finir correctement la chaîne si on n'ajoute pas :GATEWAY

        //vérifier la place restante et compléter le cas échéant
        if (strlen(nom_hote) + strlen(":GATEWAY") < sizeof(nom_hote)) {
            strcat(nom_hote, ":GATEWAY");
        }

        strcpy(pqt.machine_nom, nom_hote); //écriture du nom dans le paquet


        //La gateway est un RPi normal, on remplit les infos communes du paquet pour un Rpi
        pqt.cpu_usage = get_cpu_usage();// la fonction calcule les deltas entre 2 appels grace aux variables static
        pqt.temperature = get_temperature(TYPE_RASPBERRY);
        pqt.ram_usage = get_ram_usage();


        //on récupère les données spécifiques du robot en parsant ses arguments
        if(pqt.machine_type == TYPE_ROBOT_VOITURE){
            //ici on utilise l'union voiture
            int dist, bat;
            //message formaté {DIST:XXX;BAT:XX}
            if (sscanf(buffer, "{DIST:%d;BAT:%d}", &dist, &bat) == 2) {
                //on a correctement lu les 2 variables, on les renseigne dans le paquet
                pqt.spec.voiture.obstacle_distance_cm = dist;
                pqt.spec.voiture.niveau_batterie = bat;
            }
            else {
                //valeur par défaut indiquant un soucis de lecture, conforme à la "norme" utilisée dans les autres codes du projet
                pqt.spec.voiture.obstacle_distance_cm = -1;
                pqt.spec.voiture.niveau_batterie = -1;
            }
        }
        else{
            //ici on utilise l'union bras
            int a, b, c, d, e, f; //5 servos + état pince 0 ou 1
            //message formaté {S1:XXX;S2:XXX;S3:XXX;S4:XXX;S5:XXX;P:X}
            if (sscanf(buffer, "{S1:%d;S2:%d;S3:%d;S4:%d;S5:%d;P:%d}", &a, &b, &c, &d, &e, &f) == 6) {
                //on a correctement lu les 6 variables, on les renseigne dans le paquet
                pqt.spec.bras.servo_positions[0] = a;
                pqt.spec.bras.servo_positions[1] = b;
                pqt.spec.bras.servo_positions[2] = c;
                pqt.spec.bras.servo_positions[3] = d;
                pqt.spec.bras.servo_positions[4] = e;
                pqt.spec.bras.etat_pince = f;
            }
            else {
                pqt.spec.bras.servo_positions[0] = -1;
                pqt.spec.bras.servo_positions[1] = -1;
                pqt.spec.bras.servo_positions[2] = -1;
                pqt.spec.bras.servo_positions[3] = -1;
                pqt.spec.bras.servo_positions[4] = -1;
                pqt.spec.bras.etat_pince = -1;
            }

        }

        //Envoidu paquet
        if (send(sock, &pqt, sizeof(pqt), 0) <= 0) {
            printf("Serveur perdu.\n");
            close(sock);
            sock = -1;
            break; //ici on sort du programme, mais on pourrait aussi implémenter une reconnexion
        }

        printf("[Envoyé] CPU: %5.1f%% | RAM: %5.1f%% | Temp: %4.1f°C | ", pqt.cpu_usage, pqt.ram_usage, pqt.temperature);
        //Affichage différentiel selon le type de robot
        if(pqt.machine_type == TYPE_ROBOT_VOITURE) {
            printf("Dist: %4.1lf | Battery: %5.1lf |\n", pqt.spec.voiture.obstacle_distance_cm, pqt.spec.voiture.niveau_batterie);
        }
        else{
            printf("Servos: %2d;%2d;%2d;%2d;%2d | Pince: %3d |\n", pqt.spec.bras.servo_positions[0],pqt.spec.bras.servo_positions[1],pqt.spec.bras.servo_positions[2],pqt.spec.bras.servo_positions[3],pqt.spec.bras.servo_positions[4], pqt.spec.bras.etat_pince);
        }

        //on retire le sleep ici, car la boucle est "cadencée" par le TO de la fonction serial_init()
        //sleep(1); //cadencement de 1 seconde 
    }
    //fin du programme : nettoyage
    if (sock != -1) close(sock);
    close(sfd); //fermeture du port serie
    printf("Arrêt du client Gateway.\n");

    return 0;
}