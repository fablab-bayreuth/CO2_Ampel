#include <FastLED.h>
#include <MHZ19.h>
#include <SoftwareSerial.h>              // Remove if using HardwareSerial or Arduino package without SoftwareSerial support

// Comlink to MHZ19 sensor
#define MHZ19_RX_PIN     D1               // Rx pin which the MHZ19 Tx pin is attached to
#define MHZ19_TX_PIN     D2               // Tx pin which the MHZ19 Rx pin is attached to
#define MHZ19_BAUDRATE   9600            // Device to MH-Z19 Serial MHZ19_BAUDRATE (should not be changed)

// LED strip
#define LED_NUM           8     // Number of LEDs 

//=============================================================================

MHZ19 myMHZ19;                          // CO2 sensor
SoftwareSerial mySerial(MHZ19_RX_PIN, MHZ19_TX_PIN);                   // create device to MH-Z19 serial

CRGB led_data[LED_NUM];               // LED strip data

#define FRAME_SIZE 10 /*Frame size of BayEOS-Frame in buffer */

//Deklaration von notwendigen Funktionen
void sendCO2();
void sendBuffer(int num = -1);


void handleLED(void) {
  int cont_range = 0;
  int cont_i = 0;
  if (cfg.ampel_mode == 1) {
    cont_range = cfg.high - cfg.low - 100;
    cont_i = cont_range / 8;
  }
  // LED traffic light display
  for (size_t i = 0; i < LED_NUM; i++) {
    if (device.co2_current < (cfg.low + (cont_i * i - cont_range / 2)) ) led_data[i] = CRGB::Green;
    else if (device.co2_current > (cfg.high + (cont_i * i - cont_range / 2))) led_data[i] = CRGB::Red;
    else led_data[i] = CRGB::DarkOrange;
    if (! device.time_is_set && (i == 1 || i == 6)) led_data[i] = CRGB::Black;
    if (cfg.ampel_mode == 2) {
      if (i == 2 || i == 5) led_data[i] = CRGB::Black;
      else if (device.co2_current > cfg.high && i < 6) led_data[i] = CRGB::Black;
      else if (device.co2_current < cfg.low && i > 2) led_data[i] = CRGB::Black;
      else if ((device.co2_current >= cfg.low && device.co2_current <= cfg.high) && (i < 2 || i > 5)) led_data[i] = CRGB::Black;
    }
    if (device.led_off || device.led_blink) led_data[i] = CRGB::Black;
  }
  if (device.co2_current <= cfg.blink) device.led_blink = false;

  FastLED.show();

}

void handleSensor(void)
{
  device.lastCO2 = millis();
  // Measurements
  if (device.co2_count >= cfg.samplingavr) {
    device.co2_sum -= device.co2[device.co2_index];
    device.co2_count--;
  }
  int co2, temp;
  co2 = myMHZ19.getCO2();                 // CO2 concentration (in ppm)
  device.co2[device.co2_index] = co2;
  device.co2_single = device.co2[device.co2_index];
  Serial.print("co2: ");      Serial.print(device.co2[device.co2_index]);      Serial.print("  ");
  device.co2_sum += device.co2[device.co2_index];
  device.co2_count++;
  device.co2_index++;
  if (device.co2_index >= cfg.samplingavr) device.co2_index = 0;
  device.co2_current = device.co2_sum / device.co2_count;
  sendCO2();

  temp = myMHZ19.getTemperature();     // Temperature (in degrees Celsius)
  Serial.print("temp: ");     Serial.print(temp);     Serial.println();

  handleLED();

  //Save Data
  if ((millis() - device.lastData) > (1000 * cfg.loggingint)) {
    device.lastData = millis();
    client.startDataFrame(BayEOS_Int16le);
    client.addChannelValue(device.co2_current);
#if SEND_RAW_DATA == 0
    client.writeToBuffer();
#endif
    //Save data to SPIFFS-Buffer
    if (device.time_is_set) {
      client.setBuffer(FSBuffer);
      client.writeToBuffer();
      client.setBuffer(myBuffer);
      sendBuffer(); //sends buffer information to client
    }

  }
  //Try to send to BayEOS-Gateway
  if (cfg.mode && cfg.bayeos && cfg.bayeos_gateway[0] && cfg.bayeos_name[0]) {
    client.sendMultiFromBuffer(1000);
  }
#if SEND_RAW_DATA
  client.startDataFrame(BayEOS_Int16le);
  client.addChannelValue(device.co2_current);
  client.addChannelValue(co2);
  client.addChannelValue(temp);
  client.writeToBuffer();
#endif
}
