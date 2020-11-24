/*
   default SSID: CO2Ampel
   default Password: fablab24
   adminPassword: fab4admins

   Webfrontend on http://192.168.4.1

   Commands via WebSocket
   setConf
   getConf
   getAll

   download File is handled by Webserver

   You have to upload the sketch _AND_ the data in two steps
   for the data use ESP8266 Sketch Data Upload tool
   https://github.com/esp8266/arduino-esp8266fs-plugin

   Install the following libraries via library manager:
   - WebSockets (by Markus Sattler)
   - ArduinoJSON
   - BayEOS-ESP
   - MHZ19


*/
#define ESP_VERSION "0.0.7"
#define ADMIN_PASSWORD "fab4admins"
#define DEFAULT_SSID "CO2-Ampel"
#define LED_DATA_PIN      D3     /* LED strip Din */
#define SAMPLING_INT 10 /* Sampling Intervall */
#define CO2_ARRAY_LEN 10 /* Anzahl Werte in Mittelung */

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
DNSServer dns;
#define DNS_PORT 53

#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#include <WebSocketsServer.h>
WebSocketsServer webSocket(81);    // create a websocket server on port 81

/*
  Speicher für Dataframes - Ein Frame ist 10byte -> 200 Datenpunke
  Rückblick auf -> 200*(SAMPLING_INT * CO2_ARRAY_LENGTH) -> z.B. 5,5h
*/
#include <BayEOSBufferRAM.h>
#define BUFFER_SIZE 4000
uint8_t buffer[BUFFER_SIZE];
BayEOSBufferRAM myBuffer(buffer, BUFFER_SIZE);
RTC_Millis myRTC;
#include <BayEOSBufferSPIFFS.h>
#define SPIFFSBUFFER_SIZE 500000
BayEOSBufferSPIFFS2 FSBuffer(SPIFFSBUFFER_SIZE);

/* BayEOS-Client - um Daten an eine BayEOS-Gateway zu schicken*/
#include <BayEOS-ESP8266.h>
BayESP8266 client;


#include "config.h"
#include "co2.h"
#include "socket.h"
#include "handler.h"
#include "localtime_DE.h"


void setup(void) {
  // WS2812B LED strip init
  FastLED.addLeds< NEOPIXEL, LED_DATA_PIN >(led_data, LED_NUM);
  FastLED.setBrightness(LED_BRIGHTNESS);

  for (size_t i = 0; i < LED_NUM; i++)
    led_data[i] = CRGB::Black;
  FastLED.show();

  Serial.begin(115200);
  client.setBuffer(myBuffer);
  EEPROM.begin(sizeof(cfg));
  delay(1000);
  SPIFFS.begin();
  FSBuffer.init();
  FSBuffer.setRTC(myRTC);


  loadConfig();

  if (cfg.mode == 1) {
    WiFi.begin(cfg.ssid, cfg.password);
    Serial.print("Connecting to ");
    Serial.print(cfg.ssid);
    Serial.println(" ...");

    int i = 0;
    while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
      led_data[i % LED_NUM] = CRGB::Black;
      i++;
      led_data[i] = CRGB::DarkBlue;
      FastLED.show();
      delay(500);
      Serial.print(i);
      Serial.print('.');
      if (i > 20) break;
    }
    Serial.println('\n');
    if (WiFi.status() == WL_CONNECTED) {
      for (size_t i = 0; i < LED_NUM; i++)
        led_data[i] = CRGB::Green;
      FastLED.show();
      Serial.println("Connection established!");
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
      client.setConfig(cfg.bayeos_name, cfg.bayeos_gateway, "80", "gateway/frame/saveFlat", cfg.bayeos_user, cfg.bayeos_pw);
      if (strlen(cfg.bayeos_gateway) && strlen(cfg.bayeos_name)) {
        client.startFrame(BayEOS_Message);
        client.addToPayload("Ampel IP: <a href=\"http://");
        client.addToPayload(WiFi.localIP().toString());
        client.addToPayload("\">");
        client.addToPayload(WiFi.localIP().toString());
        client.addToPayload("</a>");
        client.sendPayload();
      }
    } else {
      for (size_t i = 0; i < LED_NUM; i++)
        led_data[i] = CRGB::Red;
      FastLED.show();
      Serial.println("Connection failed - Falling back to default AP-Mode");
      strcpy(cfg.ssid, DEFAULT_SSID);
      strcpy(cfg.password, "");
      cfg.mode = 0;
    }
  }

  if (cfg.mode == 0) {
    Serial.println("Starting own network...");
    WiFi.mode(WIFI_AP);
    IPAddress Ip(192, 168, 4, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);
    if (strlen(cfg.password)) {
      WiFi.softAP(cfg.ssid, cfg.password);
    } else {
      WiFi.softAP(cfg.ssid);
    }
    /* Setup the DNS server redirecting all the domains to the apIP */
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(DNS_PORT, "*", WiFi.softAPIP());

    delay(100);
  }
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  server.on("/download", handleDownload);
  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists
  server.begin();

  // CO2 sensor init
  mySerial.begin(MHZ19_BAUDRATE);                               // (Uno example) device to MH-Z19 serial start
  myMHZ19.begin(mySerial);                                // *Serial(Stream) refence must be passed to library begin().
  myMHZ19.autoCalibration(cfg.autocalibration);                              // Turn auto calibration ON (OFF autoCalibration(false))

  // Set to black again
  for (size_t i = 0; i < LED_NUM; i++)
    led_data[i] = CRGB::Black;
  FastLED.show();


  // Run 8 measurements to avoid high values at startup
  for (size_t i = 0; i < LED_NUM; i++) {
    led_data[i] = CRGB::DarkBlue;
    myMHZ19.getCO2();
    FastLED.show();
    delay(5000);
  }

}

void loop(void) {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // handle web server requests

  //Handle CO2-Sensor
  if ((millis() - device.lastCO2) > (SAMPLING_INT * 1000)) {
    handleSensor();
  }
  //Handle DNS-Request
  if (cfg.mode == 0)
    dns.processNextRequest();

  //Blink Red LED
  if (device.co2_current > cfg.blink && (millis() - device.lastBlink) > 1000) {
    device.lastBlink = millis();
    device.led_blink = !device.led_blink;
    handleLED();
    sendBlink();
  }

  //Check ON/OFF
  if (device.time_is_set && (cfg.ampel_start || cfg.ampel_end) && (millis() - device.lastOnOff) > 60000) {
    device.lastOnOff = millis();
    DateTime utc;
    utc = DateTime(myRTC.sec() + 3600);
    utc = DateTime(utc.year(), utc.month(), utc.day());

    device.led_off = false;
    if (cfg.ampel_start && myRTC.sec() < (utc.get() - getShift(utc) + 60 * cfg.ampel_start)) device.led_off = true;
    if (cfg.ampel_end && myRTC.sec() > (utc.get() - getShift(utc) + 60 * cfg.ampel_end)) device.led_off = true;
    handleLED();
  }
}
