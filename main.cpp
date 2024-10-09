#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <credentialss.h>
#include <Adafruit_Sensor.h>

// ------------------------------------------------------------------------------------------------------------------------------
// OLED Display instellingen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// SDA en SCL pinnen instellen
#define SDA_PIN 21   
#define SCL_PIN 22  

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

// ------------------------------------------------------------------------------------------------------------------------------
#define ROOD  13    
#define GROEN  14    
#define GEEL  26 

#define Drukknop  12
#define buzzerpin 16

int buttonState = 0;
int lastButtonState = 0; 
bool metingGestart = false; // Controleer of een meting is gestart via de knop
bool tempSent = false;

// ------------------------------------------------------------------------------------------------------------------------------
// BLE configuratie
BLEServer* pServer = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
#define serviceUUID  "c203e7c5-dfc4-46d2-a524-f3c41761a4ea" // Unique service UUID
#define characteristicUUID  "f4f7de75-c2da-4234-93ef-17fcb04d3674" // Unique characteristic UUID
#define LED_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pLedCharacteristic = NULL;

unsigned long lastBLEMillis = 0; // Tijdstempel van de laatste BLE-update
const long BLE_INTERVAL = 3000;  // 3 seconden interval

// ------------------------------------------------------------------------------------------------------------------------------
// TEMPERATUURGEGEVENS:
#define DHTPIN D10
#define DHTTYPE DHT22
unsigned long lastTempMillis = 0;
const long TEMP_INTERVAL = 900000; // 15 minuten meting
float temp;
DHT dht(DHTPIN, DHTTYPE);

// ------------------------------------------------------------------------------------------------------------------------------
// Functie om de BLE-connectiviteit te beheren
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    }

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
        meetTemperatuurEnGeefReactie();
      } else {
      }
    }
  }
};


// ------------------------------------------------------------------------------------------------------------------------------
void setup() {
    pinMode(ROOD, OUTPUT);
    pinMode(GROEN, OUTPUT);
    pinMode(GEEL, OUTPUT);
    pinMode(Drukknop, INPUT);
    pinMode(buzzerpin, OUTPUT);
    Serial.begin(115200);
    dht.begin();
    u8g2.begin();

    // ------------------------------------
    // LEDC initialisatie
    ledcSetup(0, 2000, 8);    // Kanaal 0, frequentie 2000 Hz, resolutie 8 bits
    ledcAttachPin(buzzerpin, 0);    // Koppel de buzzer pin aan kanaal 0

    // ------------------------------------
    // Wi-Fi verbinden
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    digitalWrite(GEEL, HIGH);  // Geel LED aan wanneer verbonden met Wi-Fi

    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // ------------------------------------
    // Create the BLE Device
    BLEDevice::init("ESP32SINS");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(serviceUUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic( // Wijzig hier 'characteristicUUID' naar 'pCharacteristic'
        characteristicUUID,
        BLECharacteristic::PROPERTY_READ   |
        BLECharacteristic::PROPERTY_WRITE  |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );

      // Maak de ON/OFF knop karakteristiek aan
    pLedCharacteristic = pService->createCharacteristic(
        LED_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    // Registreer de callback voor de ON/OFF knop karakteristiek
    pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    // Create a BLE Descriptor
    pCharacteristic->addDescriptor(new BLE2902());
    pLedCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(serviceUUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    Serial.println("BLE gestart");
}

// ------------------------------------------------------------------------------------------------------------------------------
// Functie om zowel LED als buzzer tegelijkertijd te laten werken
void ledEnBuzzer(int ledPin, int buzzerPin, int frequentie, int duur, int ledSnelheid) {
    unsigned long startMillis = millis();
    unsigned long piepMillis = millis(); // Tijdstip van het laatste piepje
    unsigned long ledMillis = millis(); // Tijdstip van de laatste LED wissel
    bool piepAan = false; // Bepalen of de buzzer aan is
    bool ledAan = false; // Bepalen of de LED aan is

    while (millis() - startMillis < duur) {
        // Buzzer piepjes logica
        if (millis() - piepMillis >= 100) { // Wissel elke 100 ms tussen aan en uit voor piepjes
            piepMillis = millis();
            
            if (piepAan) {
                ledcWrite(0, 0); // Zet de buzzer uit
                piepAan = false;
            } else {
                ledcWrite(0, 255); // Zet het volume van de buzzer aan
                ledcWriteTone(0, frequentie); // Speel de toon op de gegeven frequentie
                piepAan = true;
            }
        }

        // LED knipper logica
        if (millis() - ledMillis >= ledSnelheid) { // Wissel op basis van ledSnelheid tussen aan en uit
            ledMillis = millis();
            
            if (ledAan) {
                digitalWrite(ledPin, LOW); // Zet de LED uit
                ledAan = false;
            } else {
                digitalWrite(ledPin, HIGH); // Zet de LED aan
                ledAan = true;
            }
        }
    }
    ledcWrite(0, 0); // Zorg ervoor dat de buzzer uit is aan het einde
    digitalWrite(ledPin, LOW); // Zorg ervoor dat de LED uit is aan het einde
}

// ------------------------------------------------------------------------------------------------------------------------------
// Functie voor temperatuurmeting en LED/buzzer reactie
void meetTemperatuurEnGeefReactie() {
    temp = dht.readTemperature();
    
    // Weergave op OLED-display
    u8g2.clearBuffer();
    
    // Zet een klein lettertype voor "Temperatuur"
    u8g2.setFont(u8g2_font_helvR10_tf);
    u8g2.setCursor(0, 30);
    u8g2.print("Temperatuur:");

    u8g2.setFont(u8g2_font_fub20_tf);
    u8g2.setCursor(0, 62);
    u8g2.print(temp);
    u8g2.print(" C");
    u8g2.sendBuffer();

    // Print temperatuur naar Serial Monitor
    Serial.print("Temperatuur: ");
    Serial.print(temp);
    Serial.println(" °C");

    if (temp > 25) {
        // Rood LED en buzzer samen aan voor 5 seconden als temp. > 25
        ledEnBuzzer(ROOD, buzzerpin, 1000, 5000, 200); 
    } else {
        // Groen LED en buzzer samen aan voor 5 seconden
        ledEnBuzzer(GROEN, buzzerpin, 300, 5000, 200); 
    }
}

void BLE_sturen() {

     // Handle BLE connectiviteit
    if (deviceConnected) {
        // Controleer of er 3 seconden zijn verstreken sinds de laatste BLE-update
        if (millis() - lastBLEMillis > BLE_INTERVAL) {
            // Controleer of de temperatuur al is verzonden
            if (!tempSent) {
                String tempStr = String(temp); // Zet de gemeten temperatuur om in een string
                pCharacteristic->setValue(tempStr.c_str()); // Stuur de temperatuurwaarde naar BLE-characteristic
                pCharacteristic->notify(); // Verstuur de update
                Serial.println("Temperatuur verzonden via BLE: " + tempStr);
                tempSent = true; // Markeer dat de temperatuur is verzonden
            }
            lastBLEMillis = millis(); // Update de tijdstempel van de laatste BLE-update
        }
    }

    // Verwerking van disconnect
    if (!deviceConnected && oldDeviceConnected) {
        Serial.println("Een apparaat is losgekoppeld van BLE");
        delay(500);
        pServer->startAdvertising();
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    // Verwerking van connectie
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("Een apparaat is verbonden via BLE");
    }

  
    
}
// ------------------------------------------------------------------------------------------------------------------------------
void loop() {
    // Temperatuur met LED's en buzzer via timer:
    if (millis() - lastTempMillis >= TEMP_INTERVAL) {   
        lastTempMillis = millis();
        meetTemperatuurEnGeefReactie(); // Voer meting uit
        tempSent = false; // Reset de verzendstatus
    }

    // Controleer op knopdruk om meting direct te starten
    buttonState = digitalRead(Drukknop);
    if (buttonState == HIGH && lastButtonState == LOW) {  
        Serial.println("Knop ingedrukt, meting gestart!");
        meetTemperatuurEnGeefReactie();  // Start meting direct
        lastTempMillis = millis();  
        lastButtonState = HIGH; 
        tempSent = false; 
    } else if (buttonState == LOW) {
        lastButtonState = LOW;  
    }
    BLE_sturen();

}
