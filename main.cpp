
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
      int receivedValue = value[0] - '0';
      if (receivedValue == 2) {
        myServo.write(65); // Open flap to 65 degrees
        Serial.println("Flap opened to 65°");
      } else if (receivedValue == 1) {
        myServo.write(0);  // Close flap to 0 degrees
        Serial.println("Flap closed to 0°");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // Set up servo
  myServo.attach(servoPin);
  myServo.write(0); // Start with the flap closed

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
    myServo.write(65); // Open the flap
    delay(15000); // Keep the flap open for 15 seconds
    myServo.write(0);  // Close the flap
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
