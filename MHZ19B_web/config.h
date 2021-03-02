#include <EEPROM.h>
#include <WString.h>
// Types 'byte' und 'word' doesn't work!
String mes; //global String!
uint8_t sd_buffer[1024];
char char_buffer[50]; //global Buffer for chars

#define CONFIG_SIGNATURE 0xfab1ab32

struct MyCONFIG
{
  char name[30];
  char ssid[30];
  char password[30];
  char admin_pw[30];
  bool autocalibration;
  uint8_t mode;/* 0 -> AP-Mode, 1 -> Client Mode, 2 -> Client + AP Mode */
  uint8_t brightness;
  int low; //ppm for yellow
  int high; //ppm for red
  int blink; //ppm for red blink
  int ampel_start; //minutes
  int ampel_end; //minutes
  uint8_t ampel_mode; // 0 -> full stripe , 1 -> full stripe continuous, 2 -> ampel
  int samplingint;
  int samplingavr;
  int loggingint;
  char client_ssid[30];
  char client_pw[30];
  char hostname[30];
  bool static_ip;
  uint32_t ip;
  uint32_t subnet;
  uint32_t gateway;
  uint32_t dns;
  bool bayeos;
  char bayeos_name[50];
  char bayeos_gateway[50];
  char bayeos_user[50];
  char bayeos_pw[50];
  long sig;
} cfg;

struct DeviceStatus
{
  int co2[CO2_ARRAY_LEN];
  int co2_count;
  int co2_index;
  int co2_current;
  int co2_single;
  int co2_sum;
  unsigned long lastCO2;
  unsigned long lastData;
  unsigned long lastBlink;
  unsigned long lastOnOff;
  unsigned long ampel_start;
  unsigned long ampel_end;
  bool led_off;
  bool led_blink;
  bool time_is_set;

} device;


void eraseConfig() {
  // Reset EEPROM bytes to '0' for the length of the data structure
  cfg.sig = CONFIG_SIGNATURE;
  strcpy(cfg.name, DEFAULT_SSID);
  strcpy(cfg.ssid, DEFAULT_SSID);
  strcpy(cfg.hostname, DEFAULT_SSID);
  strcpy(cfg.password, "");
  strcpy(cfg.admin_pw, ADMIN_PASSWORD);
  cfg.mode = 0;
  cfg.brightness=80;
  cfg.autocalibration=true;
  cfg.low = 1000;
  cfg.high = 1500;
  cfg.blink = 2000;
  cfg.samplingint = SAMPLING_INT;
  cfg.samplingavr = 10;
  cfg.loggingint = 100;
  cfg.ampel_start = 360;
  cfg.ampel_end = 1080;
  cfg.ampel_mode = 0;
  cfg.client_ssid[0]=0;
  cfg.client_pw[0]=0;
  cfg.static_ip=0;
  cfg.ip=0x08080808;
  cfg.gateway=0x01080808;
  cfg.subnet=0x00ffffff;
  cfg.dns=0x01080808;
  cfg.bayeos=0;
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
  if (cfg.sig != CONFIG_SIGNATURE ) eraseConfig();
}
