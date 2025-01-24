# Code ESP32

Ce dossier contient le code source pour le microcontrôleur ESP32.

## Description

Ce code est responsable de :
- Connexion à un réseau Wi-Fi.
- Récupération des données de l'application mobile et formatage de ces données :
  - Extraction de l'UUID (16 octets à partir de l'index 2).
  - Extraction du champ **Major** (octets 16-17).
  - Extraction du champ **Minor** (octets 18-19).
  - Extraction de l'adresse MAC (octets 22-27).
- Communication avec un serveur (envoi par MQTT vers un Raspberry Pi).

## Pré-requis

- ESP32 ou module compatible.
- Outils nécessaires :
  - Arduino IDE (ou PlatformIO dans VS Code).
- Bibliothèques nécessaires :
  - `WiFi`
  - `PubSubClient` pour MQTT.
- Modification du schéma de partition :
  - Allez dans **Outils > Partition Scheme** et sélectionnez : **2MB APP / 2MB SPIFFS**.

## Installation

1. Connectez votre ESP32 à votre ordinateur.
2. Ouvrez le fichier `.ino` dans Arduino IDE (ou configurez le projet dans PlatformIO).
3. Configurez les paramètres Wi-Fi :
   ```cpp
   const char* ssid = "Votre_SSID";
   const char* password = "Votre_MotDePasse";
