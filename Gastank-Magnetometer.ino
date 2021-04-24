/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.


//Vereinigung von
// http://www.mysensors.org/build/binary
// Sketch DallasTemperatureSensor_Batterie (Allgemeiner Temperatursensor im Sensornetzwerk)
// http://www.mysensors.org/build/binary
// Zusammengesetzt am 29.10.2020
// Knoten ist #30 für Zisternenpegel und 2x Drainage-Pumpenstatus

 

/**************************************************************************************/
/* DEFINITIONEN                                                                       */
/**************************************************************************************/

#define MY_DEBUG
#define MY_RADIO_RFM69
#define MY_IS_RFM69HW
//#define MY_DEBUG_VERBOSE_RFM69
#define MY_RFM69_NEW_DRIVER
#define MY_RFM69_FREQUENCY   (RFM69_868MHZ)
#define MY_RFM69_TX_POWER_DBM   (14) 
#define MY_RFM69_NETWORKID   (100)
#define MY_BAUD_RATE 57600

// Knotennummer
#define MY_NODE_ID 30

// Wachzyklendefinition aus dem Temperatursensor entfällt erstmal, weil die Schaltung dauerlaufen kann

// Pindefinitionen Ausgänge
int LED_Test = 3;                     // Standard Arduino Pro Mini LED Pin PD3 (Pin3)
int LED_Indikator_Pumpe_1 = 5;      // Port D5
int LED_Indikator_Pumpe_2 = 4;      // Port D4

// Pindefinitionen Eingänge
int Taster_Indikator_Reset = A1;       // Port C1, aktiv LOW
int Pumpensensor_1 = A3;              // Port C3, aktiv High
int Pumpensensor_2 = A2;              // Port C2, aktiv High


#include <SPI.h>
#include <MySensors.h>


// Signalpin für Analogwert
int Zisternenstand = A0;              // Port C0, 0...1024 mV

// Mitteilungen definieren
// Raum für Verbesserung: Für Pumpenzustand nur ein Integer setzen und dort die einzelnen Bits setzen.
// Da Sender aber nur sporadisch sendet (wenn sich Werte ändern) und nicht batteriebetrieben ist, kann man das so lassen.
MyMessage wasserpegel(0, V_VOLUME);    // 1 ist Child-ID 0 und Analogwert XXXX
MyMessage pumpenzustand1(1, V_TRIPPED);    // 2 ist Child-ID 1 und Zustand Pumpe 1
MyMessage pumpenzustand2(2, V_TRIPPED);    // 3 ist Child-ID 1 und Zustand Pumpe 2
MyMessage persistent1(3, V_TRIPPED);    // 4 ist Child-ID 3 und persistierender Zustand Pumpe 1
MyMessage persistent2(4, V_TRIPPED);    // 5 ist Child-ID 4 und persistierender Zustand Pumpe 2

// Hilfsvariablen definieren
int fuellstand;                         // Füllstand als Analogwert
bool pumpe1 = false ;                   // aktueller Pumpenzustand 1
bool pumpe2 = false ;                   // aktueller Pumpenzustand 2
bool pumpe1_persistent = false;         // persistierter Pumpenzustand 1
bool pumpe2_persistent = false;         // persistierter Pumpenzustand 2

int fuellstand_alt = 999;
bool pumpe1_alt = false ;                   // alter Pumpenzustand 1
bool pumpe2_alt = false ;                   // alter Pumpenzustand 2
bool pumpe1_persistent_alt = false;         // alter persistierter Pumpenzustand 1
bool pumpe2_persistent_alt = false;         // alter persistierter Pumpenzustand 2

float fuellstand_rechnung;


void before()
{

  // Clear EEPROM, wenn nötig
  
}




/**************************************************************************************/
/* SETUP                                                                              */
/**************************************************************************************/


void setup()  
{  
  // Ausgänge als solche setzen
  pinMode (LED_Test, OUTPUT);
  pinMode (LED_Indikator_Pumpe_1, OUTPUT);
  pinMode (LED_Indikator_Pumpe_2, OUTPUT);

  //Eingänge als solche setzen
  pinMode(Taster_Indikator_Reset,INPUT);
  pinMode(Pumpensensor_1,INPUT);
  pinMode(Pumpensensor_2,INPUT);

  // Pumpenreset braucht internen Pull-Up, die Anderen bekommen definierte Pegel
  digitalWrite(Taster_Indikator_Reset,HIGH);

  // Analogwertmessung mit interner Referenz
  analogReference(INTERNAL);

}

/**************************************************************************************/
/* PRESENTATION                                                                       */
/**************************************************************************************/

void presentation() {
  
  sendSketchInfo("Drainage", "1.0");
  present(0, S_WATER);
  present(1, S_DOOR);  
  present(2, S_DOOR); 
  present(3, S_DOOR);
  present(4, S_DOOR);

}


/**************************************************************************************/
/* LOOP                                                                               */
/**************************************************************************************/


void loop() 
{

  // Analogwert lesen
  fuellstand = 0;
  for (int i = 0; i <= 10; i++) {
    fuellstand = fuellstand + analogRead(Zisternenstand);
    wait (13); // teilerfremd zu Netzbrumm
  }
  fuellstand = fuellstand / 10;
  
  

  #ifdef MY_DEBUG
  Serial.print("Fuellstand roh: ");
  Serial.println(fuellstand);
  #endif

  // Gemessene Werte:
  // Minimalwert (400 mV) entspricht 375 Digits ==> 0%
  // Vollzustand (1000 mV) entspricht 937 Digits ==> 100%
  // Informatorisch: Stützpunkte für Spannung zu Digits
  //       600 mV =   561
  //       800 mV =   750
  //       1000 mV =  937
  
  fuellstand_rechnung = (0.178 * fuellstand) - 66.72;

  #ifdef MY_DEBUG
  Serial.print("berechneter Fuellstand: ");
  Serial.println(fuellstand_rechnung);
  #endif
  
  fuellstand = int (fuellstand_rechnung);
  
  if (fuellstand>100) fuellstand = 100;
  if (fuellstand<0) fuellstand = 0;

  fuellstand = rundenAuf(fuellstand, 5);

  #ifdef MY_DEBUG
  Serial.print("gerundeter Fuellstand in Prozent: ");
  Serial.println(fuellstand);
  #endif


  // Signal vom Pumpensensor ansehen
  pumpe1 = digitalRead(Pumpensensor_1);
  pumpe2 = digitalRead(Pumpensensor_2);

  // LED schalten wenn nötig
  if (pumpe1 == true) {
    digitalWrite(LED_Indikator_Pumpe_1,HIGH);   // Warn-LED an
    pumpe1_persistent = true;                   // Persistenz-Status setzen
  }
  if (pumpe2 == true) {
    digitalWrite(LED_Indikator_Pumpe_2,HIGH);
    pumpe2_persistent = true;
  }

  // Rücksetztaster auswerten
  int rueckgesetzt = digitalRead(Taster_Indikator_Reset);
  if (rueckgesetzt == LOW) {
    digitalWrite(LED_Indikator_Pumpe_1,LOW);    // Warn-LED aus
    digitalWrite(LED_Indikator_Pumpe_2,LOW);
    pumpe1_persistent = false;                  // Persistenz-Status löschen
    pumpe2_persistent = false;
  }

  // Senden
 
  if ((fuellstand_alt != fuellstand) || (pumpe1_alt != pumpe1) || (pumpe2_alt != pumpe2) || (pumpe1_persistent_alt != pumpe1_persistent) || (pumpe2_persistent_alt != pumpe2_persistent)) {

  digitalWrite(LED_Test, HIGH);                  // Test-LED anschalten: Sendebetrieb

  send(pumpenzustand1.set(pumpe1));
  send(pumpenzustand2.set(pumpe2));
  send(persistent1.set(pumpe1_persistent));
  send(persistent2.set(pumpe2_persistent));
  send(wasserpegel.set(fuellstand));             // Wasserstand zuletzt, weil einfache Python-Auswertung nur den letzten Wert nimmt (keine Auswertung nach Child-ID)

  fuellstand_alt = fuellstand;
  pumpe1_alt = pumpe1;                           // wenn gesendet wurde: alte Alt-Werte aktualisieren
  pumpe2_alt = pumpe2;
  pumpe1_persistent_alt = pumpe1_persistent;
  pumpe2_persistent_alt = pumpe2_persistent;

    digitalWrite(LED_Test, LOW);                   // Test-LED ausschalten

  // Alle Variablen zur Kontrolle ausgeben

  #ifdef MY_DEBUG
  Serial.print("Fuellstand alt/neu ");
  Serial.print(fuellstand_alt);
  Serial.println(fuellstand);
  
  Serial.print("Pumpe1 alt/neu ");
  Serial.print(pumpe1_alt);
  Serial.println(pumpe1);

  Serial.print("Pumpe2 alt/neu ");
  Serial.print(pumpe2_alt);
  Serial.println(pumpe2);
  
  Serial.print("Pumpe1 persistent alt/neu ");
  Serial.print(pumpe1_persistent_alt);
  Serial.println(pumpe1);

  Serial.print("Pumpe2 persistentalt/neu ");
  Serial.print(pumpe2_persistent_alt);
  Serial.println(pumpe2);
  #endif



  }


  wait (500);                                    // bedeutet auch halbe Sekunde Latenz für Reset-Taster!
 

 }



unsigned int rundenAuf(unsigned int zahl, byte auf)
{
  return ((zahl+auf/2)/auf)*auf;
}



