/**
 * @file AsyncCalibration.ino
 * @brief Demonstration of asynchronous background calibration for the SenseAir S88 sensor.
 * * This example shows how to perform a 400 ppm background calibration without 
 * blocking the main loop. A blinking LED demonstrates that the microcontroller 
 * continues to execute other tasks while the 30-second calibration process 
 * runs in the background.
 * * HARDWARE SETUP:
 * - Connect the SenseAir S88 sensor to your board's hardware or software serial.
 * - The built-in LED will be used to show non-blocking execution.
 */

#include <Arduino.h>
// Include your main library header here. 
// Assuming the class we built is named ClimateControl or similar.
#include "ClimateControl.h" 

ClimateControl climate;

// Variables for non-blocking LED blink
unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 250; // 250 ms blink rate
bool ledState = false;

void setup() {
    Serial.begin(115200);

    // Some boards (like ESP32 variants) might not have LED_BUILTIN defined.
    // If you get a compilation error here, replace LED_BUILTIN with your LED pin number.
    #ifdef LED_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
    #endif

    // Initialize your sensor/class here
    // climate.begin();

    // Give serial monitor time to open
    delay(2000); 

    // --- FOOLPROOF PROTECTION WARNING ---
    Serial.println(F("\n=================================================="));
    Serial.println(F("      WARNING: ASYNCHRONOUS CO2 CALIBRATION       "));
    Serial.println(F("=================================================="));
    Serial.println(F("Calibration will set the current air baseline to 400 ppm."));
    Serial.println(F("If you do this indoors, all future readings will be wrong!"));
    Serial.println(F(""));
    Serial.println(F("INSTRUCTIONS:"));
    Serial.println(F("1. Move the sensor outdoors to fresh air."));
    Serial.println(F("2. Let it run for at least 15-20 minutes to stabilize."));
    Serial.println(F("3. Type exactly 'CAL' in this Serial Monitor and press Enter."));
    Serial.println(F("==================================================\n"));
}

void loop() {
    // 1. STATE MACHINE UPDATER (Crucial for async operation)
    // This must be called constantly. It takes almost zero CPU time 
    // unless a calibration step is actively changing.
    climate.updateCalibration();

    // 2. NON-BLOCKING LED BLINK
    // This proves that the MCU is not frozen during the 30-second calibration.
    if (millis() - lastBlinkTime >= blinkInterval) {
        lastBlinkTime = millis();
        ledState = !ledState;
        
        #ifdef LED_BUILTIN
            digitalWrite(LED_BUILTIN, ledState);
        #endif
    }

    // 3. SAFE USER INPUT HANDLING
    // Wait for the user to explicitly type "CAL" to prevent accidental triggers.
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim(); // Remove any invisible characters (like \r or spaces)

        if (input == "CAL") {
            Serial.println(F("Command received. Attempting to start calibration..."));
            // Start calibration. The 'false' parameter prevents force-calibration 
            // if initial CO2 is unsafely high (>1000 ppm).
            bool success = climate.startCalibration(false);
            
            if (!success) {
                Serial.println(F("Calibration failed to start. Check warnings above."));
            }
        } else if (input.length() > 0) {
            Serial.print(F("Unknown command: '"));
            Serial.print(input);
            Serial.println(F("'. Type 'CAL' to calibrate."));
        }
    }

    // Other non-blocking tasks can go here (e.g., WiFi handling, MQTT polling)
}