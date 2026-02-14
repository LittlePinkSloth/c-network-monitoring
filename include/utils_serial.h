#ifndef UTILS_SERIAL_H
#define UTILS_SERIAL_H

#include <stdint.h>

//configuration standard : 9600 bauds (comme utilisé dans le code Arduino)
#define SERIAL_BAUDRATE B9600
//#define SERIAL_PORT "/dev/ttyUSB0" // nom du port série utilisé sur le RPi

int serial_init(const char *nom_port); //ouvre et configure le port ; renvoie -1 si échoue
int serial_lecture(int fd, char *buffer, int max_len); //lit une ligne jusqu'à \n : renvoie le nb d'octets lus, le contenu est dans le buffer

#endif