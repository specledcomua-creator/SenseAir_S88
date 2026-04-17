#include <Arduino.h>
#include <HardwareSerial.h>
#include <LittleFS.h>
#include "SenseAir_S88.h"

#define S8_RX_PIN 16
#define S8_TX_PIN 17
#define BACKUP_FILE "/abc_backup.txt"

SenseAir_S88 co2Sensor;
uint32_t lastRequestTime = 0;

void saveBackup(uint16_t hours) {
    File file = LittleFS.open(BACKUP_FILE, "w");
    if (!file) return;
    file.println(hours);
    file.close();
    Serial.println("[BACKUP] Config saved to file.");
}

void checkAndRestoreBackup() {
    if (LittleFS.exists(BACKUP_FILE)) {
        File file = LittleFS.open(BACKUP_FILE, "r");
        if (file) {
            String data = file.readStringUntil('\n');
            uint16_t hours = data.toInt();
            file.close();
            
            if (hours > 0) {
                Serial.printf("[RESTORE] Restoring ABC: %d hours. File kept until success.\n", hours);
                co2Sensor.setABCPeriod(hours); 
            }
        }
    }
}

// --- КОЛЛБЭКИ ---

void onCO2DataReady(uint16_t co2) {
    Serial.printf("CO2: %d ppm\n", co2);
}

void onErrorOccurred(SenseAir_S88::ErrorCode error, SenseAir_S88::CommandType failedCmd) {
    if (failedCmd == SenseAir_S88::CommandType::SET_ABC && error == SenseAir_S88::ErrorCode::ERR_COMMAND_DROPPED) {
        Serial.println("[CRITICAL] ABC Config dropped! Writing to backup file...");
        saveBackup(180); 
    }
}

// НОВОЕ: Обработчик успешного завершения команды
void onCommandSuccess(SenseAir_S88::CommandType cmd) {
    if (cmd == SenseAir_S88::CommandType::SET_ABC) {
        Serial.println("[SUCCESS] Sensor confirmed ABC config!");
        // Удаляем бекап ТОЛЬКО после физического подтверждения от датчика
        if (LittleFS.exists(BACKUP_FILE)) {
            LittleFS.remove(BACKUP_FILE);
            Serial.println("[BACKUP] Backup file deleted.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, S8_RX_PIN, S8_TX_PIN);

    if (!LittleFS.begin(true)) {
        while (true) { delay(100); }
    }

    if (!co2Sensor.begin(&Serial2)) {
        while (true) { delay(100); }
    }

    co2Sensor.setCallback(onCO2DataReady);
    co2Sensor.setErrorCallback(onErrorOccurred);
    co2Sensor.setCommandSuccessCallback(onCommandSuccess); // Регистрация нового коллбэка

    checkAndRestoreBackup();
    co2Sensor.setABCPeriod(180);
}

void loop() {
    co2Sensor.update();

    if (millis() - lastRequestTime >= 5000) {
        lastRequestTime = millis();
        co2Sensor.requestCO2();
    }
}