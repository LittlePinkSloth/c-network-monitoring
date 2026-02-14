/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */


#include <stdio.h>
#include <stdlib.h>
#include "../include/utils_gpu.h"

//détection de l'OS pour inclure les bons headers
#ifdef _WIN32
    #include <windows.h>
    #define POPEN _popen
    #define PCLOSE _pclose
#else
    #define POPEN popen
    #define PCLOSE pclose
#endif

float get_gpu_usage() {
    FILE *fp;
    char val[1035];
    int usage = -1; // -1 par défaut si échec (pas de GPU ou pas une NVIDIA)

    //commande identitique sur les deux os
    // format=csv,noheader,nounits permet de ne récupérer qu'un nombre
    fp = POPEN("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits", "r");
    
    if (fp == NULL) {
        return usage;
    }

    if (fgets(val, sizeof(val), fp) != NULL) {
        usage = atoi(val);
    }

    PCLOSE(fp);
    return (float)usage;
}