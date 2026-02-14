/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */
#define _GNU_SOURCE //pour ne plus avoir l'alerte VSCode "pointer or reference to incomplete type "struct addrinfo" is not allowed "


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h> //pour mutex
#include "../include/protocol.h"
#include "../include/serveur_web.h"
#include "../include/serveur.h" //constant MAX_CLIENTS, SuiviClients, clients, clients_mutex

#define TAILLE_BUFFER 4096  // définit la taille de tous les buffers utilisés


/* réutilisation de la fonction answer vue en cours.
 * MODIFICATION CHAPITRE 10 : la fonction ne fork plus et autres ajustements
 * @param port (char*) le port sur lequel démarrer le serveur.
 * @param fun (int) la fonction callback à appeler une fois la socket ouverte.
 * @return (char*) un message d'erreur si la socket n'est pas ouverte, ou alors lance la fonction callback.
 */
char *answer(char * port, int (* fun)(int ) ) {
    struct addrinfo * info, * p;
    struct addrinfo indices;
    int fd, t;
    int toclient ;
    struct sockaddr fromaddr;
    unsigned int len = sizeof fromaddr;

    memset(&indices, 0, sizeof indices) ;
    indices.ai_flags = AI_PASSIVE; // accepter de toutes les machines
    indices.ai_family = AF_INET;// seulement IPv4
    indices.ai_socktype = SOCK_STREAM; // seulement TCP

    t = getaddrinfo(0, port, &indices, &info);
    if ( t != 0)
        return "answer: cannot get info on port";

    // Ouvrir la socket
    for( p = info; p != 0; p = p->ai_next){
        fd = socket(p->ai_family, p->ai_socktype, 0); // fabriquer la socket
        if (fd >= 0) {
             //éviter "Address already in use" lors des redémarrages rapides
            t = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof t);
            if (bind(fd, p->ai_addr, p->ai_addrlen) == 0)
                break;
            close(fd);
        }
    }
    if ( p == 0)
        return "answer: pas moyen de binder la socket";

    freeaddrinfo(info);

    if (listen(fd, 5) < 0) { //file d'attente de 5
        close(fd);
        return "answer: pas moyen d'ecouter sur la socket";
    }

    for(;;) {

        toclient = accept(fd, &fromaddr, &len);
        if ( toclient < 0) continue; // continue au lieu de return pour ne pas planter le serveur juste pour un accept() échouté

        // attention ici plus de fork() : traitement séquentiel
        fun(toclient);

        //il faut fermer le socket client à la main puisqu'il n'y a plus de fork()
        close(toclient);
    }
}

// --- UTILITAIRES HTTP ---
// partie serveur WEB : GET uniquement, HTML, JSON, CSS uniquement

/* Détermine le type MIME et vérifie si autorisé.
 *
 * @param fichier Le nom du fichier demandé.
 * @return Le type MIME ou NULL.
 */
const char* trouver_mime_type(const char *fichier) {
    
    const char *point = strrchr(fichier, '.'); // le fichier demandé doit être de former "nom_fichier.extension"
    if (!point) return NULL; //pas d'extension donc on refuse la demande qui est invalide

    if (strcmp(point, ".html") == 0) return "text/html";
    if (strcmp(point, ".css") == 0)  return "text/css";
    if (strcmp(point, ".json") == 0) return "application/json";
    if (strcmp(point, ".js") == 0)   return "application/javascript"; //ajout Chap 10
    
    return NULL; //pour tout autre extension, on refuse également
}


/* Construit et envoie la réponse HTTP rapide (pour les messages d'erreur).
 *
 * @param fd Le file descriptor de la socket ouverte.
 * @param statut (int) le status code de la réponse envoyée (exemple : 404 pour une page introuvable).
 * @param statut_msg (char*) l'explication du status code.
 * @param mime_type (char*) le MIME type du message.
 * @param body (char*) le corps du message.
 * @param body_len (int) la taille du message.
 * @return void.
 */
void envoyer_reponse_rapide(int fd, int statut, char *statut_msg, char *mime_type, char *body, int body_len) {
    char entete[TAILLE_BUFFER];
    int entete_len = snprintf(entete, TAILLE_BUFFER,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n" //ajout du charset afin d'afficher correctement les accents
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        statut, statut_msg, mime_type, body_len);
    
    write(fd, entete, entete_len);
    if (body_len > 0) {
        write(fd, body, body_len);
    }
}

// --- GENERATION JSON ---
/*
Fonction helper pour générer le JSON global à la demande
*/
void generer_json_global(char *buffer, size_t size) {
    //début du json
    strcpy(buffer, "[");
    int first = 1;

    // verrouillage mutex pour lire les données sans conflit avec le thread qui reçoit les paquets en continu ("snapshot" des données à l'instant T)
    pthread_mutex_lock(&clients_mutex); 

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd > 0) { //s'assure que le client est connecté 
            if (!first) strcat(buffer, ",");
            first = 0;

            char temp[512];

            PaquetReseau *p = &clients[i].dernier_envoi; 

            //données communes
            snprintf(temp, sizeof(temp), 
                "{\"id\":%d, \"type\":%d, \"nom\":\"%s\", \"cpu\":%.1f, \"ram\":%.1f, \"temp\":%.1f",
                i, p->machine_type, p->machine_nom, 
                p->cpu_usage, p->ram_usage, p->temperature);

            //données spécifiques
            if(p->machine_type == TYPE_PC_LINUX || p->machine_type == TYPE_PC_WINDOWS || p->machine_type == TYPE_RASPBERRY) {
                snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), 
                    ", \"proc\":%d, \"gpu\":%.1f}", p->spec.ordi.nombre_process, p->spec.ordi.gpu_usage);
            }
            else if (p->machine_type == TYPE_ROBOT_VOITURE){
                snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), 
                    ", \"bat\":%lf, \"dist\":%lf}", p->spec.voiture.niveau_batterie, p->spec.voiture.obstacle_distance_cm);
            }
            else if (p->machine_type == TYPE_ROBOT_BRAS){
                snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), 
                    ", \"pince\":%d, \"servos\":[%d,%d,%d,%d,%d]}", 
                    p->spec.bras.etat_pince, 
                    p->spec.bras.servo_positions[0], p->spec.bras.servo_positions[1],
                    p->spec.bras.servo_positions[2], p->spec.bras.servo_positions[3],
                    p->spec.bras.servo_positions[4]);
            }
            else {
                strcat(temp, "}"); //pour sécurité au cas où le type ne correspondà rien
            }
            
            //vérification qu'on n'a pas dépassé la capacité du buffer
            if (strlen(buffer) + strlen(temp) < size - 2) {
                strcat(buffer, temp);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(buffer, "]"); //on pense à fermer le tableau json sinon le fichier est mauvais
}


/* Permet de gérer le client (est appelée par answer()).
 *
 * @param sock (int) le descripteur de socket.
 * @return -1 si la requête est invalide (mal formée ou erreur de lecture), 0 sinon.
 */
int gestion_client(int sock) {
    char buffer[TAILLE_BUFFER];
    char methode[16]; //utilisée pour sotkcer la méthode demandée (GET, POST...)
    char path[256];// utilisée our stocker le chemin du fichier demandé (/index.html)
    char protocole[16];//utilisée pour stocker le protocole demandé dans la requete (HTTP/1.1)
    
    //Lecture de la requête envoyée dans la socket
    int n = read(sock, buffer, TAILLE_BUFFER - 1); //on transfert le contenu de la socket dans le buffer
    if (n < 0) return -1; //read() renvoie -1 s'il y a une erreur de lecture donc on sort de la fonction si c'est le cas
    buffer[n] = '\0'; //on termine correctement la chaîne de caractère transférée

    //On parse la première ligne récupérée (exempe: GET /index.html HTTP/1.1)
    if (sscanf(buffer, "%s %s %s", methode, path, protocole) < 2) {
        char *msg = "400 : Requête invalide";
        envoyer_reponse_rapide(sock, 405, "Bad request", "text/plain", msg, strlen(msg));
        return -1; //la requête est invalide puisqu'on n'a pas les 3 parties requises donc on sort
    }

    //On vérifie que la  requête est bien GET, sinon on refuse
    if (strcmp(methode, "GET") != 0) {
        char *msg = "405 : Méthode non autorisée (GET seulement)";
        envoyer_reponse_rapide(sock, 405, "method Not Allowed", "text/plain", msg, strlen(msg));
        return 0; //on retourne 0 et pas -1 car ce n'est pas un problème de requête mais de type de requête
    }

    /* Ajout du chapitre 10 : interception dynamique de la demande GET /data.json pour envoi dynamique de la data */
    if (strcmp(path, "/data.json") == 0) {
        char json_buffer[8192]; //buffer assez gros pour contenir les données de tous les clients
        generer_json_global(json_buffer, sizeof(json_buffer));
        
        //envoi immédiat de la réponse puisque la requête est traitée et qu'on n'a pas besoin des autres vérifications d'erreur
        char entete[TAILLE_BUFFER];
        int entete_len = snprintf(entete, TAILLE_BUFFER, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n" //debug facile (CORS)
            "\r\n", 
            strlen(json_buffer));

        write(sock, entete, entete_len);
        write(sock, json_buffer, strlen(json_buffer));
        return 0; //on a traité la requête donc on sot de la fonction ici
    }
    //------FIN de l'ajout CHAP 10-----------------------


    //Gestion pour les fichiers statiques 
    /* ---- Protections convtre le Directory Traversal ----*/
    // interdiction de chemins contenant des ".."
    if (strstr(path, "..") != NULL) {
        char *msg = "403 : accès interdit (tentative de Directory Traversal)";
        envoyer_reponse_rapide(sock, 403, "Forbidden", "text/plain", msg, strlen(msg));
        return 0;
    }

    //vérification sur le début du chemin demandé
    if (path[0] != '/') {
        char *msg = "400 : chemin invalide";
        envoyer_reponse_rapide(sock, 400, "Bad Request", "text/plain", msg, strlen(msg));
        return 0;
    }
    /*----*/

    //vérification sur le type de fichier demandé
    const char *mime_type = trouver_mime_type(path); //la fonction renvoie soit le type MIME, soit NULL si le type n'est pas autorisé
    if (mime_type == NULL) {
        char *msg = "403 : le type de fichier demandé n'est pas autorisé";
        envoyer_reponse_rapide(sock, 403, "Forbidden", "text/plain", msg, strlen(msg));
        return 0;
    }

    //construction du chemin relatif vers le fichier demandé
    char path_relatif[512] = "."; //on démarre dans le dossier coursant (celui dans lequel le serveur est lancé)
    if (strcmp(path, "/") == 0) { //on définit /index.html comme page donnée par défaut lorsqu'on se connecte au serveur
        strcat(path_relatif, "/index.html"); //construction de la chaîne finale pour le chemin du fichier
    } else {
        strcat(path_relatif, path); //si la requête demande une page précise, on construit le chemin à partir du dossier courant
    }

    //arrivé ici, on a la bonne méthode + type de fichier autorisé
    int fd_fichier = open(path_relatif, O_RDONLY); // on essaie d'ouvrir le fichier demandé : si open() échoue, on obtient -1
    if(fd_fichier < 0){
        char *msg = "404 File Not Found"; //fameuse erreur 404
        envoyer_reponse_rapide(sock, 404, "File Not Found", "text/plain", msg, strlen(msg));
    } else { //le fichier existe

        //on détermine la taille du fichier
        //ici je n'utilise pas fseek / SEEK_END pour ne p'as avoir besoin ensuite de remettre le curseur au début avec un second appel fseek SEEK_SET
        //la taille est sotckée dans stat.st_size
        struct stat metadonnees;
        fstat(fd_fichier, &metadonnees);
        
        //Ici on ne veut pas envoyer une réponse rapide, car en cas de gros fichier, on doit écrire dans la socket en plusieurs fois et donc on ne veut pas fermer la connexion
        //construction de l'en-tente "200 OK"
        char entete[TAILLE_BUFFER];
        int entete_len = snprintf(entete, TAILLE_BUFFER, 
            "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %ld\r\n\r\n", 
            mime_type, metadonnees.st_size);

        //envoi d'un premier bloc contenant juste l'en-tete dans la socket
        write(sock, entete, entete_len);

        //si le fichier est plus gros que la taille du buffer, il faut l'écrire en plusieurs fois ; chque écriture est donc de taille maximale "TAILLE_BUFFER"
        while((n = read(fd_fichier, buffer, sizeof(buffer))) > 0) { //tant que c'est > 0 il reste des choses dans le fd du fichier (0 pour EOF, -1 pour erreur)
            write(sock, buffer, n);
        }
        close(fd_fichier); //on pense à fermer le fichier pour éviter les uites de mémoire
    }

    return 0; //arrivé ici, c'est qu'on a transmis un fichier
}

/*
Plus de fonction main() ici puisque le serveur web est une "librairie"

int main() {
    char *erreur = answer(PORT_SERVER_WEB, gestion_client);
    if(erreur){
        fprintf(stderr, "%s\n", erreur); //affichange de l'erreur
        return 1;
    }
    return 0;
}*/