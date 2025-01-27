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
const char* mqttServer = "x.x.x.x";
const int mqttPort = 1883;

// Identifiants MQTT
const char* mqttUsername = "admin";
const char* mqttPassword = "xhc9QmmISs";

// Initialisation des objets Wi-Fi et MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Configuration BLE
BLEScan* pBLEScan;
bool isScanning = false;

// Tableau pour stocker les couples UUID, MAC, Major et Minor déjà envoyés
const int maxEntries = 50;
String sentData[maxEntries];
int sentCount = 0;

// Adresse MAC BLE du détecteur
String macBleDuDetecteur = "";

// Constante ID du porte
const int x = 1; // Remplacez par l'ID du porte souhaité

// Classe personnalisée pour gérer les périphériques BLE détectés
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String manufacturerData = advertisedDevice.getManufacturerData();

      if (manufacturerData.length() >= 20) {
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

          // Construire un couple UUID:MAC:Major:Minor pour identification unique
          String dataPair = uuid + ":" + macAddress + ":" + String(major) + ":" + String(minor);

          bool alreadySent = false;
          for (int i = 0; i < sentCount; i++) {
            if (sentData[i] == dataPair) {
              alreadySent = true;
              break;
            }
          }

          if (!alreadySent) {
            if (client.connected()) {
              // Construction des topics dynamiques
              String topicMacWifi = String("/detecteur/porte/macwifi/") + WiFi.macAddress();
              String topicIdDetecteur = String("/detecteur/porte/iddetecteur/") + String(x);

              // Création des messages JSON
              StaticJsonDocument<256> doc;
              doc["idSTRI"] = uuid;
              doc["année"] = major;
              doc["idBadge"] = minor;
              doc["mac_address_detectee"] = macAddress; // Ajout de la MAC détectée
              doc["macBLE"] = macBleDuDetecteur;
              doc["macWIFI"] = WiFi.macAddress();


              char jsonBuffer[512];
              serializeJson(doc, jsonBuffer);

              // Publication sur le topic MAC Wi-Fi
              client.publish(topicMacWifi.c_str(), jsonBuffer);
              Serial.println("Message envoyé au topic : " + topicMacWifi);
              Serial.println("Payload : " + String(jsonBuffer));

              // Publication sur le topic ID détecteur
              client.publish(topicIdDetecteur.c_str(), jsonBuffer);
              Serial.println("Message envoyé au topic : " + topicIdDetecteur);
              Serial.println("Payload : " + String(jsonBuffer));

              if (sentCount < maxEntries) {
                sentData[sentCount++] = dataPair;
              } else {
                for (int i = 1; i < maxEntries; i++) {
                  sentData[i - 1] = sentData[i];
                }
                sentData[maxEntries - 1] = dataPair;
              }
            }
          } else {
            Serial.println("Couple déjà envoyé, ignoré.");
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

// Fonction pour redémarrer le scan BLE
void restartScan() {
  pBLEScan->start(2, false);
  isScanning = true;
  Serial.println("Scan BLE relancé.");
}

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
  pBLEScan->start(2, false);
  isScanning = true;
}

// Boucle principale
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  if (isScanning) {
    pBLEScan->stop();
    isScanning = false;
  }
  restartScan();
  delay(2000);
}
