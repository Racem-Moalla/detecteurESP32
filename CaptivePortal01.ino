#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Configurations Wi-Fi
const char* ssid = "Livebox-0B10";              // Nom du réseau Wi-Fi
const char* password = "wmEwMuqRprGD6M4A9T";    // Mot de passe Wi-Fi

// Adresse IP du broker MQTT
const char* mqttServer = "192.168.1.18";        // Adresse IP du broker MQTT
const int mqttPort = 1883;                      // Port du broker MQTT

// Nom du topic MQTT
const char* mqttTopic = "esp32/data";           // Nom du topic MQTT pour publier les données BLE

// Initialisation des objets Wi-Fi et MQTT
WiFiClient espClient;                           // Client Wi-Fi
PubSubClient client(espClient);                 // Client MQTT

// Configuration BLE
BLEScan* pBLEScan;                              // Objet pour gérer les scans BLE
bool isScanning = false;                        // Indicateur pour savoir si un scan BLE est en cours

// Tableau pour stocker les couples UUID, MAC, Major et Minor déjà envoyés
const int maxEntries = 50;                      // Taille maximale du tableau
String sentData[maxEntries];                    // Tableau pour stocker les données envoyées
int sentCount = 0;                              // Compteur des entrées enregistrées

// Classe personnalisée pour gérer les périphériques BLE détectés
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Vérifier si des données fabricant sont disponibles dans l'appareil détecté
    if (advertisedDevice.haveManufacturerData()) {
      String manufacturerData = advertisedDevice.getManufacturerData();

      // Vérifier si la longueur des données fabricant est suffisante
      if (manufacturerData.length() >= 20) {
        Serial.println("Longueur des données fabricant : " + String(manufacturerData.length()));

        // Extraction de l'UUID (16 octets à partir de l'index 2)
        String uuid = "";
        for (int i = 2; i < 18; i++) {
          uuid += String(manufacturerData[i] >> 4, HEX);   // Ajouter la partie haute de l'octet en hexadécimal
          uuid += String(manufacturerData[i] & 0x0F, HEX); // Ajouter la partie basse de l'octet en hexadécimal
          if (i == 5 || i == 7 || i == 9 || i == 11) {
            uuid += "-"; // Ajouter un séparateur pour respecter le format UUID
          }
        }
        uuid.toUpperCase(); // Convertir l'UUID en majuscules

        // Vérifier si l'UUID correspond à celui que nous cherchons
        if (uuid == "2D7A9F0C-E0E8-4CC9-A71B-A21DB2D034A1") {
          Serial.println("UUID extrait : " + uuid);

          // Extraire le champ Major (octets 16-17)
          uint16_t major = (manufacturerData[18] << 8) | manufacturerData[19];

          // Extraire le champ Minor (octets 18-19)
          uint16_t minor = (manufacturerData[20] << 8) | manufacturerData[21];

          // Extraire l'adresse MAC (octets 22-27)
          String macAddress = "";
          for (int i = 22; i < 28; i++) {
            macAddress += String(manufacturerData[i] >> 4, HEX);   // Ajouter la partie haute en hexadécimal
            macAddress += String(manufacturerData[i] & 0x0F, HEX); // Ajouter la partie basse en hexadécimal
            if (i < 27) macAddress += ":";                        // Ajouter un séparateur ':'
          }
          macAddress.toUpperCase(); // Convertir l'adresse MAC en majuscules
          Serial.println("Adresse MAC extraite : " + macAddress);

          // Construire un couple UUID:MAC:Major:Minor pour identification unique
          String dataPair = uuid + ":" + macAddress + ":" + String(major) + ":" + String(minor);
          Serial.println("Couple généré : " + dataPair);

          // Vérifier si ce couple a déjà été envoyé
          bool alreadySent = false;
          for (int i = 0; i < sentCount; i++) {
            if (sentData[i] == dataPair) {
              alreadySent = true;
              break;
            }
          }

          // Si le couple n'a pas été envoyé, l'envoyer via MQTT
          if (!alreadySent) {
            if (client.connected()) {
              String message = "UUID: " + uuid + ", MAC: " + macAddress + 
                               ", Major: " + String(major) + ", Minor: " + String(minor);
              client.publish(mqttTopic, message.c_str()); // Publier le message sur le broker MQTT
              Serial.println("Message envoyé: " + message);

              // Ajouter le couple au tableau des données envoyées
              if (sentCount < maxEntries) {
                sentData[sentCount++] = dataPair;
              } else {
                // Si le tableau est plein, faire un décalage pour libérer de l'espace
                for (int i = 1; i < maxEntries; i++) {
                  sentData[i-1] = sentData[i];
                }
                sentData[maxEntries-1] = dataPair;
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

// Fonction pour se connecter au Wi-Fi
void connectWiFi() {
  Serial.println("Connexion au Wi-Fi...");
  WiFi.begin(ssid, password); // Lancer la connexion au réseau Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connecté!");
}

// Fonction pour se connecter au broker MQTT
void connectMQTT() {
  client.setServer(mqttServer, mqttPort); // Configurer l'adresse du broker et le port
  while (!client.connected()) {
    Serial.println("Connexion au broker MQTT...");
    if (client.connect("ESP32Client")) { // Se connecter avec un nom de client unique
      Serial.println("Connecté au broker MQTT!");
    } else {
      Serial.print("Echec, rc=");
      Serial.println(client.state());
      delay(2000); // Attendre avant de réessayer
    }
  }
}

// Fonction pour redémarrer le scan BLE
void restartScan() {
  if (!isScanning) {
    pBLEScan->start(2, false); // Lancer un scan BLE de 2 secondes
    isScanning = true;
    Serial.println("Scan relancé...");
  }
}

// Fonction d'initialisation
void setup() {
  Serial.begin(115200); // Initialiser la communication série

  // Connexion au Wi-Fi et au broker MQTT
  connectWiFi();
  connectMQTT();

  // Initialisation BLE
  BLEDevice::init(""); // Initialiser le périphérique BLE
  pBLEScan = BLEDevice::getScan(); // Créer un objet de scan BLE
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // Définir les callbacks
  pBLEScan->setActiveScan(true); // Activer le mode scan actif
  Serial.println("Initialisation du scan BLE...");
  pBLEScan->start(2, false); // Démarrer un scan initial
  isScanning = true;
}

// Boucle principale
void loop() {
  // Reconnexion au Wi-Fi si nécessaire
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Reconnexion au broker MQTT si nécessaire
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop(); // Maintenir la connexion MQTT active

  // Gestion du scan BLE
  if (isScanning) {
    pBLEScan->stop(); // Arrêter le scan actuel
    isScanning = false;
  }
  restartScan(); // Relancer un nouveau scan
  delay(2000); // Délai pour réduire la charge CPU
}
