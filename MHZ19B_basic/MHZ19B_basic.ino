/*
AD: basic functionality for CO2 sensor MH-Z19B
*/

//=============================================================================

#include <Arduino.h>
#include <stdint.h>
#include "HardwareSerial.h"
#include "FastLED.h"
#include "MHZ19.h"

//=============================================================================

// Comlink to MHZ19 sensor 
#define MHZ19_RX_PIN     16               // Rx pin which the MHZ19 Tx pin is attached to
#define MHZ19_TX_PIN     17               // Tx pin which the MHZ19 Rx pin is attached to
#define MHZ19_BAUDRATE   9600             // Device to MH-Z19 Serial MHZ19_BAUDRATE (should not be changed)

// CO2 traffic light thresholds (ppm)
#define CO2_THRESHOLD_RED     1400     // Above this threshold --> Red
#define CO2_THRESHOLD_YELLOW  1000     // Above this threshold --> yellow

// LED strip
#define LED_DATA_PIN      13     // LED strip Din
#define LED_NUM           8      // Number of LEDs 
#define LED_BRIGHTNESS    96     // define LED LED_BRIGHTNESS

//=============================================================================

MHZ19 myMHZ19;                          // CO2 sensor
HardwareSerial mySerial2(2);            // Comlink for CO2 sensor

CRGB led_data[LED_NUM];               // LED strip data

//=============================================================================

// CO2 data storage (used for gliding average filter)
#define CO2_DATA_MAX  60
uint16_t co2_data[CO2_DATA_MAX] = {0};
size_t co2_data_index = 0;  // Next put position
size_t co2_data_len = 0;    // Stored data points

//=============================================================================

void setup() 
{
    // PC comlink init
    Serial.begin(9600);

    // CO2 sensor init
    mySerial2.begin(MHZ19_BAUDRATE, SERIAL_8N1, MHZ19_RX_PIN, MHZ19_TX_PIN);
    myMHZ19.begin(mySerial2);
    myMHZ19.autoCalibration(true);

    // WS2812B LED strip init
    FastLED.addLeds< NEOPIXEL, LED_DATA_PIN >(led_data, LED_NUM);
    FastLED.setBrightness(LED_BRIGHTNESS);
}

//=============================================================================

void loop() 
{   
    co2_task();
    delay(1000);    // Wait 1 second
}

//=============================================================================

void co2_task(void)
{
    // Measurements
    uint16_t co2 = myMHZ19.getCO2();             // CO2 concentration (in ppm);
    uint16_t temp = myMHZ19.getTemperature();    // Temperature (in degrees Celsius)

    // CO2 Filter
    // TODO: Gliding average should be replaced by a proper filter function.
    co2_data[co2_data_index] = co2;
    if (++co2_data_index >= CO2_DATA_MAX)
        co2_data_index = 0;
    if (co2_data_len < CO2_DATA_MAX)
        co2_data_len++;

    uint32_t co2_avg = 0;
    for (size_t i=0; i<co2_data_len; i++)
        co2_avg += co2_data[i];
    co2_avg /= co2_data_len;

    Serial.print("co2: ");      Serial.print(co2);      Serial.print("  ");
    Serial.print("co2_avg: ");  Serial.print(co2_avg);  Serial.print("  ");
    Serial.print("temp: ");     Serial.print(temp);     Serial.println();

    // LED traffic light display 
    CRGB co2_color = CRGB::Green;
    if (co2_avg > CO2_THRESHOLD_RED)
        co2_color = CRGB::Red;
    else if (co2_avg > CO2_THRESHOLD_YELLOW)
        co2_color = CRGB::Yellow;

    for (size_t i=0; i<LED_NUM; i++)
        led_data[i] = co2_color;
    FastLED.show();   
}

//=============================================================================
