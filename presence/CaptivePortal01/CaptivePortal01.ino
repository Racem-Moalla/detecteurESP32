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
const char* mqttServer = "10.3.141.1";
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
unsigned long previousMillisScan = 0;
unsigned long previousMillisReset = 0;
const long scanDuration = 60000;  // 1 min
const long waitDuration = 900000; // 15 min

bool scanningComplete = false; // Indicateur de fin de scan

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
  pBLEScan->start(scanDuration / 1000, false);
}

// Classe pour gérer les périphériques BLE détectés
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String manufacturerData = advertisedDevice.getManufacturerData();
      Serial.print("Taille des données fabricant : ");
      Serial.println(manufacturerData.length());

      if (manufacturerData.length() >= 22) {
        // Extraction de l'UUID
        String uuid = "";
        for (int i = 2; i < 18; i++) {
          uuid += String(manufacturerData[i] >> 4, HEX);
          uuid += String(manufacturerData[i] & 0x0F, HEX);
          if (i == 5 || i == 7 || i == 9 || i == 11) uuid += "-";
        }
        uuid.toUpperCase();

        if (uuid == "2D7A9F0C-E0E8-4CC9-A71B-A21DB2D034A1") {
          uint16_t major = (manufacturerData[18] << 8) | manufacturerData[19];
          uint16_t minor = (manufacturerData[20] << 8) | manufacturerData[21];

          // Extraction de l'adresse MAC détectée
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

  if (scanningComplete) {
    if (currentMillis - previousMillisReset >= waitDuration) {
      previousMillisReset = currentMillis;
      scanningComplete = false;
      deviceCount = 0;
      startScan();
    }
  }

  if (currentMillis - previousMillisScan >= scanDuration) {
    previousMillisScan = currentMillis;
    scanningComplete = true;

    // Envoyer chaque appareil individuellement
    for (int i = 0; i < deviceCount; i++) {
      StaticJsonDocument<256> doc;
      doc["idSTRI"] = detectedDevices[i].uuid;
      doc["mac_address_detectee"] = detectedDevices[i].macAddress;
      doc["année"] = detectedDevices[i].major;
      doc["idBadge"] = detectedDevices[i].minor;
      doc["macBLE"] = macBleDuDetecteur;
      doc["macWIFI"] = WiFi.macAddress();

      char jsonBuffer[256];
      serializeJson(doc, jsonBuffer);

      // Publier sur un topic unique pour chaque appareil
      String topic = "detecteur/presence/macwifi/" + detectedDevices[i].macAddress;
      bool success = client.publish(topic.c_str(), jsonBuffer);

      if (success) {
        Serial.println("Message envoyé : " + String(jsonBuffer));
      } else {
        Serial.println("Erreur d'envoi du message.");
      }

      delay(100); // Petite pause pour éviter la surcharge du broker MQTT
    }

    deviceCount = 0;
  }
}
