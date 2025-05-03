#include <Bouton.h>

// Pour passer en Mode CONFIGURATION => rester appuyer sur le bouton en allumant
// Pour passer en Mode COMMUNICATION => courcicuiter EN et VCC ern allumant
#include <SoftwareSerial.h>

/* Un croquis Arduino pour contrôler l'Alpine GU via une connexion filaire mini-jack */
/* Initialement conçu pour les boutons de volant Citroen XM */
/* Roman Yadrovsky */
/* http://cx.podolsk.ru/board/memberlist.php?mode=viewprofile&u=3887 */
/* http://www.drive2.ru/r/citroen/288230376152072501 */

/* Sources :
https://github.com/morcibacsi/PSASteeringWheelAdapter
https://github.com/jadrovski/alpine_control/blob/master/alpine_control.ino
https://forum.arduino.cc/t/interface-commande-volant-autoradio-xsara-ph1-jvc/548663
https://www.avforums.com/threads/jvc-stalk-adapter-diy.248455/page-4 */

// Commandes pour Alpine GU
#define cmdPower      0x09
#define cmdSource     0x0A
#define cmdBand       0x0D
#define cmdVolumeDown 0x15
#define cmdVolumeUp   0x14
#define cmdUp         0x0E
#define cmdDown       0x0F
#define cmdLeft       0x13
#define cmdRight      0x12
#define cmdPlay       0x07
#define cmdMute       0x16
#define cmdCDChange   0x03

#define VOL_M 8 // Le bouton VOLUME- sur pin D7
#define PREV 7 // Le bouton Previous Track sur pin D9
#define UP 6 // Le bouton Memo Up sur pin D11
#define PLAY 10 // Le bouton Play/Pause sur pin D10

#define VOL_P 9 // Le bouton VOLUME+ sur pin D6
#define NEXT 11 // Le bouton Next Track sur pin D8
#define DOWN 12 // Le bouton Memo Down sur pin D12

#define Alpine 2 // Envoyer des signaux de contrôle à D2


Bouton volumePBtn(VOL_P);
Bouton volumeMBtn(VOL_M);
Bouton nextBtn(NEXT);
Bouton prevBtn(PREV);
Bouton playBtn(PLAY);
Bouton upBtn(UP);
Bouton downBtn(DOWN);

bool longAppuye = false;

SoftwareSerial bluetooth(3, 4); // (TXD, RXD)

void configureModule() {
  // Si des données sont disponibles sur le port série de l'Arduino
  if (Serial.available()) {
    // Lire la commande entrante
    String command = Serial.readStringUntil('\n');

    // Interpréter et traiter la commande entrante
    if (command.startsWith("AT")) {
      // Si la commande AT est détectée, l'envoyer au module Bluetooth HC-05
      bluetooth.print(command + "\r\n");
      // Attendre la réponse du module Bluetooth HC-05 et l'afficher sur le moniteur série
      delay(100); // Attente courte pour laisser le temps au module Bluetooth HC-05 de répondre
      while (bluetooth.available()) {
        Serial.write(bluetooth.read());
      }
    }
  }
}

void displayReceivedMessage() {
  String receivedString = ""; // Créer une chaîne vide pour stocker les données reçues

  while (bluetooth.available()) {
    char c = bluetooth.read(); // Lire un caractère du flux série
    if (c == '\n') { // Si un caractère de fin de ligne est reçu
      // Afficher la chaîne reçue
      Serial.print("Recu : ");
      Serial.println(receivedString);
      receivedString = ""; // Réinitialiser la chaîne pour la prochaine réception
    } else {
      receivedString += c; // Ajouter le caractère à la chaîne reçue
    }
  }
}

void setup() 
{
  Serial.begin(9600); 

  pinMode(VOL_P, INPUT_PULLUP);
  digitalWrite(VOL_P, HIGH);

  pinMode(VOL_M, INPUT_PULLUP);
  digitalWrite(VOL_M, HIGH);

  pinMode(NEXT, INPUT_PULLUP);
  digitalWrite(NEXT, HIGH);

  pinMode(PREV, INPUT_PULLUP);
  digitalWrite(PREV, HIGH);

  pinMode(PLAY, INPUT_PULLUP);
  digitalWrite(PLAY, HIGH);

  pinMode(UP, INPUT_PULLUP);
  digitalWrite(UP, HIGH);

  pinMode(DOWN, INPUT_PULLUP);
  digitalWrite(DOWN, HIGH);

  // Ouvre la voie série avec le module BT
  bluetooth.begin(9600); // En mode COMMANDE communication a 9600 Baud, pour AT 38400

  pinMode(Alpine, OUTPUT);
}

void loop(void) 
{
  volumePBtn.refresh();
  volumeMBtn.refresh();
  nextBtn.refresh();
  prevBtn.refresh();  
  playBtn.refresh();
  upBtn.refresh();
  downBtn.refresh();

  configureModule();
  displayReceivedMessage();

  if (volumePBtn.isPressed()){
    Serial.println("Volume +");
    SendCommand(cmdVolumeUp);
    //bluetooth.println("vol_up");
  }

  if (volumeMBtn.isPressed()){
    Serial.println("Volume -");    
    SendCommand(cmdVolumeDown);
    //bluetooth.println("vol_down");
  }  

  if (nextBtn.isPressed()){
    Serial.println("Suivant");
    SendCommand(cmdRight);
    bluetooth.println("next");
  }

  if (prevBtn.isPressed()){
    Serial.println("Precedant");
    SendCommand(cmdLeft);
    bluetooth.println("prev");
  }

  if (playBtn.isLongPressed() && longAppuye == false){
    Serial.println("Power");
    SendCommand(cmdPower);
    longAppuye = true;
  }

  if (playBtn.isPressed()){
    Serial.println("Play/Pause");
    SendCommand(cmdPlay);
    bluetooth.println("play");
  }

  if (playBtn.isReleased()){
    longAppuye = false;
  }

  if (upBtn.isPressed()){
    Serial.println("Up");
    SendCommand(cmdSource);
  }

  if (downBtn.isPressed()){
    Serial.println("Down");
    SendCommand(cmdMute);
  }
}

// Envoi d'un octet (protocole NEC-Clarion)
void SendByte(byte data)
{
  for (int i = 0; i < 8; i++)
  {
    digitalWrite(Alpine, HIGH);
    delayMicroseconds(560); // Pause entre les battements
    digitalWrite(Alpine, LOW);

    if (data & 1) // Le dernier bit est multiplié par 1
    {
      delayMicroseconds(1680); // unité logique
    }
    else
    {
      delayMicroseconds(560); // zéro logique
    }

    data >>= 1; // Nouvel octet décalé à droite (de 1 bit)
  }
}

// Envoi d'une commande
void SendCommand(byte command)
{
  // Quelque chose comme un bonjour
  // caractéristique du protocole NEC modifié
  digitalWrite(Alpine, HIGH);
  delayMicroseconds(9000);
  digitalWrite(Alpine, LOW);
  delayMicroseconds(4500);

  // Adresse de protocole (informations de référence publiques)
  SendByte(0x86);
  SendByte(0x72);
  
  // Envoi d'une commande
  SendByte(command);
  SendByte(~command);

  // Pour déterminer le dernier bit (fin de trame)
  digitalWrite(Alpine, HIGH); 
  delayMicroseconds(560); 
  digitalWrite(Alpine, LOW);
  
  delay(50); // Pause technique
}