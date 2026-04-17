#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SenseAir_S88.h"

// Инициализация программного UART для связи с датчиком (RX: 10, TX: 11)
SoftwareSerial s8Serial(10, 11); 

// Экземпляр класса датчика
SenseAir_S88 co2Sensor;

// Таймер для неблокирующего опроса
uint32_t lastRequestTime = 0;
const uint32_t REQUEST_INTERVAL_MS = 5000; // Опрашивать каждые 5 секунд

// --- КОЛЛБЭКИ ---

// Вызывается автоматически при успешном получении и валидации данных
void onCO2DataReady(uint16_t co2) {
    Serial.print("[SUCCESS] CO2 Level: ");
    Serial.print(co2);
    Serial.println(" ppm");
}

// Вызывается автоматически при удалении команды из-за сбоя связи
void onErrorOccurred(SenseAir_S88::ErrorCode error, SenseAir_S88::CommandType failedCmd) {
    Serial.print("[ERROR] Command dropped. Type: ");
    Serial.print(failedCmd == SenseAir_S88::CommandType::READ_CO2 ? "READ_CO2" : "SET_ABC");
    Serial.print(" | Error Code: ");
    Serial.println(static_cast<int>(error));
}

void setup() {
    // Инициализация аппаратного порта для вывода в консоль
    Serial.begin(115200);
    while (!Serial) {;} // Ожидание подключения консоли
    
    // Инициализация связи с датчиком (9600 бод по умолчанию для S8)
    s8Serial.begin(9600);
    
    // Передача интерфейса связи в библиотеку с проверкой успешности
    if (!co2Sensor.begin(&s8Serial)) {
        Serial.println("[CRITICAL] Failed to initialize sensor pointer!");
        while (true) { delay(10); } // Остановка системы при критическом сбое
    }

    // Назначение функций обратного вызова
    co2Sensor.setCallback(onCO2DataReady);
    co2Sensor.setErrorCallback(onErrorOccurred);

    Serial.println("System initialized. Starting async monitoring...");
}

void loop() {
    // 1. Асинхронный "движок" библиотеки. Должен вызываться максимально часто.
    co2Sensor.update();

    // 2. Неблокирующий таймер для отправки запросов
    if (millis() - lastRequestTime >= REQUEST_INTERVAL_MS) {
        lastRequestTime = millis();
        
        // Добавление команды в очередь
        SenseAir_S88::ErrorCode status = co2Sensor.requestCO2();
        
        if (status == SenseAir_S88::ErrorCode::ERR_QUEUE_FULL) {
            Serial.println("[WARNING] Command queue is full!");
        }
    }
    
    // Здесь может выполняться любой другой код (Wi-Fi, дисплей, другие датчики).
    // Функция delay() не используется.
}