#if defined(__AVR__) // Защита: компилировать только для плат AVR (Uno, Mega, Nano)

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SenseAir_S88.h"

SoftwareSerial s8Serial(10, 11); 
SenseAir_S88 co2Sensor;
uint32_t lastRequestTime = 0;
const uint32_t REQUEST_INTERVAL_MS = 5000;

void onCO2DataReady(uint16_t co2) {
    Serial.print("[SUCCESS] CO2 Level: ");
    Serial.print(co2);
    Serial.println(" ppm");
}

void onErrorOccurred(SenseAir_S88::ErrorCode error, SenseAir_S88::CommandType failedCmd) {
    Serial.print("[ERROR] Command dropped. Type: ");
    Serial.print(failedCmd == SenseAir_S88::CommandType::READ_CO2 ? "READ_CO2" : "SET_ABC");
    Serial.print(" | Error Code: ");
    Serial.println(static_cast<int>(error));
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {;} 
    s8Serial.begin(9600);
    
    if (!co2Sensor.begin(&s8Serial)) {
        Serial.println("[CRITICAL] Init failed");
        while (true) {;} 
    }

    co2Sensor.setCallback(onCO2DataReady);
    co2Sensor.setErrorCallback(onErrorOccurred);
}

void loop() {
    co2Sensor.update();
    if (millis() - lastRequestTime >= REQUEST_INTERVAL_MS) {
        lastRequestTime = millis();
        co2Sensor.requestCO2();
    }
}

#else
// Если пользователь выбрал другую плату (ESP32/ESP8266), компилируем пустой скетч
#pragma message("This example is specifically designed for AVR boards (Uno, Mega) using SoftwareSerial.")
void setup() {}
void loop() {}
#endif