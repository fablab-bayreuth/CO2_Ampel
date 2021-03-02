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
#define DEFAULT_SSID "CO2-Ampel_XXXX"
#define ADMIN_PASSWORD "fab4admins"
#define ESP_VERSION "0.1.7"

#define BAYEOS_GATEWAY "192.168.2.1"
#define BAYEOS_USER "import"
#define BAYEOS_PW "import"

#define LED_DATA_PIN      D3     /* LED strip Din */
#define SAMPLING_INT 10 /* Default Sampling Intervall */
#define CO2_ARRAY_LEN 50 /* Anzahl Werte in Mittelung */
#define SEND_RAW_DATA 0 /* Only effective in Client-Mode and BayEOS-Gateway */

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
#define BUFFER_SIZE 2000
uint8_t buffer[BUFFER_SIZE];
BayEOSBufferRAM myBuffer(buffer, BUFFER_SIZE);
RTC_Millis myRTC;
#include <BayEOSBufferSPIFFS.h>
#define SPIFFSBUFFER_SIZE 800000
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
  FastLED.addLeds< NEOPIXEL, LED_DATA_PIN >(led_data, LED_NUM);
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
  // WS2812B LED strip init
  FastLED.setBrightness(cfg.brightness);

  WiFi.hostname(cfg.hostname);

  bool connected=0;
  if (cfg.mode >= 1) {
    if(cfg.static_ip){
      IPAddress ip(cfg.ip);
      IPAddress subnet(cfg.subnet);
      IPAddress gateway(cfg.gateway);
      IPAddress dns(cfg.dns);
      Serial.println(ip.toString());
      Serial.println(gateway.toString());
      Serial.println(subnet.toString());
      Serial.println(dns.toString());
      
      WiFi.config(ip, subnet, gateway, cfg.dns);
    }
    if(cfg.mode==1) WiFi.mode(WIFI_STA); 
    else  WiFi.mode(WIFI_AP_STA);

    WiFi.begin(cfg.client_ssid, cfg.client_pw);
    Serial.print("Connecting to ");
    Serial.print(cfg.client_ssid);
    Serial.println(" ...");

    int i = 0;
    while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
      led_data[i % LED_NUM] = CRGB::Black;
      i++;
      led_data[i % LED_NUM] = CRGB::DarkBlue;
      FastLED.show();
      delay(500);
      Serial.print(i);
      Serial.print('.');
      if (i > 30) break;
    }
    Serial.println('\n');
    if (WiFi.status() == WL_CONNECTED) {
      connected=1;
      for (size_t i = 0; i < LED_NUM; i++)
        led_data[i] = CRGB::Green;
      FastLED.show();
      Serial.println("Connection established!");
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
      client.setConfig(cfg.bayeos_name, cfg.bayeos_gateway, "80", "gateway/frame/saveFlat", cfg.bayeos_user, cfg.bayeos_pw);
      if (cfg.bayeos && cfg.bayeos_gateway[0] && cfg.bayeos_name[0]) {
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
      WiFi.mode(WIFI_OFF);
    }
  }

  if (cfg.mode == 0 || cfg.mode==2 || ! connected) {
    Serial.println("Starting own network...");
    if(cfg.mode == 0 || ! connected) WiFi.mode(WIFI_AP);
    else WiFi.mode(WIFI_AP_STA);
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
  server.on("/cmd", handleCMD); //Command line interface via GET-Requests - see handler.h for more details.
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


}

// Run 8 measurements to avoid high values at startup
int warmup = 8;
unsigned long loop_count = 0;

void loop(void) {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // handle web server requests
  loop_count++;

  //Handle CO2-Sensor
  if (warmup) {
    if ((millis() - device.lastCO2) > 5000) {
      warmup--;
      led_data[warmup] = CRGB::DarkBlue;
      myMHZ19.getCO2();
      FastLED.show();
      device.lastCO2 = millis();
    }
  } else {
    if ((millis() - device.lastCO2) > (cfg.samplingint * 1000)) {
      handleSensor();
    }
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
