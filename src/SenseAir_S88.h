#ifndef SENSEAIR_S88_H
#define SENSEAIR_S88_H

#include <Arduino.h>

class SenseAir_S88 {
public:
    // Типы поддерживаемых команд для управления очередью
    enum class CommandType { 
        READ_CO2, 
        SET_ABC 
    };

    // Коды состояний для обработки ошибок и уведомлений
    enum class ErrorCode {
        OK = 0,
        ERR_NULL_POINTER,        // Отсутствует указатель на объект (например, Serial)
        ERR_TIMEOUT,             // Превышено время ожидания ответа
        ERR_CRC_MISMATCH,        // Нарушена целостность пакета (Modbus CRC)
        ERR_INVALID_DATA,        // Физически невозможные данные (например, CO2 < 400 ppm)
        ERR_BUSY,                // Конечный автомат занят
        ERR_PARAM_OUT_OF_BOUNDS, // Недопустимый аргумент (например, период ABC вне диапазона)
        ERR_BUFFER_OVERFLOW,     // Пакет превысил размеры внутреннего буфера
        ERR_QUEUE_FULL,          // Кольцевой буфер команд заполнен
        ERR_COMMAND_DROPPED      // Команда была удалена системой самовосстановления
    };

    // Сигнатуры пользовательских коллбэков
    typedef void (*DataReadyCallback)(uint16_t co2);
    typedef void (*ErrorCallback)(ErrorCode error, CommandType failedCmd);
    typedef void (*CommandSuccessCallback)(CommandType cmd);
    

    // Структура для возврата результата по запросу
    struct SensorData {
        uint16_t co2Ppm;
        ErrorCode status;
    };

    SenseAir_S88();

    // Обязательная инициализация с проверкой указателя
    bool begin(Stream* stream);
    
    // Главный асинхронный обработчик. Вызывать в loop()
    bool update();
    
    // Асинхронные методы (добавляют задачу в очередь)
    ErrorCode requestCO2();
    ErrorCode setABCPeriod(uint16_t hours);
    
    // Геттер последних валидных данных
    SensorData getLatestData() const;
    
    // Назначение коллбэков
    void setCallback(DataReadyCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setCommandSuccessCallback(CommandSuccessCallback callback); 

    // Принудительная очистка очереди с возможностью сохранения важных команд
    void flushQueue(bool keepConfigCommands = true); 

private:
    // Внутренние состояния конечного автомата
    enum class State {
        IDLE,
        WAITING_CO2_RESPONSE,
        PROCESSING_CO2,
        WAITING_ABC_READ_RESPONSE,
        PROCESSING_ABC_READ,
        WAITING_ABC_WRITE_RESPONSE,
        PROCESSING_ABC_WRITE,
        ERROR
    };

    // Элемент очереди команд
    struct QueueItem {
        CommandType type;
        uint16_t param;
    };

    Stream* _serial;
    State _currentState;
    uint32_t _requestTime;
    SensorData _latestData;
    
    CommandSuccessCallback _onCmdSuccess;
    
    DataReadyCallback _onDataReady;
    ErrorCallback _onError; 
    
    // Кольцевой буфер (Ring Buffer)
    static const uint8_t QUEUE_SIZE = 4;
    QueueItem _commandQueue[QUEUE_SIZE];
    uint8_t _queueHead;
    uint8_t _queueTail;

    // Внутренние буферы UART
    static const uint8_t TX_BUFFER_SIZE = 8;
    static const uint8_t RX_BUFFER_SIZE = 16;
    uint8_t _rxBuffer[RX_BUFFER_SIZE];
    uint8_t _rxIndex;
    uint8_t _expectedLength;

    uint16_t _targetABCHours;

    // Константы ограничений и логики
    static const uint32_t TIMEOUT_MS = 1000;
    static const uint16_t MIN_CO2 = 400;
    static const uint16_t MAX_CO2 = 10000;
    static const uint8_t MAX_RETRIES = 3;
    uint8_t _consecutiveErrors;

    // Скрытые служебные методы
    bool enqueueCommand(CommandType cmdType, uint16_t param = 0);
    bool dequeueCommand(QueueItem& item);
    bool isQueueEmpty() const;

    void clearRxBuffer();
    bool validateModbusCRC(uint8_t length);
    void resetState(ErrorCode err);
    void sendModbusCommand(const uint8_t* cmd, uint8_t len);
};

#endif // SENSEAIR_S88_H