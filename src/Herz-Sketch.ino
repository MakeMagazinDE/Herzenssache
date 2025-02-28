/*
  Dateiname:    Herz-Sketch.ino
  Autor:        Kai-Uwe Mrkor
  Datum:        2025-01-09
  Beschreibung: Beispielprogramm zum Empfang der Herzfrequenz von einem BLE-Sensor.  
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_NeoPixel.h>

// UUID der Herzfrequenzdienst und -charakteristik
static BLEUUID serviceUUID("0000180d-0000-1000-8000-00805f9b34fb");  // Herzfrequenzdienst
static BLEUUID charUUID("00002a37-0000-1000-8000-00805f9b34fb");     // Herzfrequenzcharakteristik

static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

#define LED_PIN 14
#define MOTOR_PIN 15
#define LED_COUNT 8
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);

#define SCAN_TIMEOUT 30000       // Zeitlimit für das Scannen in Millisekunden (30 Sekunden)
unsigned long lastScanTime = 0;  // Zeitpunkt des letzten Scans
bool deviceFound = false;        // Status, ob Gerät gefunden wurde

unsigned long lastHeartbeatTime = 0;  // Letzter Herzschlag-Startzeitpunkt
unsigned int heartInterval = 0;       // Zeit zwischen zwei Herzschlägen

// Variablen für die nicht-blockierende Animation
bool isAnimating = false;         // Gibt an, ob eine Animation aktiv ist
unsigned long animationStartTime; // Startzeitpunkt der Animation
const int animationDuration = 300; // Gesamtdauer der Animation in Millisekunden

void startHeartbeatAnimation() {
  isAnimating = true;
  animationStartTime = millis();
  analogWrite(MOTOR_PIN, 127); // Motor auf halbe Leistung setzen
}

void stopHeartbeatAnimation() {
  isAnimating = false;
  analogWrite(MOTOR_PIN, 0); // Motor ausschalten
  strip.clear();             // LEDs ausschalten
  strip.show();
}

void updateHeartbeatAnimation() {
  if (!isAnimating) return;

  unsigned long elapsedTime = millis() - animationStartTime;

  if (elapsedTime < animationDuration / 2) {
    // Animationsteil 1: LEDs heller machen
    int brightness = map(elapsedTime, 0, animationDuration / 2, 0, 255);
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(0, brightness, 0));
    }
    strip.show();
  } else if (elapsedTime < animationDuration) {
    // Animationsteil 2: LEDs dunkler machen
    int brightness = map(elapsedTime, animationDuration / 2, animationDuration, 255, 0);
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(0, brightness, 0));
    }
    strip.show();
  } else {
    stopHeartbeatAnimation(); // Animation beenden
  }
}

// Rückrufmethode für Benachrichtigungen (Herzfrequenzdaten)
void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  if (pRemoteCharacteristic != nullptr && length > 1) {
    uint8_t heartRate = (uint8_t)pData[1]; // Herzfrequenz aus Byte extrahieren
    Serial.print("Herzfrequenz: ");
    Serial.println(heartRate);

    // Berechne das Intervall zwischen zwei Herzschlägen (in Millisekunden)
    heartInterval = 60000 / heartRate;
  } else {
    Serial.println("Ungültige Herzfrequenzdaten empfangen.");
  }
}

// Rückruf für Verbindungsstatus
class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) {
    Serial.println("Verbindung hergestellt.");
    connected = true;
  }

  void onDisconnect(BLEClient* pClient) {
    Serial.println("Verbindung unterbrochen.");
    connected = false;
    deviceFound = false;  // Setzt deviceFound auf false, um den Scan neu zu starten
  }
};

// Verbindung zu BLE-Gerät herstellen
bool connectToServer() {

  Serial.print("Versuche, mit dem Gerät zu verbinden: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallbacks());  // Setze die Rückrufmethode für den Verbindungsstatus
  if (!pClient->connect(myDevice)) {
    Serial.println("Verbindung fehlgeschlagen.");
    return false;
  }
  Serial.println("Verbunden.");

  // Herzfrequenzdienst abrufen
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Herzfrequenzdienst nicht gefunden.");
    pClient->disconnect();
    return false;
  }

  // Herzfrequenzcharakteristik abrufen
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Herzfrequenzcharakteristik nicht gefunden.");
    pClient->disconnect();
    return false;
  }

  // Benachrichtigungen aktivieren
  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  return true;
}

// Klasse für das Scannen nach BLE-Geräten
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Gefundenes Gerät: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      deviceFound = true;  // Gerät gefunden, Status auf true setzen
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Herz wird initialisiert");

  // Jewel einrichten
  strip.begin();            // NeoPixel-Strip initialisieren
  strip.show();             // Alle Pixel ausschalten
  strip.setBrightness(50);  // Helligkeit setzen
  strip.clear();
  strip.show();

  // Motor-PIN als Ausgang konfigurieren
  pinMode(MOTOR_PIN, OUTPUT);

  // BLE initialisieren
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1000); // Intervall 1 Sekunde (1000 ms)
  pBLEScan->setWindow(300);    // Fenster 300 ms
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);  // Kurzzeitiges Scannen (5 Sekunden)
  delay(500);                 // Kleine Verzögerung, um dem System Zeit zum Starten zu geben.
  lastScanTime = millis();    // Zeit des letzten Scans speichern
}


unsigned long letztePeriode = 0;  

void loop() {
  // Überprüfen, ob ein passendes Gerät gefunden wurde
  if (doConnect && !connected) {
    if (connectToServer()) {
      Serial.println("Verbindung erfolgreich.");
      deviceFound = true;  // Gerät erfolgreich gefunden
    } else {
      Serial.println("Verbindung fehlgeschlagen, versuche erneut...");
    }
    doConnect = false;
  }

  // Wenn kein Gerät gefunden wurde und die Zeit abgelaufen ist, mittlere LED auf Rot setzen
  if (!deviceFound && millis() - lastScanTime >= SCAN_TIMEOUT) {
    strip.clear();
    strip.setPixelColor(0, strip.Color(255, 0, 0));
    strip.show();
    lastScanTime = millis();  // Zeit zurücksetzen, um LEDs weiterhin anzuzeigen
  }

  // Wenn verbunden, Herzfrequenzdaten auswerten
  if (connected && pRemoteCharacteristic != nullptr) {
    if (millis() - lastHeartbeatTime >= heartInterval) {
      startHeartbeatAnimation();
      Serial.print(millis()-letztePeriode);
      Serial.println(" ms");      
      letztePeriode = millis();
      lastHeartbeatTime += heartInterval;  // Periodendauer der aktuellen Herzfrequenz aufsummieren
    }
  }

  // Animation aktualisieren
  updateHeartbeatAnimation();

  // Wenn nicht verbunden, BLE-Scan neu starten
  if (!connected) {
    Serial.println("Verbindung verloren. Starte erneuten Scan.");
    strip.clear();
    strip.setPixelColor(0, strip.Color(255, 0, 0));
    strip.show();
    BLEDevice::getScan()->start(5, false);  // Starte den Scan neu für 5 Sekunden
    lastScanTime = millis();                // Reset des Scan-Zeitpunkts
  }

  delay(1);
}
