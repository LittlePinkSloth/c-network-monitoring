#ifndef UTILS_WINDOWS_H
#define UTILS_WINDOWS_H

#include <windows.h>
#include <tlhelp32.h> //permet de prendre la "photo" du système à l'instant t pour get_nombre_process
#include <stdint.h> //pour uint16_t

float get_ram_usage(); //réccupérer l'utilisation de la RAM sous Windows
unsigned long long conversion_FILETIME_int64(FILETIME ft); //fonction utilitaire : convertir FILETIME en entier
float get_cpu_usage();//récupère les informations sur le CPU
uint16_t get_nombre_process(); //récupère le nombre de processus actifs

#endif //UTILS_WINDOWS_H