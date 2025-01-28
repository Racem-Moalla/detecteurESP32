#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

// Configurations Wi-Fi
const char* ssid = "presence";
const char* password = "xhc6T7mASs";

// Adresse IP du broker MQTT
const char* mqttServer = "X.X.X.X";
const int mqttPort = 1883;

// Identifiants MQTT
const char* mqttUsername = "admin";
const char* mqttPassword = "xhc9QmmISs";

// Initialisation des objets Wi-Fi et MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Configuration BLE
BLEScan* pBLEScan;

// Structure pour stocker les informations des appareils détectés
struct DeviceInfo {
  String uuid;
  uint16_t major;
  uint16_t minor;
  String macAddress;
};

// Tableau pour stocker les appareils détectés
const int maxEntries = 50;
DeviceInfo detectedDevices[maxEntries];
int deviceCount = 0;

// Adresse MAC BLE du détecteur
String macBleDuDetecteur = "";

// Constante ID du porte
const int x = 1; // Remplacez par l'ID du porte souhaité

// Variables de temporisation pour le scan
unsigned long previousMillisScan = 0; // Temps du dernier démarrage du scan
unsigned long previousMillisReset = 0; // Temps du dernier reset de scan
const long scanDuration = 60000; // Durée du scan en millisecondes (1 minute)
const long waitDuration = 900000; // Durée de l'attente entre les scans en millisecondes (15 minutes)

bool scanningComplete = false; // Indicateur pour savoir si le scan est terminé

// Fonction pour récupérer la MAC BLE du détecteur
void getMacBleDuDetecteur() {
  BLEDevice::init("");
  macBleDuDetecteur = BLEDevice::getAddress().toString().c_str();
  macBleDuDetecteur.toUpperCase();
  BLEDevice::deinit();
  Serial.println("MAC BLE du détecteur : " + macBleDuDetecteur);
}

// Fonction pour se connecter au Wi-Fi
void connectWiFi() {
  Serial.println("Connexion au Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connecté!");
}

// Fonction pour se connecter au broker MQTT
void connectMQTT() {
  client.setServer(mqttServer, mqttPort);
  while (!client.connected()) {
    Serial.println("Connexion au broker MQTT...");
    if (client.connect("ESP32Client", mqttUsername, mqttPassword)) {
      Serial.println("Connecté au broker MQTT!");
    } else {
      Serial.print("Echec, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// Fonction pour démarrer un scan BLE
void startScan() {
  Serial.println("Scan BLE démarré.");
  pBLEScan->start(scanDuration / 1000, false); // Scan pendant la durée définie (60 secondes)
}

// Classe personnalisée pour gérer les périphériques BLE détectés
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String manufacturerData = advertisedDevice.getManufacturerData();
      Serial.print("Taille des données fabricant : ");
      Serial.println(manufacturerData.length());

      if (manufacturerData.length() >= 22) {
        // Extraction de l'UUID (16 octets à partir de l'index 2)
        String uuid = "";
        for (int i = 2; i < 18; i++) {
          uuid += String(manufacturerData[i] >> 4, HEX);
          uuid += String(manufacturerData[i] & 0x0F, HEX);
          if (i == 5 || i == 7 || i == 9 || i == 11) uuid += "-";
        }
        uuid.toUpperCase();

        if (uuid == "2D7A9F0C-E0E8-4CC9-A71B-A21DB2D034A1") {
          // Extraire le champ Major (octets 16-17)
          uint16_t major = (manufacturerData[18] << 8) | manufacturerData[19];

          // Extraire le champ Minor (octets 18-19)
          uint16_t minor = (manufacturerData[20] << 8) | manufacturerData[21];

          // Extraire l'adresse MAC détectée
          String macAddress = "";
          for (int i = 22; i < 28; i++) {
            macAddress += String(manufacturerData[i] >> 4, HEX);
            macAddress += String(manufacturerData[i] & 0x0F, HEX);
            if (i < 27) macAddress += ":";
          }
          macAddress.toUpperCase();

          // Vérifier si le périphérique est déjà détecté
          bool alreadySent = false;
          for (int i = 0; i < deviceCount; i++) {
            if (detectedDevices[i].uuid == uuid && detectedDevices[i].macAddress == macAddress) {
              alreadySent = true;
              break;
            }
          }

          if (!alreadySent && deviceCount < maxEntries) {
            detectedDevices[deviceCount++] = {uuid, major, minor, macAddress};
            Serial.println("Appareil détecté : " + uuid + " - " + macAddress);
          }
        } else {
          Serial.println("Ce n'est pas l'UUID recherché.");
        }
      } else {
        Serial.println("Données trop petites pour être traitées");
      }
    } else {
      Serial.println("Aucune donnée de fabricant disponible");
    }
  }
};

// Fonction d'initialisation
void setup() {
  Serial.begin(115200);

  getMacBleDuDetecteur();
  connectWiFi();
  connectMQTT();

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);

  // Initialisation du scan périodique
  startScan();
}

// Boucle principale
void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  // Si le scan est terminé et qu'il faut attendre 15 minutes avant le prochain scan
  if (scanningComplete) {
    if (currentMillis - previousMillisReset >= waitDuration) {
      previousMillisReset = currentMillis;
      scanningComplete = false;
      deviceCount = 0; // Réinitialiser la liste des appareils détectés
      startScan(); // Démarrer un nouveau scan après 15 minutes
    }
  }

  // Si le scan dure 1 minute
  if (currentMillis - previousMillisScan >= scanDuration) {
    previousMillisScan = currentMillis;
    scanningComplete = true; // Indiquer que le scan est terminé

    // Publier les données sur les topics MQTT
    if (deviceCount > 0) {
      StaticJsonDocument<512> doc;
      JsonArray devices = doc.createNestedArray("devices");

      // Ajouter tous les appareils détectés à la liste
      for (int i = 0; i < deviceCount; i++) {
        JsonObject device = devices.createNestedObject();
        device["idSTRI"] = detectedDevices[i].uuid;
        device["mac_address_detectee"] = detectedDevices[i].macAddress;
        device["année"] = detectedDevices[i].major;
        device["idBadge"] = detectedDevices[i].minor;
      }

      // Ajouter les informations MAC de l'ESP32
      doc["macBLE"] = macBleDuDetecteur;
      doc["macWIFI"] = WiFi.macAddress();

      // Sérialiser le message JSON dans un buffer
      char jsonBuffer[512];
      serializeJson(doc, jsonBuffer);

      // Publication sur le topic `/detecteur/presence/macwifi/xx:xx:xx:xx`
      String macWiFiTopic = "/detecteur/presence/macwifi/" + WiFi.macAddress();
      bool success = client.publish(macWiFiTopic.c_str(), jsonBuffer);

      if (success) {
        Serial.println("Message envoyé au topic : " + macWiFiTopic);
        Serial.println("Payload : " + String(jsonBuffer));
      } else {
        Serial.println("Erreur d'envoi du message.");
      }

      // Publication sur le topic `/detecteur/presence/iddetecteur/x`
      String idDetecteurTopic = "/detecteur/presence/iddetecteur/" + String(x);
      success = client.publish(idDetecteurTopic.c_str(), jsonBuffer);

      if (success) {
        Serial.println("Message envoyé au topic : " + idDetecteurTopic);
        Serial.println("Payload : " + String(jsonBuffer));
      } else {
        Serial.println("Erreur d'envoi du message.");
      }

      // Réinitialiser les variables pour le prochain scan
      deviceCount = 0;
    }
  }
}
