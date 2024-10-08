#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pLedCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

static const int servoPin = D2; // Verander naar de juiste pin voor jouw setup
Servo servo1;

#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pLedCharacteristic) {
    std::string ledvalue = pLedCharacteristic->getValue(); 
    String value = String(ledvalue.c_str());
    if (value.length() >= 0) {
      Serial.print("Characteristic event, written: ");
      Serial.println(static_cast<int>(value[0])); // Print de integer waarde

      int receivedValue = static_cast<int>(value[0]);
      if (receivedValue == 1) {
        servo1.write(65); // Servo naar open positie (65 graden)
        delay(20); // Kleine vertraging voor servo om te bewegen
      } else {
        servo1.write(0); // Servo naar gesloten positie (0 graden)
        delay(20); // Kleine vertraging voor servo om te bewegen
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  servo1.attach(servoPin); // Koppel servo aan pin

  // Maak het BLE apparaat aan
  BLEDevice::init("ESP32SINSE");

  // Maak de BLE server aan
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Maak de BLE service aan
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Maak de ON/OFF knop karakteristiek aan
  pLedCharacteristic = pService->createCharacteristic(
                      LED_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  // Registreer de callback voor de ON/OFF knop karakteristiek
  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Voeg een BLE descriptor toe voor de karakteristiek
  pLedCharacteristic->addDescriptor(new BLE2902());

  // Start de service
  pService->start();

  // Start adverteren
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // Stel in op 0x00 om deze parameter niet te adverteren
  BLEDevice::startAdvertising();
  Serial.println("Wacht op een clientverbinding...");
}

void loop() {
  // Disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Apparaat gedisconnect.");
    delay(500); // Geef de Bluetooth-stack tijd om zich voor te bereiden
    pServer->startAdvertising(); // Herstart adverteren
    Serial.println("Start adverteren");
    oldDeviceConnected = deviceConnected;
  }
  // Connecting
  if (deviceConnected && !oldDeviceConnected) {
    // Voer hier acties uit bij het verbinden
    oldDeviceConnected = deviceConnected;
    Serial.println("Apparaat verbonden");
  }
}
