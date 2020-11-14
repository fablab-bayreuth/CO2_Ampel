#include <EEPROM.h>
#include <WString.h>
// Types 'byte' und 'word' doesn't work!
String mes; //global String!
uint8_t sd_buffer[1024];
char char_buffer[50]; //global Buffer for chars

#define CONFIG_SIGNATURE 0xfab1ab24

struct MyCONFIG
{
  char ssid[20];
  char password[20];
  bool autocalibration;
  uint8_t mode;/* 0 -> AP-Mode, 1 -> Client Mode */
  int low; //ppm for green
  int high; //ppm for red
  char bayeos_name[50];
  char bayeos_gateway[50];
  char bayeos_user[50];
  char bayeos_pw[50];
  long sig;
} cfg;

struct DeviceStatus
{
  bool send_error;
  char error[50];
  bool send_msg;
  char msg[50];
  int co2[CO2_ARRAY_LEN];
  int co2_count;
  int co2_current;
  int co2_sum;
  unsigned long lastCO2;
  unsigned long lastData;
} device;

void error(const String& s) {
  s.toCharArray(device.error, 50);
  device.send_error = true;
}
void message(const String& s) {
  s.toCharArray(device.msg, 50);
  device.send_msg = true;
}

void eraseConfig() {
  // Reset EEPROM bytes to '0' for the length of the data structure
  cfg.sig = CONFIG_SIGNATURE;
  strcpy(cfg.ssid, "CO2Ampel");
  strcpy(cfg.password, "fablab24");
  cfg.mode = 0;
  cfg.autocalibration=true;
  cfg.low = 1000;
  cfg.high = 1400;
  cfg.bayeos_name[0] = 0;
  cfg.bayeos_gateway[0] = 0;
  cfg.bayeos_user[0] = 0;
  cfg.bayeos_pw[0] = 0;
  EEPROM.put( 0, cfg );
  delay(200);
  EEPROM.commit();                      // Only needed for ESP8266 to get data written
}

void saveConfig() {
  EEPROM.put( 0, cfg );
  delay(200);
  EEPROM.commit();                      // Only needed for ESP8266 to get data written
}

void loadConfig() {
  // Loads configuration from EEPROM into RAM
  EEPROM.get( 0, cfg );
  if (cfg.sig != CONFIG_SIGNATURE || cfg.mode > 1) eraseConfig();
}
