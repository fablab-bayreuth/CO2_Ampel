#include <ArduinoJson.h>
#include <Base64.h>
StaticJsonDocument<1024> doc;

/*
   Method to send current config
   called at connect and on update
*/
void sendConfig(int num=-1) {
  mes = F("{\"event\":\"conf\",\"ssid\":\"");
  mes += cfg.ssid;
  mes += F("\",\"password\":\"");
  mes += cfg.password;
  mes += F("\",\"mode\":");
  mes += cfg.mode;
  mes += F(",\"autocalibration\":");
  mes += cfg.autocalibration;
  mes += F(",\"low\":");
  mes += cfg.low;
  mes += F(",\"high\":");
  mes += cfg.high;
  mes += F(",\"bayeos_name\":\"");
  mes += cfg.bayeos_name;
  mes += F("\",\"bayeos_gateway\":\"");
  mes += cfg.bayeos_gateway;
  mes += F("\",\"bayeos_user\":\"");
  mes += cfg.bayeos_user;
  mes += F("\",\"bayeos_pw\":\"");
  mes += cfg.bayeos_pw;
  mes += "\"}";
  if(num<0) webSocket.broadcastTXT(mes);
  else webSocket.sendTXT(num,mes);
}

void sendCO2(void) {
  mes = F("{\"event\":\"data\",\"co2\":\"");
  mes += (device.co2_sum / device.co2_count);
  mes += "\"}";
  webSocket.broadcastTXT(mes);
}

void sendFrames(bool full,int num) {
  mes = F("{\"event\":\"frame\",\"frames\":[");
  int num_entries = 0;
  char tmp[150];
  char df_buffer[102];

  unsigned long read_pos = myBuffer.readPos();
  if (full) {
    myBuffer.seekReadPointer(myBuffer.endPos());
    while (myBuffer.available()) {
      myBuffer.initNextPacket();
      df_buffer[0] = BayEOS_DelayedFrame;
      *(unsigned long*)(df_buffer + 1) = millis() - myBuffer.packetMillis();
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
  } else {
    base64_encode(tmp, (char*) client.getPayload(), client.getPacketLength());
    tmp[base64_enc_len(client.getPacketLength())] = 0;
    mes += "\"";
    mes += tmp;
    mes += "\"";
  }
  mes += "]}";
  if(full) webSocket.sendTXT(num,mes);
  else webSocket.broadcastTXT(mes);
}





/*
  main method for sending status updates to the clients
*/
void sendEvent(void) {
  if (device.send_error) {
    mes = F("{\"event\":\"error\",\"text\":\"");
    mes += device.error;
    mes += "\"}";
    webSocket.broadcastTXT(mes);
    device.send_error = false;
    return;
  }
  if (device.send_msg) {
    mes = F("{\"event\":\"msg\",\"text\":\"");
    mes += device.msg;
    mes += "\"}";
    webSocket.broadcastTXT(mes);
    device.send_msg = false;
    return;
  }
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
        error(mes);
        return;
      }
      command = doc["command"];
      Serial.println(command);

      if (strcmp(command, "setConf") == 0) {
        if (strcmp(doc["admin_pw"], ADMIN_PASSWORD) != 0) {
          error(String(F("Admin Passwort ist falsch")));
          return;
        }

        //Got a message with new config values!
        strncpy(cfg.ssid, doc["ssid"], 19);
        cfg.ssid[19] = 0;
        if (strlen(doc["ssid"]) > 19) {
          error(String(F("SSID to long! Truncated")));
        }
        strncpy(cfg.password, doc["password"], 19);
        cfg.password[19] = 0;
        cfg.mode = doc["mode"];
        cfg.autocalibration = doc["autocalibration"];
        cfg.low = doc["low"];
        cfg.high = doc["high"];
        strncpy(cfg.bayeos_name, doc["bayeos_name"], 49);
        cfg.bayeos_name[49] = 0;
        strncpy(cfg.bayeos_gateway, doc["bayeos_gateway"], 49);
        cfg.bayeos_gateway[49] = 0;
        strncpy(cfg.bayeos_user, doc["bayeos_user"], 49);
        cfg.bayeos_user[49] = 0;
        strncpy(cfg.bayeos_pw, doc["bayeos_pw"], 49);
        cfg.bayeos_pw[49] = 0;
        saveConfig(); //save to EEPROM - defined in config.h
        client.setConfig(cfg.bayeos_name, cfg.bayeos_gateway, "80", "gateway/frame/saveFlat", cfg.bayeos_user, cfg.bayeos_pw);
        message(String(F("new config saved to EEPROM")));
        myMHZ19.autoCalibration(cfg.autocalibration);
        sendConfig(); //send the current config to client
        return;
      }
      if (strcmp(command, "getConf") == 0) {
        sendConfig(num); //send the current config to client
        return;
      }

      if (strcmp(command, "getAll") == 0) {
        sendConfig(num);
        delay(2);
        sendCO2();
        delay(2);
        sendFrames(true,num);
        return;
      }


  }
}
