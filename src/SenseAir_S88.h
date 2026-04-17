#ifndef SENSEAIR_S88_H
#define SENSEAIR_S88_H

#include <Arduino.h>
#include <Stream.h>

class SenseAir_S88 {
public:
    // Префиксы S8_CMD_ защищают от конфликтов с системными макросами
    enum class CommandType { 
        S8_CMD_READ_CO2, 
        S8_CMD_SET_ABC 
    };

    enum class ErrorCode {
        S8_ERR_OK = 0,
        S8_ERR_NULL_POINTER,
        S8_ERR_TIMEOUT,
        S8_ERR_CRC_MISMATCH,
        S8_ERR_INVALID_DATA,
        S8_ERR_BUSY,
        S8_ERR_PARAM_OUT_OF_BOUNDS,
        S8_ERR_BUFFER_OVERFLOW,
        S8_ERR_QUEUE_FULL,
        S8_ERR_COMMAND_DROPPED
    };

    typedef void (*DataReadyCallback)(uint16_t co2);
    typedef void (*ErrorCallback)(ErrorCode error, CommandType failedCmd);
    typedef void (*CommandSuccessCallback)(CommandType cmd);

    struct SensorData {
        uint16_t co2Ppm;
        ErrorCode status;
    };

    SenseAir_S88();

    bool begin(Stream* stream);
    bool update();
    
    ErrorCode requestCO2();
    ErrorCode setABCPeriod(uint16_t hours);
    
    SensorData getLatestData() const;
    
    void setCallback(DataReadyCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setCommandSuccessCallback(CommandSuccessCallback callback); 

    void flushQueue(bool keepConfigCommands = true); 

private:
    // Префиксы S8_STATE_ защищают от AVR макросов (например, IDLE)
    enum class State {
        S8_STATE_IDLE,
        S8_STATE_WAITING_CO2_RESPONSE,
        S8_STATE_PROCESSING_CO2,
        S8_STATE_WAITING_ABC_READ_RESPONSE,
        S8_STATE_PROCESSING_ABC_READ,
        S8_STATE_WAITING_ABC_WRITE_RESPONSE,
        S8_STATE_PROCESSING_ABC_WRITE,
        S8_STATE_ERROR
    };

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
    
    static const uint8_t QUEUE_SIZE = 4;
    QueueItem _commandQueue[QUEUE_SIZE];
    uint8_t _queueHead;
    uint8_t _queueTail;

    static const uint8_t TX_BUFFER_SIZE = 8;
    static const uint8_t RX_BUFFER_SIZE = 16;
    uint8_t _rxBuffer[RX_BUFFER_SIZE];
    uint8_t _rxIndex;
    uint8_t _expectedLength;

    uint16_t _targetABCHours;

    static const uint32_t TIMEOUT_MS = 1000;
    static const uint16_t MIN_CO2 = 400;
    static const uint16_t MAX_CO2 = 10000;
    static const uint8_t MAX_RETRIES = 3;
    uint8_t _consecutiveErrors;

    bool enqueueCommand(CommandType cmdType, uint16_t param = 0);
    bool dequeueCommand(QueueItem& item);
    bool isQueueEmpty() const;

    void clearRxBuffer();
    bool validateModbusCRC(uint8_t length);
    void resetState(ErrorCode err);
    void sendModbusCommand(const uint8_t* cmd, uint8_t len);
};

#endif // SENSEAIR_S88_H 