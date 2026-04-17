#include <Arduino.h>
#include "SenseAir_S88.h"

// ESP32 имеет 3 аппаратных порта. Serial(0) используется для логов.
// Мы используем Serial2 для датчика.
// Обязательно указываем пины: RX2 = 16, TX2 = 17.
#define S8_RX_PIN 16
#define S8_TX_PIN 17

SenseAir_S88 co2Sensor;
uint32_t lastRequestTime = 0;
const uint32_t REQUEST_INTERVAL_MS = 5000;

// Коллбэк для успешных данных
void onCO2DataReady(uint16_t co2) {
    Serial.printf("[SUCCESS] CO2: %d ppm\n", co2);
}

// Коллбэк для ошибок
void onErrorOccurred(SenseAir_S88::ErrorCode error, SenseAir_S88::CommandType failedCmd) {
    Serial.printf("[ERROR] Command %d failed with code %d\n", static_cast<int>(failedCmd), static_cast<int>(error));
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    // Инициализация аппаратного порта с указанием пинов (только для ESP32)
    Serial2.begin(9600, SERIAL_8N1, S8_RX_PIN, S8_TX_PIN);

    // Строгая проверка инициализации
    if (!co2Sensor.begin(&Serial2)) {
        Serial.println("[CRITICAL] Failed to initialize hardware serial pointer!");
        while (true) { delay(100); }
    }

    co2Sensor.setCallback(onCO2DataReady);
    co2Sensor.setErrorCallback(onErrorOccurred);
    Serial.println("ESP32 HardwareSerial Initialized.");
}

void loop() {
    co2Sensor.update();

    if (millis() - lastRequestTime >= REQUEST_INTERVAL_MS) {
        lastRequestTime = millis();
        co2Sensor.requestCO2();
    }
}