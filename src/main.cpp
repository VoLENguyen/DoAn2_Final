#include "Adafruit_SGP30.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "DHT.h"
#include "MQ135.h"
#include <Arduino.h>
#include "SharpGP2Y10.h"
#include <WiFi.h>
#include <FirebaseESP32.h>
// macro biến
// tạo timer senđata
const unsigned long updateInterval = 3000;  // 5 seconds
// Last time the update function was called
unsigned long lastUpdateTime = 0;
// set nguong
long variables[3] = { 1000, 1000, 1000 };
// DHT-11
#define DHT_Pin 5
#define DHT_Type DHT11
// dust sensor
#define voPin 36
#define ledPin 14
// ky040
const int CLK_PIN = 16;  // KY-040 CLK pin
const int DT_PIN = 27;   // KY-040 DT pin
const int SW_PIN = 17;   // KY-040 SW pin
volatile bool aState;
volatile bool bState;
volatile bool buttonState;
volatile bool lastButtonState = HIGH;
volatile long encoderValue = 0;
int selectedThreshSensor = 0;
// MQ135
#define MQ135_pin 39
// oled
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET 4         // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // 0x3D for 128x64, 0x3C for 128x32
// ssid and passwork
#define WIFI_SSID "Volenguyen"
#define WIFI_PASSWORD "1502200226052002"
// firebase
#define FIREBASE_HOST "https://air-minotoring-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "QtGIWEUT92pXJVmWrtPEclYAF6jZw3KA9DIUXdRv"
//  call obj
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_Pin, DHT_Type);
Adafruit_SGP30 sgp;
MQ135 mq135 = MQ135(MQ135_pin);
SharpGP2Y10 dustSensor(voPin, ledPin);
FirebaseData fbdo;
// function

uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature));  // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity);                                                                 // [mg/m^3]
  return absoluteHumidityScaled;
}
void SGPSenSor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  sgp.setHumidity(getAbsoluteHumidity(t, h));
  if (!sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    Firebase.setString(fbdo, "/Air_Sensor", "Measurement failed");
    display.clearDisplay();
    display.setCursor(0, 5);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.println("Measurement failed");
    display.display();
    display.clearDisplay();
    return;
  }
  Serial.print("TVOC ");
  Serial.print(sgp.TVOC);
  Serial.print(" ppb\t");
  Serial.print("eCO2 ");
  Serial.print(sgp.eCO2);
  Serial.println(" ppm");
  if (Firebase.setDouble(fbdo, "/Air_Sensor/TVOC", sgp.TVOC))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");
  if (Firebase.setDouble(fbdo, "/Air_Sensor/eCO2", sgp.eCO2))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");
  // TVOC
  display.clearDisplay();
  display.setCursor(0, 0);
  //display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.print("TVOC:");
  display.print(sgp.TVOC);
  display.setTextSize(0.5);
  display.print(" ppb");
  // eCO2
  display.setCursor(0, 10);  //oled display
  //display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.print("CO2:");
  display.print(sgp.eCO2);
  display.setTextSize(0.5);
  display.print(" ppm");
  display.display();
  if (sgp.TVOC > variables[1]) {
    Serial.println("vuot nguong");
  } else {
    Serial.println("binh thuong");
  }
}
void ReadMQ135() {
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read DHT");
    Firebase.setString(fbdo, "/MQ135/", "Failed to read DHT");
    return;
  }
  //Lấy hệ số được tính từ nhiệt độ và độ ẩm
  float Correctrzero = mq135.getCorrectedRZero(temp, hum);
  //Lấy khoảng cách tối đa mà cảm biến phát hiện ra.
  float resistance = mq135.getResistance();
  float correctedPPM = mq135.getCorrectedPPM(temp, hum);

  Serial.print("\t Corrected RZero: ");
  Serial.print(Correctrzero);
  Serial.print("\t Resistance: ");
  Serial.print(resistance);
  Serial.print("\t Corrected PPM: ");
  Serial.print(correctedPPM);
  Serial.println("ppm");

  if (Firebase.setFloat(fbdo, "/MQ135", correctedPPM))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");

  display.setCursor(0, 20);  //oled display
  display.setTextColor(SH110X_WHITE);
  display.print("Air:");
  display.print(correctedPPM);
  display.setTextSize(0.5);
  display.print(" ppm");
  display.display();
}
void ReadDHT11() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read DHT");
    return;
  }
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  if (Firebase.setFloat(fbdo, "/DHT11/nhietdo", t))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");
  if (Firebase.setFloat(fbdo, "/DHT11/doam", h))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");
  display.setCursor(0, 30);
  display.print("Temperature: ");
  //display.setCursor(60,30);
  display.print(t);
  display.print(" ");
  display.cp437(true);
  display.write(167);
  display.print("C");
  display.setCursor(0, 40);
  display.print("Humidity: ");
  display.setCursor(62, 40);
  display.print(h);
  display.print(" %");
  display.display();
}
void DustSenSor() {
  float dustDensity = dustSensor.getDustDensity();
  if (Firebase.setDouble(fbdo, "/DustSenSor", dustDensity))
    Serial.println("Upload success");
  else
    Serial.println("Upload fail");
  display.setCursor(0, 50);  //oled display
  display.setTextColor(SH110X_WHITE);
  display.print("Bui:");
  display.print(dustDensity);
  display.setTextSize(0.5);
  display.print(" ppm");
  display.display();
}
void handleVariableChange(long &variable) {
  noInterrupts();
  long val = encoderValue;
  interrupts();

  if (val != 0) {
    variable += (val > 0) ? 1 : -1;
    Serial.print("Variable ");
    Serial.print(selectedThreshSensor + 1);
    Serial.print(": ");
    Serial.println(variables[selectedThreshSensor]);

    noInterrupts();
    encoderValue = 0;
    interrupts();
  }

  if (buttonState != lastButtonState) {
    if (!buttonState) {
      selectedThreshSensor++;
      if (selectedThreshSensor >= 3) {
        selectedThreshSensor = 0;
      }
      Serial.print("Selected Sensor: ");
      Serial.println(selectedThreshSensor + 1);
    }
    lastButtonState = buttonState;
  }
}
void handleEncoder();
void handleButton();
void updateFirebaseData() {
  SGPSenSor();
  ReadMQ135();
  ReadDHT11();
  DustSenSor();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SW_PIN), handleButton, CHANGE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(fbdo, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(fbdo, "tiny");
  dht.begin();
  Wire.begin();
  display.begin(SCREEN_ADDRESS, true);
  display.display();
  if (!sgp.begin()) {
    Serial.println("Sensor not found :(");
    Firebase.setString(fbdo, "/Air_Sensor", "Sensor not found :(");
    display.setCursor(0, 5);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.println("Sensor not found :(");
    display.display();
    display.clearDisplay();
  }
  lastUpdateTime = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentTime;  // Update lastUpdateTime
    updateFirebaseData();          // Call the function to update Firebase data
  }

  handleVariableChange(variables[selectedThreshSensor]);  // Handle encoder and button actions
}
void handleEncoder() {
  aState = digitalRead(CLK_PIN);
  bState = digitalRead(DT_PIN);

  if (aState == bState) {
    encoderValue++;
  } else {
    encoderValue--;
  }
}

void handleButton() {
  buttonState = digitalRead(SW_PIN);
}