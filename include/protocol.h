#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h> //pour les types entiers non signés

//énumération des types de machines
typedef enum {
    TYPE_PC_LINUX = 1,
    TYPE_PC_WINDOWS,
    TYPE_RASPBERRY,
    TYPE_ROBOT_BRAS,
    TYPE_ROBOT_VOITURE
} MachineType;

/*Données spécifiques à ranger dans une union*/

//Bras robot adeept : on force les int à faire 8 et 16 bits pour uniformiser la taille du paquet à travers les différentes architectures
typedef struct {
    int16_t servo_positions[5]; // angles des 5 axes
    uint8_t etat_pince;      // 0 = ouvert, 1 = fermé
} DonneesBras;

//Voiture robot MakeBlock
typedef struct {
    float obstacle_distance_cm; //capteur ultrason
    float niveau_batterie;      //niveau de batterie
} DonneesVoiture;

//Machines de calcul(PC et RPi) on force les int à faire 8 et 16 bits pour uniformiser la taille du paquet à travers les différentes architectures
typedef struct {
    float gpu_usage;  // 0-100 (pour PC)
    uint16_t nombre_process;     //nmbre de processus actifs
} DonneesOrdi;

/*Union permettant de regrouper les 3 structures ci-dessus en une même structure (un seul champ peut être occupé à la fois) */
typedef union {
    DonneesBras bras;
    DonneesVoiture voiture;
    DonneesOrdi  ordi;
} DonneesSpecifiques;


/*
Paquet réseau perso. L'utilisation de lattribut packed est recommandé pour échanger des messages sur le réseau (cela permet d'éviter de combler les données avec des octets vides pour aligner la mémoire).
Dans le cas présent, cela permet aussi à des machines sur architectures différentes d'échanger des données sans problème.
Là aussi on force les int à faire 8 et 16 bits pour uniformiser la taille du paquet à travers les différentes architectures
*/
typedef struct __attribute__((packed)) {
    uint32_t magic_nbr;      //vérification du protocole
    uint32_t paquet_id; //AJOUT du 02/02/2026 : numéro de paquet pour double vérification malgré proto TCP
    uint8_t  machine_type;      //une des valeurs de l'enum MachineType
    char     machine_nom[64];  //nom de la machine MODIF du 03/02/2026 : passage à 64 car HOST_NAME_MAX == 64 sinon risque de buffer overflow en lecture
    
    //Informations communes à toutes les machines
    float    cpu_usage;         // 0.0 à 100.0
    float    ram_usage;         // 0.0 à 100.0
    float    temperature;       // °C
    
    //Informatios spécifiques (contenu variable)
    DonneesSpecifiques spec;         
    
} PaquetReseau;

//constantes utiles
#define PACKET_SIZE sizeof(PaquetReseau)
#define MAGIC_NUMBER 0x19021993 //identifiant unique pour le protocole
#define PORT_SERVER 8080

#endif // PROTOCOL_H 