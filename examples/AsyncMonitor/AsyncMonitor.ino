#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SenseAir_S88.h"

// Ручные прототипы, чтобы обойти баг компилятора Arduino
void onCO2DataReady(uint16_t co2);
void onErrorOccurred(SenseAir_S88::ErrorCode error, SenseAir_S88::CommandType failedCmd);

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
    Serial.print(failedCmd == SenseAir_S88::CommandType::S8_CMD_READ_CO2 ? "READ_CO2" : "SET_ABC");
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
// Пустой скетч для плат (типа ESP32), где нет SoftwareSerial
void setup() {}
void loop() {}
#endif