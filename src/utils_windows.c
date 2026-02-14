/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

#include "../include/utils_windows.h" //contient l'include windows.h



/*
Fonction qui renvoie le %RAM actuellement utilisée par un PC Windows.
L'information est directement disponible dans MEMORYSTATUSEX.dwMemoryLoad
*/
float get_ram_usage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX); //permet à Windows d'accepter la struct

    //appel systeme
    GlobalMemoryStatusEx(&memInfo);

    // contrairement à Linux, Windows donne directement le % RAM donc on peut le renvoyer tel quel en castant le float
    return (float)memInfo.dwMemoryLoad;
}


/*
Fonction qui renvoie le %CPU utilisé.
En dehors des types propres à Windows, elle fonctionne de la même manière que celle sous Linux (delta entre deux lectures).
Nécessite une fonction utilitaire pour pouvoir extraire les informations du type "FILETIME" (int de 64 bits coupé en deux x 32 bits (dwLowDateTime, dwHighDateTime))
*/
float get_cpu_usage() {
    //déclaration en static pour que la valeur de la variable reste la meme d'un appel à l'autre
    static unsigned long long t0_repos = 0;
    static unsigned long long t0_kernel = 0; //kernel contient intrinsèquement le repos dans Windows contrairement à Linux
    static unsigned long long t0_user = 0;

    FILETIME repos, kernel, user;
    
    //appel système : récupère les temps cumulés actuels = état à t1
    if (!GetSystemTimes(&repos, &kernel, &user)) return -1.0;

    //conversion en entiers 64 bits via la fonction utilitaire
    unsigned long long t1_repos = conversion_FILETIME_int64(repos);
    unsigned long long t1_kernel = conversion_FILETIME_int64(kernel);
    unsigned long long t1_user = conversion_FILETIME_int64(user);

    //calcul du t1-t0
    unsigned long long diff_repos = t1_repos - t0_repos;
    unsigned long long diff_kernel = t1_kernel - t0_kernel;
    unsigned long long diff_user = t1_user - t0_user;

    // mise à jour de t0 pour la prochaine itération : rappel t0_* déclaré en static donc il conservera sa valeur pour le prochain appel
    t0_repos = t1_repos;
    t0_kernel = t1_kernel;
    t0_user = t1_user;

    // sous windows kernel inclu le repos, donc tot = kernel + user
    unsigned long long total_sys = diff_kernel + diff_user;

    if (total_sys == 0) return 0.0;

    // le temps de CPU utilisé = total - repos
    // formule pour le % = (total - repos) / total * 100
    return (float)(total_sys - diff_repos) / total_sys * 100.0;
}

/*
Fonction utilitaire qui convertit un FILETIME (un type de temps Windows) en entier 64 bits.
*/
unsigned long long conversion_FILETIME_int64(FILETIME ft) {
    ULARGE_INTEGER int_64;
    int_64.LowPart = ft.dwLowDateTime;
    int_64.HighPart = ft.dwHighDateTime;
    return int_64.QuadPart; //QuadPart permet de "recoller les deux morceaux" en un entier 64bits 
}


/*
Fonction qui permet de compteur le nombre de processus actifs à un instant donné.
Retourne le nombre de processus actifs.
*/
uint16_t get_nombre_process() {
    //prise de "photo" de tous les processus au système
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    //si erreur, on renvoie juste 0 poru ne pas faire planter ni le client ni le serveur
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    //structure pour stocker les infos d'un processus
    PROCESSENTRY32 pe32;
    
    //meme principe que pour le %CPU, on donne la taille de la structure pour que Windows en connasise la version (=versionning)
    pe32.dwSize = sizeof(PROCESSENTRY32);

    //récupération du premier processus : si ça échoue on nettoie tout et on renvoie 0
    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot); //libère la mémoire
        return 0;
    }

    uint16_t nproc = 0;

    //tant qu'il y a un processus suivant, on boucle
    do {
        nproc++;
        //on pourrait afficer le nom du processus via pe32.szExeFile mais ça n'a pas d'intérêt ici mais c'est bon à savoir
    } while (Process32Next(hSnapshot, &pe32));

    //libération de la mémoire
    CloseHandle(hSnapshot);
    
    return nproc;
}