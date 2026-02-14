#ifndef UTILS_LINUX_H
#define UTILS_LINUX_H

#include <netdb.h> //struct addrinfo 
#include <stdint.h> //  uint_16
#include <stdio.h> //FILE, NULL
#include <unistd.h> //pour close()
#include <string.h> //pour memset

//----CONFIGURATION CLIENT----
//à adapter à la machine : zone0 pour Rpi, zone7 pour ubuntu.
#define TEMP_PATH_LINUX_PC "/sys/class/thermal/thermal_zone7/temp"
#define TEMP_PATH_LINUX_RPI "/sys/class/thermal/thermal_zone0/temp"

/*--Fonction de connexion au serveur*/
int dial(char * machine, char * port);

/*--Fonctions de collecte--*/
float get_cpu_usage() ; //%CPU
float get_temperature(int machine_type); //°C - prend 1 pour PC linux (TYPE_PC_LINUX), 3 pour Rpi (TYPE_RASPBERRY)
float get_ram_usage() ; //%RAM
uint16_t get_nombre_process(); //nb processus actifs


#endif // UTILS_LINUX_H