#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>  // Use the ESP32-specific servo library

Servo myServo;

BLEServer* pServer = NULL;
BLECharacteristic* pServoCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

const int servoPin = 12; // Pin where the servo is connected
int deurOpenHoek = 65;  // Open position at 65°
int deurSluitHoek = 0;  // Closed position at 0°
int doorState = deurSluitHoek;  // Initial state is closed
bool doorIsOpen = false;

#define SERVICE_UUID "42fd51eb-e931-43c5-b222-3fec95abc662"
#define SERVO_CHARACTERISTIC_UUID "28fa84b6-1d36-45c8-94b8-46876735e94f"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 0) {
      int receivedValue = value[0] - '0'; // Get first character (either '1' or '2')

      if (receivedValue == 2 && doorState == deurSluitHoek) {
        myServo.write(deurOpenHoek); // Open the door
        doorState = deurOpenHoek;  // Update state to open
        doorIsOpen = true;
        Serial.println("Deur geopend!");
      } else if (receivedValue == 1 && doorState == deurOpenHoek) {
        myServo.write(deurSluitHoek); // Close the door
        doorState = deurSluitHoek;  // Update state to closed
        doorIsOpen = false;
        Serial.println("Deur gesloten!");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // Set up servo
  myServo.attach(servoPin);
  myServo.write(deurSluitHoek); // Start with the door closed

  // Create BLE device
  BLEDevice::init("ESP32SINS");

  // Create BLE server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE characteristic for servo control
  pServoCharacteristic = pService->createCharacteristic(
    SERVO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pServoCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("Waiting for client connection...");
}

void loop() {
  // Auto-opening flap every 15 minutes (900000 milliseconds)
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 900000) {
    if (doorState == deurSluitHoek) {
      myServo.write(deurOpenHoek);  // Automatically open the door
      doorState = deurOpenHoek;
      doorIsOpen = true;
      Serial.println("Deur automatisch geopend!");
      delay(15000);  // Keep the door open for 15 seconds
      myServo.write(deurSluitHoek);  // Automatically close the door
      doorState = deurSluitHoek;
      doorIsOpen = false;
      Serial.println("Deur automatisch gesloten!");
    }
    lastTime = millis();
  }

  // Check for Bluetooth connections
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE to reset
    pServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}
