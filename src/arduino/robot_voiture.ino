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
		
		//on simule les servos
		int a = random(0, 180);
		int b = random(0, 180);
		int c = random(0, 180);
		int d = random(0, 180);
		int e = random(0, 180);
		int pince = random(0, 2);
		
		//envoi formaté vers le port série : {S1:XXX;S2:XXX;S3:XXX;S4:XXX;S5:XXX;P:X}
		Serial.print("{S1:");
		Serial.print(a);
		Serial.print(";S2:");
		Serial.print(b);
		Serial.print(";S3:");
		Serial.print(c);
		Serial.print(";S4:");
		Serial.print(d);
		Serial.print(";S5:");
		Serial.print(e);
		Serial.print(";P:");
		Serial.print(pince);
		Serial.println("}"); // println ajoute \n contrairement au print normal, donc on finit par lui
		
		delay(1000); //1 envoi/seconde
	}