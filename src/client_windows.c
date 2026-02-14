/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

//includes pour windows
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
//#include <windows.h> inclu dans le header utils_windows.h

//includes normaux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//ncludes perso
#include "../include/protocol.h"
#include "../include/utils_gpu.h" 
#include "../include/utils_windows.h" //contient windows.h

//ligne issue d'exemple de Microsft : utile pour forcer le link avec la lib réseau dans VSCode
//#pragma comment(lib, "ws2_32.lib") //finalement elle est inutile puisque je compile avec MSYS2 et non MSVC... il faut lier la lib au moment de la compilation dans le make

//---FONCTION DIAL (même princiep que Linux mais version Windows) ---
SOCKET dial_windows(char *machine, char *port) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    int iResult;

    // initialisation de Winsock (on est obligé)
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("Echec WSAStartup: %d\n", iResult);
        return INVALID_SOCKET;
    }

    //configuration de getaddrinfo (meme principe que Linux)
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;     // IPv4
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_protocol = IPPROTO_TCP;

    //partie DNS
    iResult = getaddrinfo(machine, port, &hints, &result);
    if (iResult != 0) {
        printf("Echchec getaddrinfo: %d\n", iResult);
        WSACleanup();
        return INVALID_SOCKET;
    }

    //partie connexion
    for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        //création du socket
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            printf("Erreur socket(): %d\n", WSAGetLastError());
            WSACleanup();
            return INVALID_SOCKET;
        }

        //connexion
        iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(sock); //closesocket au lieu de close sous Linux
            sock = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result); //on libere la mémoire

    if (sock == INVALID_SOCKET) {
        printf("Impossible de se connecter au serveur.\n");
        WSACleanup();
        return INVALID_SOCKET;
    }

    return sock;
}

//----MAIN----

int main(int argc, char *argv[]) {
    char ip_cible[64] = "127.0.0.1"; //valeurs par défaut pour pouvoir être lancé sans argument
    char port_cible[16] = "8080";
    
    //on récupère le premier argument si il existe -> ne fonctionne pas sous VSCode/Windows
    //if (argc > 1) ip_cible = argv[1];
    // détection de l'argument valide
    if (argc > 1) {
        int index_ip = 1;
        //au moins 3 arguments ET que argv[1] contient ".exe"
        if (argc > 2 && strstr(argv[1], ".exe") != NULL) {
            index_ip = 2;
        }

        //ici, on veut récupérer IP ou IP:PORT selon ce que l'utilisateur a entré
        char *arg = argv[index_ip];
        char *doublepoint = strchr(arg, ':'); //on vérifie si présence de ":"
        if(doublepoint){
            //récupération ip IP
            size_t ip_len = doublepoint - arg;
            if(ip_len >= sizeof(ip_cible)) ip_len = sizeof(ip_cible)-1;

            strncpy(ip_cible, arg, ip_len);
            ip_cible[ip_len] = '\0';

            //récupération du PORT
            strncpy(port_cible, doublepoint+1, sizeof(port_cible)-1);
            port_cible[sizeof(port_cible)-1] = '\0';
        }
        else {
            //il n'y a pas de double point donc on n'a qu'une IP à récupérer
            strncpy(ip_cible, arg, sizeof(ip_cible)-1);
            ip_cible[sizeof(ip_cible)-1] = '\0';
        }
    }

    printf("----CLIENT WINDOWS COMPLET----\n");
    printf("Connexion vers  %s:%s...\n", ip_cible, port_cible);

    //connexion au serveur avec le nouveau dial
    SOCKET sock = dial_windows(ip_cible, port_cible);
    if (sock == INVALID_SOCKET) {
        return 1;
    }


    printf("Connexion etablie : debut d'envoi des donnees...\n");

    //boucle d'envoi des données
    int nombre_pqt = 0;
    while(1) {
        PaquetReseau pqt;
        memset(&pqt, 0, sizeof(pqt));

        pqt.magic_nbr = MAGIC_NUMBER;
        pqt.paquet_id = nombre_pqt++;
        pqt.machine_type = TYPE_PC_WINDOWS;
        gethostname(pqt.machine_nom, 64);  // récupère le nom du PC
        //on remplit le paquet avec les infos communes
        pqt.cpu_usage = get_cpu_usage();
        pqt.ram_usage = get_ram_usage(); 
        pqt.temperature = -1.0; //WMI étant déprécié, on garde une température à -1 indiquant au serveur qu'elle n'a pas pu être lue

        //récupération des données spécifiques 
        pqt.spec.ordi.nombre_process = get_nombre_process(); 
        pqt.spec.ordi.gpu_usage = get_gpu_usage(); //devrait fonctionner grâce à nvidia-smi

        //Envoi du paquet au serveur
        int iSendResult = send(sock, (char*)&pqt, sizeof(pqt), 0); //on caste en (char*)&pqt car windows veut un char*
        if (iSendResult == SOCKET_ERROR) {
            printf("Echec envoi: %d\n", WSAGetLastError());
            closesocket(sock);
            WSACleanup();
            break;
        }

        printf("[Envoye #%d] GPU: %.1f%% | CPU:  %.1f%% | RAM:  %.1f%% | Process :  %.1d | Temp:  %.1f\n", pqt.paquet_id, pqt.spec.ordi.gpu_usage, pqt.cpu_usage, pqt.ram_usage, pqt.spec.ordi.nombre_process, pqt.temperature);
        Sleep(2000); //sleep avec une majuscule en millisecondes et non en secodnes
    }

    //mise au propore en fin de programme
    closesocket(sock);
    WSACleanup();

    return 0;
}