#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

const int ledPin = 26; // Pas dit aan naar je GPIO pin
bool deviceConnected = false;

// BLE Service en Characteristic UUIDs
#define SERVICE_UUID "064fd7fc-23b7-4518-840d-d5a475223b3b"
#define LED_CHARACTERISTIC_UUID "e97eeab2-0d47-4ce3-8590-959073d5b2c2"

// Callback class om te reageren op schrijfacties
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pLedCharacteristic) {
    std::string ledvalue = pLedCharacteristic->getValue();
    Serial.print("Received value: ");
    Serial.println(ledvalue.c_str());

    if (ledvalue == "ON") {
      digitalWrite(ledPin, HIGH);  // Zet LED aan
      Serial.println("LED ON");
    } else if (ledvalue == "OFF") {
      digitalWrite(ledPin, LOW);   // Zet LED uit
      Serial.println("LED OFF");
    } else {
      Serial.println("Unknown command received");
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device Disconnected");
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Zorg dat de LED uit is in het begin

  BLEDevice::init("ESP32SINS");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pLedCharacteristic = pService->createCharacteristic(
      LED_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );

  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE Server is now advertising.");
}

void loop() {
  // Voeg hier debug-informatie toe of andere functionaliteiten
  if (deviceConnected) {
    Serial.println("Device is connected.");
  } else {
    Serial.println("Device is not connected.");
  }
  delay(1000); // Vermijd overbelasting van de serial output
}
