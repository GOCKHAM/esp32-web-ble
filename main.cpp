#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

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

// ------------------------------------------------------------------------------------------------------------------------------
// BLE configuratie
BLEServer* pServer = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
const char* serviceUUID = "c203e7c5-dfc4-46d2-a524-f3c41761a4ea"; // Unique service UUID
const char* characteristicUUID = "f4f7de75-c2da-4234-93ef-17fcb04d3674"; // Unique characteristic UUID
BLECharacteristic* pCharacteristic = NULL;

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
    // BLE initialisatie
    BLEDevice::init("ESP32SINS"); // Naam van je BLE-apparaat
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(serviceUUID);
    pCharacteristic = pService->createCharacteristic(
        characteristicUUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pService->start();
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
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

// ------------------------------------------------------------------------------------------------------------------------------
void loop() {
    // TEMPERATUUR MET LEDJES EN BUZZER CODE VIA TIMER:
    if (millis() - lastTempMillis >= TEMP_INTERVAL) {   // Meet elke 15 minuten
        lastTempMillis = millis();
        meetTemperatuurEnGeefReactie(); // Voer meting uit
    }

    // Controleer op knopdruk om meting direct te starten
    buttonState = digitalRead(Drukknop);
    if (buttonState == HIGH && lastButtonState == LOW) {  
        Serial.println("Knop ingedrukt, meting gestart!");
        meetTemperatuurEnGeefReactie();  // Start meting direct
        lastTempMillis = millis();  
        lastButtonState = HIGH;  
    } else if (buttonState == LOW) {
        lastButtonState = LOW;  
    }

    // Handle BLE connectiviteit
    if (deviceConnected) {
        // Update de BLE-kenmerken, als dat nodig is
        String bleState = deviceConnected ? "Connected" : "Disconnected";
        pCharacteristic->setValue(bleState.c_str());
        pCharacteristic->notify(); // Notificeer verbonden apparaten
    }

    // Houd bij of de verbinding is gewijzigd
    if (deviceConnected != oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        if (deviceConnected) {
            Serial.println("Een apparaat is verbonden via BLE");
        } else {
            Serial.println("Een apparaat is losgekoppeld van BLE");
        }
    }
}
