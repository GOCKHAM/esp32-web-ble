#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
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

// TEMPERATUURGEGEVENS:
#define DHTPIN D10
#define DHTTYPE DHT22
unsigned long lastTempMillis = 0;
const long TEMP_INTERVAL = 900000; // 15 minuten meting
float temp;
DHT dht(DHTPIN, DHTTYPE);

// Web server
WebServer server(80);

// Function prototypes
void handleRoot();
void handleTemperature();
void meetTemperatuur();

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

  // Webserver endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/temperature", HTTP_GET, handleTemperature);
  server.begin();
}

// ------------------------------------------------------------------------------------------------------------------------------
void handleRoot() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", "<html><body><h1>Welcome to the ESP32 Web Server</h1></body></html>");
}

void handleTemperature() {
  float currentTemp = dht.readTemperature();
  String response = String("Temperature: ") + currentTemp + " °C";
  server.send(200, "text/plain", response);
}

// ------------------------------------------------------------------------------------------------------------------------------
void meetTemperatuur() {
  temp = dht.readTemperature();
  
  // Print temperatuur naar Serial Monitor
  Serial.print("Temperatuur: ");
  Serial.print(temp);
  Serial.println(" °C");
}

// ------------------------------------------------------------------------------------------------------------------------------
void loop() {
  // Handle client requests
  server.handleClient();

  // TEMPERATUUR MET LEDJES EN BUZZER CODE VIA TIMER:
  if (millis() - lastTempMillis >= TEMP_INTERVAL) {   // Meet elke 15 minuten
    lastTempMillis = millis();
    meetTemperatuur(); // Voer meting uit
  }

  // Controleer op knopdruk om meting direct te starten
  buttonState = digitalRead(Drukknop);
  if (buttonState == HIGH && lastButtonState == LOW) {  
    Serial.println("Knop ingedrukt, meting gestart!");
    meetTemperatuur();  // Start meting direct
    lastTempMillis = millis();  
    lastButtonState = HIGH;  
  } else if (buttonState == LOW) {
    lastButtonState = LOW;  
  }
}
