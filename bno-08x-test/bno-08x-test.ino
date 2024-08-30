#include <Wire.h>
#include "driver/adc.h"
#include "esp_log.h"

#define BAT_ADC    ADC1_CHANNEL_0  // GPIO 36 on most boards (check your board's pinout)
float Voltage = 0.0;

#include "SparkFun_BNO080_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_BNO080
BNO080 myIMU;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 10;

String response;

void setup() {
    Serial.begin(115200);

    delay(100); //  Wait for BNO to boot
    // Start i2c and BNO080
    Wire.flush();   // Reset I2C
    Wire.begin(9, 8);

    myIMU.begin(0x4B, Wire); // 0x4A, 0x4B

    if (myIMU.begin() == false) {
        Serial.println("BNO080 not detected at default I2C address. Check your jumpers and the hookup guide. Freezing...");
        while (1);
    }

    Wire.setClock(400000); //Increase I2C data rate to 400kHz

    myIMU.enableRotationVector(50); //Send data update every 50ms

    Serial.println(F("Rotation vector enabled"));
    Serial.println(F("Output in form i, j, k, real, accuracy"));
}

void loop() {
    if (myIMU.dataAvailable() == true) {
        float quatI = myIMU.getQuatI();
        float quatJ = myIMU.getQuatJ();
        float quatK = myIMU.getQuatK();
        float quatReal = myIMU.getQuatReal();
        float quatRadianAccuracy = myIMU.getQuatRadianAccuracy();

        Serial.print(quatI, 2);
        Serial.print(F(" "));
        Serial.print(quatK, 2);
        Serial.print(F(" "));
        Serial.print(quatJ, 2);
        Serial.print(F(" "));
        Serial.print(quatReal, 2);

        Voltage = readADC() * 2;
        Serial.printf(" %.2fV\n", Voltage / 1000.0);
    }
}

float readADC() {
    // Configure ADC width and attenuation
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BAT_ADC, ADC_ATTEN_DB_11);

    // Read the raw ADC value
    int adc_raw = adc1_get_raw(BAT_ADC);

    // Convert raw ADC value to voltage in millivolts
    float voltage = (float)adc_raw * 1100 / 4095;

    return voltage;
}
