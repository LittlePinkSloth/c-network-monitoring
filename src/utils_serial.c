/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

#define _GNU_SOURCE //indispensable sinon il ne trouve pas CRTSCTS qui n'est pas nativement dans POSIX (cf termios manual)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // write(), read(), close()
#include <fcntl.h>  //options de controle de fichiers comme O_RDWR
#include <errno.h>   //strerror()
#include <termios.h>  //POSIX terminal
#include "../include/utils_serial.h"

/*
Fonction qui ouvre et configure le port indiqué en argument
Retourne le descripteur du port.
*/
int serial_init(const char *nom_port) {
    //ouverture du port
    /*options utilisées : 
        O_RDWR : lecture/ecrtiure ; 
        O_NOCTTY : pas de Ctrl+C qui tue le programme ; 
        O_SYNC : ecriture synchrone
    */
    int fd = open(nom_port, O_RDWR | O_NOCTTY | O_SYNC);
    
    if (fd < 0) {
        perror("Erreur ouverture du port serie");
        return -1;
    }

    /*CONFGIURATION DE LA STRUCTURE TERMIOS*/
    struct termios tty;
    
    //vérification de la configuration actuelle du port
    if (tcgetattr(fd, &tty) != 0) {
        perror("Erreur tcgetattr");
        return -1;
    }

    //configuration de la vitesse (9600bauds)
    cfsetospeed(&tty, SERIAL_BAUDRATE); //sortie
    cfsetispeed(&tty, SERIAL_BAUDRATE); //entrée

    //configuration des controles (=c_flag) : on veut la configuration 8N1 standard pour Arduino
    /*  PARENB (N): pas de bit de parité ; 
        CSTOPB (1): 1 bit stop ; 
        CS8  (8)  :données sur 8bits*/
    tty.c_cflag &= ~PARENB; 
    tty.c_cflag &= ~CSTOPB; 
    tty.c_cflag &= ~CSIZE;  
    tty.c_cflag |= CS8;     

    //CRTSCTS : pas de contrôle de flux matériel (RTS/CTS) car Arduino ne s'en sert pas
    tty.c_cflag &= ~CRTSCTS; 

    //CREAD : réception de caracteres ; 
    //CLOCAL : ignore les lignes de contrôle du modem 
    tty.c_cflag |= CREAD | CLOCAL;

    //ICANON : mode ligne à ligne (séparée par \n) : c'est exactement de cette façon dont sont formatées les données envoyées par l'Arduino
    tty.c_lflag |= ICANON;
    
    //désactiviation de l'écho, sinon le Pi renvoie tout à l'Arduino à chaque réception: ce n'est pas grave mais ce n'est pas "propre"
    tty.c_lflag &= ~ECHO; 
    tty.c_lflag &= ~ECHOE; //pas d'effacement
    tty.c_lflag &= ~ISIG;  //pas de signaux (INTR, QUIT, SUSP)

    //configuration de l'entrée
    //IGNBRK : Ignore les BREAK sur la ligne
    //désactive le contrôle de flux logiciel (IXON/IXOFF/IXANY)
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
    //on demande les données brutes
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    //configuration de la sortie
    //OPOST : raw output
    tty.c_oflag &= ~OPOST; 
    tty.c_oflag &= ~ONLCR; // non conversion \n -> \r\n

    //configuration du TO et du nombre mini de caractère :
    tty.c_cc[VTIME] = 5; //TO de 0.5 seconde (5 * 100ms) : si rien n'est arrivé, la fonction se termine pour que le programme principal continue son code sans rester bloquer ici en cas de déconnexion du robot ou de bug quelconque
    tty.c_cc[VMIN] = 0;  //pas de minimum de caractère

    /*APPLICATION DE LA CONFIGURATION*/
    //TCSANOW : application immédiatement
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Erreur tcsetattr lors de l'application de la config.");
        return -1;
    }

    //pause visiblement indispensable car le robot semble rebooter après la configuration du port série
    sleep(2); 

    return fd;
}

int serial_lecture(int fd, char *buffer, int max_len) {
    // vu qu'on a configurer le mode canonnique (ICANON), read() récupère tout jusqu'à "\n "
    int n = read(fd, buffer, max_len - 1);
    
    if (n > 0) {
        buffer[n] = '\0'; //on termine le buffer proprement par le caactère NULL de fin de chaîne
    }
    return n;
}