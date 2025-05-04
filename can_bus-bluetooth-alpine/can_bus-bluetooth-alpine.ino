#include <SPI.h>
#include <mcp_can.h>

// Pour passer en Mode CONFIGURATION => rester appuyer sur le bouton en allumant
// Pour passer en Mode COMMUNICATION => courcicuiter EN et VCC ern allumant
#include <SoftwareSerial.h>

/* Un croquis Arduino pour contrôler l'Alpine GU via une connexion filaire mini-jack */
/* Initialement conçu pour les boutons de volant Citroen XM */
/* Roman Yadrovsky */
/* http://cx.podolsk.ru/board/memberlist.php?mode=viewprofile&u=3887 */
/* http://www.drive2.ru/r/citroen/288230376152072501 */

/* Sources pour commande volant Alpine:
 https://github.com/morcibacsi/PSASteeringWheelAdapter
 https://github.com/jadrovski/alpine_control/blob/master/alpine_control.ino
 https://forum.arduino.cc/t/interface-commande-volant-autoradio-xsara-ph1-jvc/548663
 https://www.avforums.com/threads/jvc-stalk-adapter-diy.248455/page-4 */

/* Sources pour CAN BUS:
https://forum.arduino.cc/t/nano-with-mcp2515-canbus-questions/1151418/8
https://medium.com/@alexandreblin/can-bus-reverse-engineering-with-arduino-and-ios-5627f2b1709a
http://romain.raveaux.free.fr/teaching/TP207.pdf */


// Commandes pour Alpine GU
#define cmdPower 0x09
#define cmdSource 0x0A
#define cmdBand 0x0D
#define cmdVolumeDown 0x15
#define cmdVolumeUp 0x14
#define cmdUp 0x0E
#define cmdDown 0x0F
#define cmdLeft 0x13
#define cmdRight 0x12
#define cmdPlay 0x07
#define cmdMute 0x16
#define cmdCDChange 0x03

#define Alpine 2  // Envoyer des signaux de contrôle à D2

#define LONG_PRESS_MS 1000  // durée seuil appui long (1 s)

// variables pour détection appui long
bool srcPrevState = false;
unsigned long srcPressTime = 0;

MCP_CAN CAN(9);  // CS pin

SoftwareSerial bluetooth(3, 4);  // (TXD, RXD)

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
      delay(100);  // Attente courte pour laisser le temps au module Bluetooth HC-05 de répondre
      while (bluetooth.available()) {
        Serial.write(bluetooth.read());
      }
    }
  }
}

void displayReceivedMessage() {
  String receivedString = "";  // Créer une chaîne vide pour stocker les données reçues

  while (bluetooth.available()) {
    char c = bluetooth.read();  // Lire un caractère du flux série
    if (c == '\n') {            // Si un caractère de fin de ligne est reçu
      // Afficher la chaîne reçue
      Serial.print("Recu : ");
      Serial.println(receivedString);
      receivedString = "";  // Réinitialiser la chaîne pour la prochaine réception
    } else {
      receivedString += c;  // Ajouter le caractère à la chaîne reçue
    }
  }
}

// Envoi d'un octet (protocole NEC-Clarion)
void SendByte(byte data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(Alpine, HIGH);
    delayMicroseconds(560);  // Pause entre les battements
    digitalWrite(Alpine, LOW);

    if (data & 1)  // Le dernier bit est multiplié par 1
    {
      delayMicroseconds(1680);  // unité logique
    } else {
      delayMicroseconds(560);  // zéro logique
    }

    data >>= 1;  // Nouvel octet décalé à droite (de 1 bit)
  }
}

// Envoi d'une commande
void SendCommand(byte command) {
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

  delay(50);  // Pause technique
}

void setup() {
  Serial.begin(115200);
  while (CAN.begin(MCP_STDEXT, CAN_125KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("CAN init fail, retrying...");
    delay(1000);
  }
  Serial.println("CAN init ok!");
  CAN.setMode(MCP_NORMAL);

  // Ouvre la voie série avec le module BT
  bluetooth.begin(9600);  // En mode COMMANDE communication a 9600 Baud, pour AT 38400

  pinMode(Alpine, OUTPUT);
}

void loop() {
  long unsigned int id = 0;
  unsigned char len = 0;
  unsigned char buf[8];

  configureModule();
  displayReceivedMessage();

  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    CAN.readMsgBuf(&id, &len, buf);
    //Serial.print("Id : 0x ");
    //Serial.println(id);
    //return;

    /*if (id == 0x09) {
      Serial.println("Ca appuye br");
    }*/

    //Serial.println("br");
    if (id == 0x208 && len >= 6) {  // Infos moteur
      // Régime moteur : octets 0 et 1
      unsigned int rpm_raw = (buf[0] << 8) | buf[1];
      float rpm = rpm_raw / 8.0;

      // Couple réel : octet 2
      int torque = (buf[2] * 2) - 100;

      // Volonté conducteur : octet 3
      float vol_cond = buf[3] / 2.0;

      // Octet 4 : flags
      byte flags = buf[4];
      bool regen_filter_request = flags & 0x80;
      bool diag_net_active = flags & 0x40;
      bool torque_freeze_request = flags & 0x20;
      bool torque_info_uncertain = flags & 0x10;
      byte cruise_control_status = (flags >> 2) & 0x03;
      bool brake_pedal_pressed = flags & 0x02;
      bool compressor_request = flags & 0x01;

      // Couple réel hors réduction BV : octet 5
      int torque_bv = (buf[5] * 2) - 100;

      // Affichage des données
      /*for (int i = 0; i < 20; i++) {
        Serial.println();
      }*/
      //Serial.print("Trame 0x208 décodée :");
      Serial.print("  Régime moteur : ");
      Serial.print(rpm);
      Serial.print(" tr/min br");
      Serial.print("  Couple réel : ");
      Serial.print(torque);
      Serial.print(" Nm br");
      Serial.print("  Volonté conducteur : ");
      Serial.print(vol_cond);
      Serial.print(" % br");
      Serial.print("  Requête régénération FAP : ");
      Serial.print(regen_filter_request ? "Oui br" : "Non br");
      Serial.print("  Diagnostic réseau activé : ");
      Serial.print(diag_net_active ? "Oui br" : "Non br");
      Serial.print("  Demande de figeage couple : ");
      Serial.print(torque_freeze_request ? "Oui br" : "Non br");
      Serial.print("  Infos couple incertaines : ");
      Serial.print(torque_info_uncertain ? "Oui br" : "Non br");
      Serial.print("  État régulation vitesse : ");
      switch (cruise_control_status) {
        case 0: Serial.print("OFF br"); break;
        case 1: Serial.print("Reprise pédale br"); break;
        case 2: Serial.print("ON br"); break;
        default: Serial.print("Inconnu br"); break;
      }
      Serial.print("  Frein appuyé : ");
      Serial.print(brake_pedal_pressed ? "Oui br" : "Non br");
      Serial.print("  Demande compresseur : ");
      Serial.print(compressor_request ? "Oui br" : "Non br");
      Serial.print("  Couple hors boîte : ");
      Serial.print(torque_bv);
      Serial.print(" Nm br");
      Serial.println();
    }

    if (id == 0x520 /*&& len >= 8*/) {
      // Exemple de décodage hypothétique :
      // Octet 0 : source audio (0=radio,1=CD,...)
      byte source = buf[0];
      // Octet 1 : niveau de volume (0–100)
      byte volume = buf[1];
      // Octets 2–3 : numéro de station / piste (16 bits)
      unsigned int station = (buf[2] << 8) | buf[3];
      // Octet 4 : flags divers (mute, stéréo/mono…)
      byte flags = buf[4];
      bool mute = flags & 0x01;
      bool stereo = flags & 0x02;
      // Octets 5–7 : réservé ou données constructeur
      // (à analyser selon vos captures)

      // Affichage
      Serial.print("Trame 0x520 décodée : br");
      Serial.print("  Source audio       : ");
      switch (source) {
        case 0: Serial.print("Radio br"); break;
        case 1: Serial.print("CD br"); break;
        default:
          Serial.print("Autre (");
          Serial.print(source);
          Serial.println(") br");
          break;
      }
      Serial.print("  Volume             : ");
      Serial.print(volume);
      Serial.print(" % br");
      Serial.print("  Station / Piste    : ");
      Serial.print(station);
      Serial.print("  Muet               : ");
      Serial.print(mute ? "Oui br" : "Non br");
      Serial.print("  Stéréo             : ");
      Serial.print(stereo ? "Oui br" : "Non br");
      Serial.println();
    }

    if (id == 0x21F && len >= 3) {  // Info commodo
      byte b0 = buf[0];
      int8_t b1 = (int8_t)buf[1];  // interpréter le 2ᵉ octet comme signé pour la molette
      byte b2 = buf[2];

      Serial.print("Commande volant détectée : ");

      // Push SRC
      if (b0 == 0x02) {
        if (!srcPrevState) {
          // début appui
          srcPrevState = true;
          srcPressTime = millis();
        } else {
          // maintien
          if (millis() - srcPressTime > LONG_PRESS_MS) {
            Serial.println("SRC long press → ACTION SPÉCIALE");
            SendCommand(cmdPower);
            bluetooth.println("Power");
            // n'oublie pas de ne pas réafficher à chaque boucle
            srcPrevState = false;
          }
        }
      } else {
        // relâche
        if (srcPrevState) {
          // si on n'a pas encore déclenché le long press
          if (millis() - srcPressTime < LONG_PRESS_MS) {
            Serial.println("SRC court press → action normale");
            SendCommand(cmdPlay);
            bluetooth.println("play");
          }
          srcPrevState = false;
        }
      }

      // Volume + / –
      if (b0 == 0x08) {
        Serial.println("Volume +");
        SendCommand(cmdVolumeUp);
      } else if (b0 == 0x04) {
        Serial.println("Volume -");
        SendCommand(cmdVolumeDown);
      } else if (b0 == 0xC) {
        Serial.println("Mute");
        SendCommand(cmdMute);
      }

      // Station suivante / précédente (boutons dédiés)
      if (b0 == 0x80) {
        Serial.println("Station >>");
        SendCommand(cmdRight);
        bluetooth.println("next");
      } else if (b0 == 0x40) {
        Serial.println("Station <<");
        SendCommand(cmdLeft);
        bluetooth.println("prev");
      }

      // Molette Mémo station (b0 == 0x00 mais b1 ≠ 0)
      if (b0 == 0x00 && b1 != 0) {
        if (b1 > 0) {
          Serial.print("Molette tournée vers la droite : +");
          Serial.println(b1);
        } else {
          Serial.print("Molette tournée vers la gauche : ");
          Serial.println(b1);
        }
      }

      // Aucun bouton pressé
      if (b0 == 0x00 && b1 == 0) {
        Serial.println("Aucun bouton pressé");
      }
      // Cas général (inconnu)
      else {
        /*Serial.print("Trame inconnue (b0, b1, b2) : ");
          Serial.print(b0, HEX);
          Serial.print(" ");
          Serial.print((byte)b1, HEX);
          Serial.print(" ");
          Serial.print(b2, HEX);
          Serial.println();*/

        Serial.print("Inconnu 21F: ");
        Serial.print(b0, HEX);
        Serial.print(" ");
        Serial.print((uint8_t)b1, HEX);
        Serial.print(" ");
        Serial.println(buf[2], HEX);
      }
    }
  } else {
    //Serial.println("Aucun message");
  }
}
