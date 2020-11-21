#include <FastLED.h>
#include <MHZ19.h> 
#include <SoftwareSerial.h>              // Remove if using HardwareSerial or Arduino package without SoftwareSerial support

// Comlink to MHZ19 sensor
#define MHZ19_RX_PIN     5               // Rx pin which the MHZ19 Tx pin is attached to
#define MHZ19_TX_PIN     4               // Tx pin which the MHZ19 Rx pin is attached to
#define MHZ19_BAUDRATE   9600            // Device to MH-Z19 Serial MHZ19_BAUDRATE (should not be changed)

// LED strip
#define LED_NUM           8     // Number of LEDs 
#define LED_BRIGHTNESS    96    // define LED LED_BRIGHTNESS

//=============================================================================

MHZ19 myMHZ19;                          // CO2 sensor
SoftwareSerial mySerial(MHZ19_RX_PIN, MHZ19_TX_PIN);                   // create device to MH-Z19 serial

CRGB led_data[LED_NUM];               // LED strip data


//Deklaration von notwendigen Funktionen
void sendCO2();
void sendFrames(bool full = false,int num=0);

void handleLED(void){
	  // LED traffic light display
	  CRGB co2_color = CRGB::DarkOrange;

	  if (device.co2_current > cfg.high)
	    co2_color = CRGB::Red;
	  else if (device.co2_current < cfg.low)
	    co2_color = CRGB::Green;

    if(device.co2_current<=cfg.blink) device.led_blink=false;
    
	  if(device.led_off || device.led_blink) co2_color = CRGB::Black;

	  for (size_t i = 0; i < LED_NUM; i++)
	    led_data[i] = co2_color;
	  FastLED.show();

}

void handleSensor(void)
{
  device.lastCO2 = millis();
  // Measurements
  if(device.co2_count==CO2_ARRAY_LEN){
    device.co2_sum-=device.co2[device.co2_index];
    device.co2_count--;
  }
  device.co2[device.co2_index] = myMHZ19.getCO2();                   // CO2 concentration (in ppm);
  Serial.print("co2: ");      Serial.print(device.co2[device.co2_index]);      Serial.print("  ");
  device.co2_sum+=device.co2[device.co2_index];
  device.co2_index++;
  device.co2_count++;
  if(device.co2_index==CO2_ARRAY_LEN) device.co2_index=0;
  device.co2_current=device.co2_sum/device.co2_count;
  sendCO2();
 
  uint16_t temp = myMHZ19.getTemperature();     // Temperature (in degrees Celsius)
  Serial.print("temp: ");     Serial.print(temp);     Serial.println();

  handleLED();

  //Save Data
  if ((millis() - device.lastData) > (1000*SAMPLING_INT*CO2_ARRAY_LEN)) {
    device.lastData = millis();
    client.startDataFrame(BayEOS_Int16le);
    client.addChannelValue(device.co2_current);
    client.writeToBuffer();
    //Send current frame via Websocket to connected devices
    //Try to send to BayEOS-Gateway
    if (cfg.mode && strlen(cfg.bayeos_gateway) && strlen(cfg.bayeos_name)) {
      client.sendMultiFromBuffer(1000);
    }

    if(device.time_is_set){
      client.setBuffer(FSBuffer);
      client.writeToBuffer();
      client.setBuffer(myBuffer);
    }
    sendFrames(); 
  }
}
