/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */
#define _GNU_SOURCE //pour ne plus avoir l'alerte VSCode "pointer or reference to incomplete type "struct addrinfo" is not allowed "

#include <dirent.h> //pour type DIR et struct dirent
#include <ctype.h> //pour fonction isdigit()
#include "../include/utils_linux.h"
#include "../include/protocol.h"

//Prototype de la fonction utilitaire qui est à la fin du fichier :
static int get_cpu_stats(unsigned long *repos, unsigned long *total);

//----FONCTIONS DE CONNEXION AU SERVEUR----
/*
Fonction dial du cours (non modifiée)
Pramètres :
- machine : nom de la machine à contacter (ex "localhost" ou adresse IP)
- port : numéro de port à utiliser pour la connexion
*/
int dial(char * machine, char * port) {
    struct addrinfo * info, * p;
    struct addrinfo indices;
    int toserver, t;

    memset(&indices, 0, sizeof indices);//initialisation de hints à 0
    indices.ai_family = AF_INET;     // IPv4
    indices.ai_socktype = SOCK_STREAM; // TCP

    //récupération des informations sur le serveur
    t = getaddrinfo(machine, port, &indices, &info);
    if (t != 0) return -1;

    //on essaie toutes les adreses trouvées
    for(p = info; p != NULL; p = p->ai_next) {
        toserver = socket(p->ai_family, p->ai_socktype, 0);
        if (toserver >= 0) break;
    }

    if (p == NULL) return -1; // Aucune adresse n'a fonctionné donc on sort

    //Connexion au serveur
    t = connect(toserver, info->ai_addr, info->ai_addrlen);
    if (t < 0) {
        freeaddrinfo(info);
        close(toserver);
        return -1;
    }

    freeaddrinfo(info);
    return toserver;
}

//----FONCTIONS DE COLLECTE----

/*
Fonction qui calcule l'utilisation du CPU à un moment donné par rapport au moment t-1. 
Cette fonction utilise les données fournies par la fonction static get_cpu_stats().

Retourne le pourcentage d'utilisation du CPU.
*/
float get_cpu_usage() {
    static unsigned long t0_total = 0;
    static unsigned long t0_repos = 0;

    unsigned long t1_total, t1_repos;

    //récupération des données actuelles
    if (get_cpu_stats(&t1_repos, &t1_total) != 0) {
        return -1.0;
    }

    //initialisation des valeurs pour le premier appel uniquement
    if (t0_total == 0) {
        t0_total = t1_total;
        t0_repos = t1_repos;
        return 0.0; // Pas encore de delta possible
    }

    //calcul des deltas
    unsigned long diff_total = t1_total - t0_total;
    unsigned long diff_repos = t1_repos - t0_repos;

    //mise à jour pour le prchain appel
    t0_total = t1_total;
    t0_repos = t1_repos;

    //calcul pourcentage
    if (diff_total == 0) return 0.0;

    return (float)(diff_total - diff_repos) / diff_total * 100.0;
}

/*
Fonction qui lit la température dans TEMP_PATH.

Return (float): température en °C
 */
float get_temperature(int machine_type) {
    const char *path; //pointeur vers une chaîne de caractère constante

    // On choisit juste le chemin, on n'ouvre pas encore
    if (machine_type == TYPE_PC_LINUX) {
        path = TEMP_PATH_LINUX_PC;
    } else {
        path = TEMP_PATH_LINUX_RPI;
    }

    FILE *f = fopen(path, "r");
    if (!f) return -1.0; //erreur de elcture du fichier

    long temp_milli;
    if (fscanf(f, "%ld", &temp_milli) != 1) { 
        fclose(f); //si fscanf échoue à lire un nombre, au cas où
        return -1.0;
    }
    fclose(f);//ferme le fichier car on n'en a plus besoin

    return temp_milli / 1000.0; //convertit l'entier lu en °C 
}

/*
Fonction qui lit le fichier /proc/meminfo, specit les informations utiles ("MemTotal" et "MemAvailable"), et retourne le pourcentage de la RAM utilisée.
Return (float): pourcentage de la RAM utilisée
*/
float get_ram_usage() {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1.0;//erreur de elcture du fichier

    char ligne[256];
    long total = 0, available = 0;
    int info = 0;

    while (fgets(ligne, sizeof(ligne), f)) {
        //tant qu'on n'a pas récupéré les deux informations désirées, on continue de lire le fichier ligne à ligne
        if (sscanf(ligne, "MemTotal: %ld kB", &total) == 1) info++; //on cherche MemTotal
        if (sscanf(ligne, "MemAvailable: %ld kB", &available) == 1) info++; //on cherche MemAvailable
        if (info == 2) break; //on arrête quand on a récupéré les deux informations voulues
    }
    fclose(f); //fermeture du fichier

    if (total == 0) return 0.0; //indique un soucis dans le parcourt du fichier (il a trouvé que la mémoire totale est de 0 : impossible)
    
    //application de la formule : ((total - dispo) / total) * 100
    return (float)(total - available) / total * 100.0;
}

/*
Fonction qui compte les dossiers numériques dans /proc.
Renvoie le nombre de dossiers trouvés.
Return (unint_16t) : le nombre de processus sur 16 bits (car c'est ce qui est demandé par le protocole)
*/
uint16_t get_nombre_process() {
    struct dirent *proc;
    uint16_t nombre = 0;

    DIR *dp = opendir("/proc");
    if (dp == NULL) return 0; // erreur à l'ouvreture du dossier /proc

    while ((proc = readdir(dp))) {
        //on vérifie juste le premier caractere du nom : si c'est un nombre c'est un prcesus
        if (isdigit(proc->d_name[0])) {
            nombre++;
        }
    }

    closedir(dp);
    return nombre;
}

//--FONCTIONS UTILITAIRES--

/*
Fonction qui récupère les informations sur l'utilisation du CPU à cet instant.
Le fait en lisant les données de /proc/stat.
Param *repos : pointeur qui contiendra le temps total passé en repos
Param *total : pointeur qui contiendra le temps total du CPU dans tous les états possibles
Return (int): 0 si tout s'est bien passé, les valeurs d'intéret sont stockées dans les pointeurs
*/
static int get_cpu_stats(unsigned long *repos, unsigned long *total) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;//erreur de elcture du fichier

    char ligne[256];
    if (!fgets(ligne, sizeof(ligne), f)) { fclose(f); return -1; }
    fclose(f);

    //format: cpu  user nice system idle iowait irq softirq steal guest...
    unsigned long user, nice, system, local_idle, iowait, irq, softirq, steal, guest, guest_nice;
    
    //parsage de la ligne et récupération des 10 variables
    sscanf(ligne, "cpu  %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
           &user, &nice, &system, &local_idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    //calcul des totaux
    *repos = local_idle + iowait; //ici on considère l'attente du disque comme un temps de repos
    *total = user + nice + system + *repos + irq + softirq + steal + guest + guest_nice;

    return 0;
}
