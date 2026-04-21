#include "SenseAir_S88.h"

SenseAir_S88::SenseAir_S88() : _serial(nullptr), _currentState(State::S8_STATE_IDLE), 
                               _requestTime(0),
                               _onCmdSuccess(nullptr),
                               _onDataReady(nullptr), _onError(nullptr),
                               _queueHead(0), _queueTail(0),
                               _rxIndex(0), _expectedLength(0), 
                               _targetABCHours(0), _consecutiveErrors(0) {
    _latestData.co2Ppm = 0;
    _latestData.status = ErrorCode::S8_ERR_NULL_POINTER;

    // Initialize command queue
    for (uint8_t i = 0; i < QUEUE_SIZE; i++) {
        _commandQueue[i].type = CommandType::S8_CMD_READ_CO2;
        _commandQueue[i].param = 0;
    }
    // Initialize RX buffer
    for (uint8_t i = 0; i < RX_BUFFER_SIZE; i++) {
        _rxBuffer[i] = 0;
    }
}

bool SenseAir_S88::begin(Stream* stream) {
    // Strict pointer validation
    if (stream == nullptr) {
        _latestData.status = ErrorCode::S8_ERR_NULL_POINTER;
        return false;
    }
    _serial = stream;
    _currentState = State::S8_STATE_IDLE;
    _consecutiveErrors = 0;
    return true;
}

void SenseAir_S88::setCommandSuccessCallback(CommandSuccessCallback callback) {
    _onCmdSuccess = callback;
}

void SenseAir_S88::setCallback(DataReadyCallback callback) {
    _onDataReady = callback;
}

void SenseAir_S88::setErrorCallback(ErrorCallback callback) {
    _onError = callback;
}

bool SenseAir_S88::enqueueCommand(CommandType cmdType, uint16_t param) {
    uint8_t nextHead = (_queueHead + 1) % QUEUE_SIZE;
    if (nextHead == _queueTail) return false; // Queue is full
    
    _commandQueue[_queueHead].type = cmdType;
    _commandQueue[_queueHead].param = param;
    _queueHead = nextHead;
    return true;
}

bool SenseAir_S88::dequeueCommand(QueueItem& item) {
    if (isQueueEmpty()) return false;
    item = _commandQueue[_queueTail];
    _queueTail = (_queueTail + 1) % QUEUE_SIZE;
    return true;
}

bool SenseAir_S88::isQueueEmpty() const {
    return _queueHead == _queueTail;
}

void SenseAir_S88::flushQueue(bool keepConfigCommands) {
    if (!keepConfigCommands) {
        // Drop all pending commands
        while (!isQueueEmpty()) {
            QueueItem item;
            dequeueCommand(item);
            if (_onError != nullptr) _onError(ErrorCode::S8_ERR_COMMAND_DROPPED, item.type);
        }
    } else {
        // Retain only configuration commands (like SET_ABC)
        uint8_t count = (_queueHead >= _queueTail) ? (_queueHead - _queueTail) : (QUEUE_SIZE - _queueTail + _queueHead);
        uint8_t tempTail = _queueTail;
        
        QueueItem tempQueue[QUEUE_SIZE];
        uint8_t tempHead = 0;

        for (uint8_t i = 0; i < count; i++) {
            QueueItem item = _commandQueue[tempTail];
            if (item.type == CommandType::S8_CMD_SET_ABC) {
                tempQueue[tempHead++] = item; 
            } else {
                if (_onError != nullptr) _onError(ErrorCode::S8_ERR_COMMAND_DROPPED, item.type); 
            }
            tempTail = (tempTail + 1) % QUEUE_SIZE;
        }

        _queueTail = 0;
        _queueHead = tempHead;
        for (uint8_t i = 0; i < tempHead; i++) {
            _commandQueue[i] = tempQueue[i];
        }
    }
    _consecutiveErrors = 0; 
}

SenseAir_S88::ErrorCode SenseAir_S88::requestCO2() {
    if (_serial == nullptr) return ErrorCode::S8_ERR_NULL_POINTER;
    if (!enqueueCommand(CommandType::S8_CMD_READ_CO2)) return ErrorCode::S8_ERR_QUEUE_FULL;
    return ErrorCode::S8_ERR_OK;
}

SenseAir_S88::ErrorCode SenseAir_S88::setABCPeriod(uint16_t hours) {
    if (_serial == nullptr) return ErrorCode::S8_ERR_NULL_POINTER;
    
    // Validate input: Max period is 180 hours. 0 is valid (Disables ABC).
    if (hours > 180) return ErrorCode::S8_ERR_PARAM_OUT_OF_BOUNDS;
    
    if (!enqueueCommand(CommandType::S8_CMD_SET_ABC, hours)) return ErrorCode::S8_ERR_QUEUE_FULL;
    return ErrorCode::S8_ERR_OK;
}

SenseAir_S88::SensorData SenseAir_S88::getLatestData() const {
    return _latestData;
}

void SenseAir_S88::clearRxBuffer() {
    for(uint8_t i = 0; i < RX_BUFFER_SIZE; i++) _rxBuffer[i] = 0;
    _rxIndex = 0;
}

void SenseAir_S88::sendModbusCommand(const uint8_t* cmd, uint8_t len) {
    if (_serial == nullptr || cmd == nullptr || len == 0) return;
    clearRxBuffer();
    _expectedLength = 0;
    _serial->write(cmd, len);
    _requestTime = millis();
}

void SenseAir_S88::resetState(ErrorCode err) {
    if (err != ErrorCode::S8_ERR_OK) {
        _latestData.status = err;
        _consecutiveErrors++;
        
        // Trigger self-healing flush if too many consecutive errors occur
        if (_consecutiveErrors >= MAX_RETRIES) {
            flushQueue(true); 
        }
    } else {
        _consecutiveErrors = 0; 
    }
    
    _currentState = State::S8_STATE_IDLE;
    _rxIndex = 0;
    _expectedLength = 0;
}

bool SenseAir_S88::validateModbusCRC(uint8_t length) {
    if (length < 4 || length > RX_BUFFER_SIZE) return false;
    uint16_t calculatedCRC = 0xFFFF; 
    for (uint8_t pos = 0; pos < length - 2; pos++) {
        calculatedCRC ^= (uint16_t)_rxBuffer[pos]; 
        for (uint8_t i = 8; i != 0; i--) { 
            if ((calculatedCRC & 0x0001) != 0) {
                calculatedCRC >>= 1;
                calculatedCRC ^= 0xA001;
            } else {
                calculatedCRC >>= 1;
            }
        }
    }
    uint16_t receivedCRC = (uint16_t)_rxBuffer[length - 2] | ((uint16_t)_rxBuffer[length - 1] << 8);
    return calculatedCRC == receivedCRC;
}

bool SenseAir_S88::update() {
    if (_serial == nullptr) return false;

    // Check for pending commands in IDLE state
    if (_currentState == State::S8_STATE_IDLE && !isQueueEmpty()) {
        QueueItem nextCmd;
        if (dequeueCommand(nextCmd)) {
            if (nextCmd.type == CommandType::S8_CMD_READ_CO2) {
                const uint8_t command[TX_BUFFER_SIZE] = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};
                sendModbusCommand(command, TX_BUFFER_SIZE);
                _currentState = State::S8_STATE_WAITING_CO2_RESPONSE;
            } 
            else if (nextCmd.type == CommandType::S8_CMD_SET_ABC) {
                _targetABCHours = nextCmd.param;
                // Command to READ current ABC period from Holding Register 0x001F
                const uint8_t command[TX_BUFFER_SIZE] = {0xFE, 0x03, 0x00, 0x1F, 0x00, 0x01, 0xA1, 0xC3};
                sendModbusCommand(command, TX_BUFFER_SIZE);
                _currentState = State::S8_STATE_WAITING_ABC_READ_RESPONSE;
            }
        }
    }

    // Process asynchronous timeouts and incoming data
    if (_currentState == State::S8_STATE_WAITING_CO2_RESPONSE || 
        _currentState == State::S8_STATE_WAITING_ABC_READ_RESPONSE || 
        _currentState == State::S8_STATE_WAITING_ABC_WRITE_RESPONSE) {
        
        if (millis() - _requestTime > TIMEOUT_MS) {
            resetState(ErrorCode::S8_ERR_TIMEOUT);
            return false;
        }

        while (_serial->available() > 0 && _rxIndex < RX_BUFFER_SIZE) {
            _rxBuffer[_rxIndex++] = _serial->read();

            // Determine expected payload length dynamically based on Modbus response type
            if (_expectedLength == 0 && _rxIndex >= 3) {
                if (_rxBuffer[1] & 0x80) _expectedLength = 5; // Modbus Exception Frame
                else _expectedLength = 3 + _rxBuffer[2] + 2;  // Standard Data Frame

                if (_expectedLength > RX_BUFFER_SIZE) {
                    resetState(ErrorCode::S8_ERR_BUFFER_OVERFLOW);
                    return false;
                }
            }
        }

        if (_expectedLength > 0 && _rxIndex >= _expectedLength) {
            if (_currentState == State::S8_STATE_WAITING_CO2_RESPONSE) _currentState = State::S8_STATE_PROCESSING_CO2;
            else if (_currentState == State::S8_STATE_WAITING_ABC_READ_RESPONSE) _currentState = State::S8_STATE_PROCESSING_ABC_READ;
            else if (_currentState == State::S8_STATE_WAITING_ABC_WRITE_RESPONSE) _currentState = State::S8_STATE_PROCESSING_ABC_WRITE; 
        }
    }

    // Process complete Modbus frames based on current state
    switch (_currentState) {
        case State::S8_STATE_PROCESSING_CO2:
            if (validateModbusCRC(_expectedLength) && !(_rxBuffer[1] & 0x80)) {
                uint16_t co2 = (_rxBuffer[3] << 8) | _rxBuffer[4];
                // Validate sensor physics boundaries
                if (co2 >= MIN_CO2 && co2 <= MAX_CO2) {
                    _latestData.co2Ppm = co2;
                    _latestData.status = ErrorCode::S8_ERR_OK;
                    if (_onDataReady != nullptr) _onDataReady(co2);
                    resetState(ErrorCode::S8_ERR_OK);
                    return true;
                } else {
                    resetState(ErrorCode::S8_ERR_INVALID_DATA);
                }
            } else {
                resetState(ErrorCode::S8_ERR_CRC_MISMATCH);
            }
            break;

        case State::S8_STATE_PROCESSING_ABC_READ:
            if (validateModbusCRC(_expectedLength) && !(_rxBuffer[1] & 0x80)) {
                uint16_t currentABC = (_rxBuffer[3] << 8) | _rxBuffer[4];
                
                // Only write to EEPROM if the value actually changed (Read-Modify-Write pattern)
                if (currentABC != _targetABCHours) {
                    uint8_t writeCmd[TX_BUFFER_SIZE] = {0xFE, 0x06, 0x00, 0x1F, 
                                                        (uint8_t)(_targetABCHours >> 8), 
                                                        (uint8_t)(_targetABCHours & 0xFF), 0, 0};
                    
                    uint16_t crc = 0xFFFF;
                    for (uint8_t pos = 0; pos < 6; pos++) {
                        crc ^= (uint16_t)writeCmd[pos];
                        for (uint8_t i = 8; i != 0; i--) {
                            if ((crc & 0x0001) != 0) {
                                crc >>= 1; crc ^= 0xA001;
                            } else crc >>= 1;
                        }
                    }
                    writeCmd[6] = crc & 0xFF; 
                    writeCmd[7] = (crc >> 8) & 0xFF; 

                    sendModbusCommand(writeCmd, TX_BUFFER_SIZE);
                    _currentState = State::S8_STATE_WAITING_ABC_WRITE_RESPONSE;
                } else {
                    // Value is already correct, fire success callback and finish
                    if (_onCmdSuccess != nullptr) _onCmdSuccess(CommandType::S8_CMD_SET_ABC);
                    resetState(ErrorCode::S8_ERR_OK);
                }
            } else {
                resetState(ErrorCode::S8_ERR_CRC_MISMATCH);
            }
            break;

        case State::S8_STATE_PROCESSING_ABC_WRITE:
            if (validateModbusCRC(_expectedLength) && !(_rxBuffer[1] & 0x80)) {
                if (_onCmdSuccess != nullptr) {
                    _onCmdSuccess(CommandType::S8_CMD_SET_ABC);
                }
                resetState(ErrorCode::S8_ERR_OK);
            } else {
                resetState(ErrorCode::S8_ERR_CRC_MISMATCH);
            }
            break;

        case State::S8_STATE_ERROR:
            resetState(ErrorCode::S8_ERR_OK); 
            break;
            
        default:
            break;
    }
    return false;
}