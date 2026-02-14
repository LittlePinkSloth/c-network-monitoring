/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */


//simulation de data envoyées par un robot	
	void setup() {
		//initialisation du port série sur 9600 bauds (valeur standard)
		Serial.begin(9600);
	}
	
	void loop() {
		//boucle principale
		
		//on simule les capteurs de distance et de bty pour le moment
		int distance = random(10, 400);
		int batterie = random(0, 100);
		
		//envoi formaté vers le port série : {DIST:XXX;BAT:XX}
		Serial.print("{DIST:");
			Serial.print(distance);
			Serial.print(";BAT:");
			Serial.print(batterie);
			Serial.println("}"); // println ajoute \n)contrairement au print normal, donc on finit par lui
		
		delay(1000); //1 envoi/seconde
	}