
String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("fwlink")) path = "/";                //Microsoft captive portal.
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File SPIFFSfile = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(SPIFFSfile, contentType);    // Send it to the client
    SPIFFSfile.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void sendNotFound(void) {
  server.send(404, "text/plain; charset=utf-8", "404: File Not Found");
}

boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}
/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (cfg.mode == 0 && ! isIp(server.hostHeader())) {
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", "");
    server.client().stop();
    return;
  }
  //Send file or Not Found
  if ( ! handleFileRead(server.uri()) ) 
    sendNotFound();

}


void handleDownload(void) {
  char buffer[10];
  server.arg(0).toCharArray(buffer, 10);
  bool csv_format = (buffer[0] == '1');
  server.arg(1).toCharArray(buffer, 10);
  long dl_size = atoi(buffer);

  unsigned long read_pos = FSBuffer.readPos();
  unsigned long pos = FSBuffer.endPos();
  if (dl_size >= 0) {
    if (dl_size > 0) {
      if (dl_size > (FSBuffer.writePos() - FSBuffer.endPos())) {
        pos = FSBuffer.endPos();
      } else {
        pos = FSBuffer.writePos() - FSBuffer.endPos();
        if (pos > FSBuffer.length()) {
          pos += FSBuffer.length();
        }
      }
    } else
      pos=FSBuffer.readPos();
  }
  FSBuffer.seekReadPointer(pos);

  server.client().print(F("HTTP/1.1 200\r\n"));
  if (! csv_format) server.client().print(F("Content-Type: application/octet-stream\r\n"));
  else server.client().print(F("Content-Type: text/csv\r\n"));
  server.client().print(F("Content-Description: "));
  server.client().print(cfg.ssid);
  server.client().write('.');
  if (csv_format) server.client().print("csv");
  else  server.client().print("dbd");
  server.client().print(F("\r\n"));
  server.client().print(F("Content-Transfer-Encoding: binary\r\n"));
  server.client().print(F("Expires: 0\r\n"));
  server.client().print(F("Cache-Control: must-revalidate, post-check=0, pre-check=0\r\n"));
  server.client().print(F("Content-disposition: inline; filename="));
  server.client().print(cfg.ssid);
  server.client().write('.');
  if (csv_format) server.client().print("csv");
  else  server.client().print("dbd");
  server.client().print(F("\r\n"));
  server.client().print(F("Content-Length: "));
  char tmp[12];
  unsigned long flength = FSBuffer.available();
  if (csv_format) flength = flength * 25 / 10 + 10; //10.11.2020 08:44:33;1345\n
  else flength += 4 + 1 + strlen(cfg.ssid);
  itoa(flength, tmp, 10);
  server.client().print(tmp);
  server.client().print(F("\r\n"));
  server.client().print(F("Connection: close\r\n\r\n"));

  if (! csv_format) {
    const long sig = 0x50e0a10b; //File Signature 0BA1E050
    server.client().write((uint8_t*)&sig, 4);
    server.client().write(strlen(cfg.ssid));
    server.client().write((uint8_t*)cfg.ssid, strlen(cfg.ssid));
    uint8_t i_buffer[128];
    pos = FSBuffer.readPos();
    uint8_t bytes_read;
    while (pos != FSBuffer.writePos() &&  server.client().connected()) {
      yield();
      bytes_read = FSBuffer.readBinary(pos, FSBuffer.writePos(), 128, i_buffer);
      pos += bytes_read;
      if (pos > FSBuffer.length()) pos -= FSBuffer.length();
      server.client().write(i_buffer, bytes_read);
    }

  } else {
    DateTime utc;
    server.client().print("Datum;CO2\n");
    pos = 10;
    int16_t value;
    client.setBuffer(FSBuffer);
    while (FSBuffer.available() &&  server.client().connected()) {
      yield();
      client.readFromBuffer();
      utc = DateTime(FSBuffer.packetMillis() + 3600);
      memcpy((uint8_t*)& value, (uint8_t*)(client.getPayload() + 8), 2);
      server.client().printf("%02d.%02d.%4d %02d:%02d:%02d;%4d\n", utc.day(), utc.month(), utc.year(),
                             utc.hour(), utc.minute(), utc.second(), value);
      pos += 25;
      FSBuffer.next();
    }

    while (pos < flength) {
      server.client().print(" ");
      pos++;
    }

  }

  client.setBuffer(myBuffer);

  server.client().stop();
  FSBuffer.seekReadPointer(FSBuffer.writePos());
  return;

}
