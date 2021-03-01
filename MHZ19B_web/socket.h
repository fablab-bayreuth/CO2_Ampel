#include <ArduinoJson.h>
#include <Base64.h>
StaticJsonDocument<1024> doc;

/*
   Method to send current config
   called at connect and on update
*/
void sendConfig(int num = -1) {
  mes = F("{\"event\":\"conf\",\"ssid\":\"");
  mes += cfg.ssid;
  mes += F("\",\"name\":\"");
  mes += cfg.name;
  mes += F("\",\"password\":\"");
  mes += cfg.password;
  mes += F("\",\"mode\":");
  mes += cfg.mode;
  mes += F(",\"ampel_mode\":");
  mes += cfg.ampel_mode;
  mes += F(",\"autocalibration\":");
  mes += cfg.autocalibration;
  mes += F(",\"low\":");
  mes += cfg.low;
  mes += F(",\"high\":");
  mes += cfg.high;
  mes += F(",\"blink\":");
  mes += cfg.blink;
  mes += F(",\"samplingint\":");
  mes += cfg.samplingint;
  mes += F(",\"samplingavr\":");
  mes += cfg.samplingavr;
  mes += F(",\"loggingint\":");
  mes += cfg.loggingint;
  mes += F(",\"socket_id\":");
  mes += num;
  mes += F(",\"co2_array_len\":");
  mes += CO2_ARRAY_LEN;
  mes += F(",\"frame_size\":");
  mes += FRAME_SIZE;
  mes += F(",\"ampel_start\":");
  mes += cfg.ampel_start;
  mes += F(",\"ampel_end\":");
  mes += cfg.ampel_end;
  mes += F(",\"brightness\":");
  mes += cfg.brightness;
  mes += F(",\"client_ssid\":\"");
  mes += cfg.client_ssid;
  mes += F("\",\"client_pw\":\"");
  mes += cfg.client_pw; 
  mes += F("\",\"hostname\":\"");
  mes += cfg.hostname;
  mes += F("\",\"static_ip\":");
  mes += cfg.static_ip;
  mes += F(",\"ip\":\"");
  IPAddress ip(cfg.ip);
  mes += ip.toString();
  mes += F("\",\"subnet\":\"");
  ip=IPAddress(cfg.subnet);
  mes += ip.toString();
  mes += F("\",\"gateway\":\"");
  ip=IPAddress(cfg.gateway);
  mes += ip.toString();
  mes += F("\",\"dns\":\"");
  ip=IPAddress(cfg.dns);
  mes += ip.toString();
  mes += F("\",\"bayeos\":");
  mes += cfg.bayeos;
  mes += F(",\"bayeos_name\":\"");
  mes += cfg.bayeos_name;
  mes += F("\",\"bayeos_gateway\":\"");
  mes += cfg.bayeos_gateway;
  mes += F("\",\"bayeos_user\":\"");
  mes += cfg.bayeos_user;
  mes += F("\",\"bayeos_pw\":\"");
  mes += cfg.bayeos_pw;
  mes += F("\",\"esp_version\":\"");
  mes += ESP_VERSION;
  mes += F("\",\"mac\":\"");
  mes += WiFi.macAddress();
  mes += F("\",\"current_ip\":\"");
  mes += WiFi.localIP().toString();
  mes += "\"}";
  if (num < 0) webSocket.broadcastTXT(mes);
  else webSocket.sendTXT(num, mes);
}

void sendCO2(void) {
  if(! device.co2_current) return;
  mes = F("{\"event\":\"data\",\"co2\":");
  mes += device.co2_current;
  mes += F(",\"co2_single\":");
  mes += device.co2_single;
  mes += "}";
  webSocket.broadcastTXT(mes);
}

void sendBlink(void) {
  mes = F("{\"event\":\"blink\",\"off\":");
  mes += device.led_blink;
  mes += "}";
  webSocket.broadcastTXT(mes);
}

void sendBuffer(int num){
  mes = F("{\"event\":\"buffer\",\"write\":");
  mes += FSBuffer.writePos();
  mes += F(",\"read\":");
  mes += FSBuffer.readPos();
  mes += F(",\"end\":");
  mes += FSBuffer.endPos();
  mes += F(",\"length\":");
  mes += FSBuffer.length();
  mes += "}"; 
  if(num<0) webSocket.broadcastTXT(mes);
  else webSocket.sendTXT(num, mes);
}

void sendFrames(int num) {
  mes = F("{\"event\":\"frame\",\"frames\":[");
  int num_entries = 0;
  char tmp[150];
  char df_buffer[102];
  unsigned long timestamp;
  unsigned long read_pos = myBuffer.readPos();
  unsigned long ram_ts;
  myBuffer.seekReadPointer(myBuffer.endPos());
  while (myBuffer.available()) {
    myBuffer.initNextPacket();
    df_buffer[0] = BayEOS_DelayedFrame;
    timestamp = millis() - myBuffer.packetMillis();
    if (! ram_ts) ram_ts = myRTC.sec() - timestamp / 1000;
    memcpy(df_buffer + 1, (uint8_t*) &timestamp, 4);
    myBuffer.readPacket((uint8_t*)df_buffer + 5);
    base64_encode(tmp, df_buffer, myBuffer.packetLength() + 5);
    tmp[base64_enc_len(myBuffer.packetLength() + 5)] = 0;
    if (num_entries) mes += ",";
    mes += "\"";
    mes += tmp;
    mes += "\"";
    num_entries++;
    myBuffer.next();
    yield();
  }
  myBuffer.seekReadPointer(read_pos);
  if (num_entries < 100) {
    read_pos = FSBuffer.readPos();
    long pos = FSBuffer.writePos();
    pos -= 1000;
    if (pos < 0) pos = 0;
    FSBuffer.seekReadPointer(pos);
    while (FSBuffer.available()) {
      FSBuffer.initNextPacket();
      timestamp = FSBuffer.packetMillis();
      if (timestamp > ram_ts) break;
      df_buffer[0] = BayEOS_TimestampFrame;
      memcpy(df_buffer + 1, (uint8_t*) &timestamp, 4);
      FSBuffer.readPacket((uint8_t*)df_buffer + 5);
      base64_encode(tmp, df_buffer, FSBuffer.packetLength() + 5);
      tmp[base64_enc_len(FSBuffer.packetLength() + 5)] = 0;
      if (num_entries) mes += ",";
      mes += "\"";
      mes += tmp;
      mes += "\"";
      num_entries++;
      FSBuffer.next();
      yield();
    }
    FSBuffer.seekReadPointer(read_pos);
  }
  mes += "]}";
  webSocket.sendTXT(num, mes);
}





void sendMessage(char* str, bool error = false, int num = -1) {
  mes = F("{\"event\":\"");
  if (error) mes += F("error");
  else mes += F("msg");
  mes += F("\",\"text\":\"");
  mes += str;
  mes += "\"}";
  if (num >= 0) webSocket.sendTXT(num, mes);
  else webSocket.broadcastTXT(mes);
}

void sendMessage(String &s, bool error = false, int num = -1) {
  mes = F("{\"event\":\"");
  if (error) mes += F("error");
  else mes += F("msg");
  mes += F("\",\"text\":\"");
  mes += s;
  mes += "\"}";
  if (num >= 0) webSocket.sendTXT(num, mes);
  else webSocket.broadcastTXT(mes);
}


//WebsocketEvent
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload,
                    size_t length) {
  DeserializationError json_error;
  const char* command;
  uint8_t i = 0;
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0],
                      ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      json_error = deserializeJson(doc, payload);
      if (json_error) {
        mes = F("deserializeJson() failed: ");
        mes += json_error.c_str();
        sendMessage(mes, true, num);
        return;
      }
      command = doc["command"];
      Serial.println(command);

      if (strcmp(command, "setConf") == 0) {
        //Got a message with new config values!
        if (strcmp(doc["admin_pw"], cfg.admin_pw) != 0) {
          sendMessage("Admin Passwort ist falsch", true, num);
          return;
        }
        if (strcmp(doc["admin_pw1"], doc["admin_pw2"]) != 0) {
          sendMessage("Die Admin Passwörter sind nicht gleich", true, num);
          return;
        }
        if (strlen(doc["admin_pw1"]) > 0) {
          if (strlen(doc["admin_pw1"]) < 6) {
            sendMessage("Die Admin Passwort ist zu kurz", true, num);
            return;
          } else if (strlen(doc["admin_pw1"]) > 29) {
            sendMessage("Die Admin Passwort ist zu lang", true, num);
            return;
          } else {
            strncpy(cfg.admin_pw, doc["admin_pw1"], 29);
            cfg.admin_pw[29] = 0;
          }
        }
        strncpy(cfg.name, doc["name"], 29);
        cfg.name[29] = 0;
        if (strlen(doc["name"]) > 29) {
          sendMessage("Name to long! Truncated", true, num);
        }
        strncpy(cfg.ssid, doc["ssid"], 29);
        cfg.ssid[29] = 0;
        if (strlen(doc["ssid"]) > 29) {
          sendMessage("SSID to long! Truncated", true, num);
        }
        strncpy(cfg.client_ssid, doc["client_ssid"], 29);
        cfg.client_ssid[29] = 0;
        if (strlen(doc["client_ssid"]) > 29) {
          sendMessage("Client SSID to long! Truncated", true, num);
        }
        strncpy(cfg.client_pw, doc["client_pw"], 29);
        cfg.client_pw[29] = 0;
        if (strlen(doc["client_pw"]) > 29) {
          sendMessage("Client PW to long! Truncated", true, num);
        }
        strncpy(cfg.hostname, doc["hostname"], 29);
        cfg.hostname[29] = 0;
        if (strlen(doc["hostname"]) > 29) {
          sendMessage("Hostname to long! Truncated", true, num);
        }
        strncpy(cfg.password, doc["password"], 29);
        cfg.password[29] = 0;
        cfg.mode = doc["mode"];
        cfg.ampel_mode = doc["ampel_mode"];
        cfg.brightness = doc["brightness"];
        FastLED.setBrightness(cfg.brightness);
        cfg.autocalibration = doc["autocalibration"];
        cfg.low = doc["low"];
        cfg.high = doc["high"];
        cfg.blink = doc["blink"];
        cfg.samplingint = doc["samplingint"];
        if(cfg.samplingint<2) cfg.samplingint=2;
        if(cfg.samplingint>30) cfg.samplingint=30;
        if(cfg.samplingavr != doc["samplingavr"]){
        	device.co2_count=0;
        	device.co2_index=0;
        	device.co2_sum=0;
        }
        cfg.samplingavr = doc["samplingavr"];
        if(cfg.samplingavr<1) cfg.samplingavr=1;
        if(cfg.samplingavr>CO2_ARRAY_LEN) cfg.samplingavr=CO2_ARRAY_LEN;
        cfg.loggingint = doc["loggingint"];
        if(cfg.loggingint<cfg.samplingint) cfg.loggingint=cfg.samplingint;
        if(cfg.loggingint>3600) cfg.loggingint=3600;
        cfg.ampel_start = doc["ampel_start"];
        cfg.ampel_end = doc["ampel_end"];
        cfg.static_ip = doc["static_ip"];
        char buffer[16];
        strncpy(buffer,doc["ip"],16);
        IPAddress ip;
        ip.fromString(buffer);
        cfg.ip=(uint32_t) ip;
        strncpy(buffer,doc["gateway"],16);
        ip.fromString(buffer);
        cfg.gateway=(uint32_t) ip;
        strncpy(buffer,doc["subnet"],16);
        ip.fromString(buffer);
        cfg.subnet=(uint32_t) ip;
        strncpy(buffer,doc["dns"],16);
        ip.fromString(buffer);
        cfg.dns=(uint32_t) ip;
        cfg.bayeos = doc["bayeos"];
        strncpy(cfg.bayeos_name, doc["bayeos_name"], 49);
        cfg.bayeos_name[49] = 0;
        strncpy(cfg.bayeos_gateway, doc["bayeos_gateway"], 49);
        cfg.bayeos_gateway[49] = 0;
        strncpy(cfg.bayeos_user, doc["bayeos_user"], 49);
        cfg.bayeos_user[49] = 0;
        strncpy(cfg.bayeos_pw, doc["bayeos_pw"], 49);
        cfg.bayeos_pw[49] = 0;
        saveConfig(); //save to EEPROM - defined in config.h
        device.lastOnOff = millis() - 60000; //Will check for updated on/off-time
        client.setConfig(cfg.bayeos_name, cfg.bayeos_gateway, "80", "gateway/frame/saveFlat", cfg.bayeos_user, cfg.bayeos_pw);
        sendMessage("new config saved to EEPROM", false, num);
        if (doc["zerocal"]) {
          myMHZ19.calibrateZero();
          sendMessage("Runing zero calibration", true);
        }
        myMHZ19.autoCalibration(cfg.autocalibration);
        sendConfig(); //send the current config to client
        if (doc["restart"]) {
          sendMessage("Starte CO2-Ampel neu", true);
          delay(3000);
          ESP.reset();
        }

        
        return;
      }
      if (strcmp(command, "getConf") == 0) {
        sendConfig(num); //send the current config to client
        return;
      }

      if (strcmp(command, "getAll") == 0) {
        unsigned long time = doc["time"];
        myRTC.adjust(time);
        device.time_is_set = true;
        //pinMode(LED_BUILTIN,OUTPUT);
        //digitalWrite(LED_BUILTIN,LOW);
        sendConfig(num);
        delay(2);
        sendFrames(num);
        delay(2);
        sendCO2();
        delay(2);
        sendBuffer(num);
        return;
      }


  }
}
