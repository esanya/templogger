
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

#include <uri/UriBraces.h>
#include <uri/UriRegex.h>

#ifndef STASSID
#define STASSID "logger-ap"; 
#define STAPSK "your-password"
#endif

#include <dhtnew.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h> 
#include <string.h>

#define vers "Version: 11.11.2026 taupunkmesser"
#define RTCMEMORYSTART 65 
#define CONFIG_FILENAME "/configfile.conf"

#define DHTPIN_1 5 
#define DHTTYPE_1 DHT22 
#define Korrektur_t_1  -3 
#define Korrektur_h_1  0  

#define CTRLPIN_WIFI_AP		3

#define FLASH_MAP_SETUP_CONFIG(FLASH_MAP_MAX_FS)
ADC_MODE(ADC_VCC)

const char *ssid = STASSID;
const char *password = STAPSK;
uint32_t bootcount = 0; //deepsleeptime knapp unter 2hoch32, daher 22 x booten, Bootcount wird in der rtc des ESP gespeichert
uint32_t deepsleeptime = (1800*1000)*1000; 
char buf[200]; 

DHTNEW dht1(DHTPIN_1); 
bool fehler = true;
ESP8266WebServer server(80);

uint16_t floatToFloat16(float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));

  uint16_t sign = (bits >> 16) & 0x8000;
  uint32_t exponent = (bits >> 23) & 0xff;
  uint32_t mantissa = bits & 0x7fffff;

  if (exponent == 0xff) {
    return sign | (mantissa ? 0x7e00 : 0x7c00);
  }

  int32_t halfExponent = static_cast<int32_t>(exponent) - 127 + 15;
  if (halfExponent >= 31) {
    return sign | 0x7c00;
  }

  if (halfExponent <= 0) {
    if (halfExponent < -10) {
      return sign;
    }

    mantissa |= 0x800000;
    uint32_t shift = 14 - halfExponent;
    uint32_t rounded = mantissa >> shift;
    uint32_t remainder = mantissa & ((1U << shift) - 1);
    uint32_t halfway = 1U << (shift - 1);
    if (remainder > halfway || (remainder == halfway && (rounded & 1))) {
      rounded++;
    }
    return sign | rounded;
  }

  uint32_t roundedMantissa = mantissa >> 13;
  uint32_t remainder = mantissa & 0x1fff;
  if (remainder > 0x1000 || (remainder == 0x1000 && (roundedMantissa & 1))) {
    roundedMantissa++;
    if (roundedMantissa == 0x400) {
      roundedMantissa = 0;
      halfExponent++;
      if (halfExponent >= 31) {
        return sign | 0x7c00;
      }
    }
  }

  return sign | (halfExponent << 10) | roundedMantissa;
}

void appendMeasurement(unsigned long count, float t1, float h1, float dewPoint, bool sensorError) {
  // Little-endian record: uint32 bootcount, float16 t1/h1/dewPoint, uint8 error flag.
  uint8_t record[11];
  uint16_t t1Half = floatToFloat16(t1);
  uint16_t h1Half = floatToFloat16(h1);
  uint16_t dewPointHalf = floatToFloat16(dewPoint);

  record[0] = count & 0xff;
  record[1] = (count >> 8) & 0xff;
  record[2] = (count >> 16) & 0xff;
  record[3] = (count >> 24) & 0xff;
  record[4] = t1Half & 0xff;
  record[5] = t1Half >> 8;
  record[6] = h1Half & 0xff;
  record[7] = h1Half >> 8;
  record[8] = dewPointHalf & 0xff;
  record[9] = dewPointHalf >> 8;
  record[10] = sensorError ? 1 : 0;

  const char *path = "/measurement.bin";
  Serial.printf("Appending binary measurement to: %s\n", path);
  File file = LittleFS.open(path, "a");
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.write(record, sizeof(record)) == sizeof(record)) {
    Serial.println("Binary measurement appended");
  } else {
    Serial.println("Binary measurement append failed");
  }
  file.close();
}

void appendFile(const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = LittleFS.open(path, "a");
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void listDir(const char *dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("  FILE: ");
    Serial.print(root.fileName());
    Serial.print("  SIZE: ");
    Serial.print(file.size());
    time_t cr = file.getCreationTime();
    time_t lw = file.getLastWrite();
    file.close();
    struct tm *tmstruct = localtime(&cr);
    Serial.printf("    CREATION: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    tmstruct = localtime(&lw);
    Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
  }
  Serial.println("");
}


float taupunkt(float t, float r) {
  
float a, b;
  
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  float sdd = 6.1078 * pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  float dd = sdd * (r/100);
  
  // v-Parameter
  float v = log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  float tt = (b*v) / (a-v);
  return { tt };  
}

void take_measurement_and_store() {
  dht1.powerUp();
  delay(2000);

  dht1.read(); 

  float h1 = dht1.getHumidity()   + Korrektur_h_1;       
  float t1 = dht1.getTemperature()+ Korrektur_t_1;   
  float dewPoint = NAN;
  dht1.powerDown();

  if (isnan(h1) || isnan(t1) || h1 > 100 || h1 < 1 || t1 < -40 || t1 > 80 )  {
    Serial.println(F("Fehler beim Auslesen vom 1. Sensor!"));
    fehler = true;
  } else {
    fehler = false;
    dewPoint = taupunkt(t1, h1);
  } 

#ifdef WRITE_TEXT_FORMAT
  snprintf(buf, sizeof(buf), "%lu,%.2f,%.2f,%.2f,%b\n", (unsigned long)bootcount, t1, h1, dewPoint, fehler);
  appendFile("/measurement.txt", buf);
#else
  appendMeasurement((unsigned long)bootcount, t1, h1, dewPoint, fehler);
#endif
}

void handle_reset_from_deepsleep() {
  ESP.rtcUserMemoryRead(RTCMEMORYSTART, &bootcount, sizeof(bootcount)); 
  bootcount++;
  ESP.rtcUserMemoryWrite(RTCMEMORYSTART, &bootcount, sizeof(bootcount)); 
  ESP.rtcUserMemoryRead(RTCMEMORYSTART+1, &deepsleeptime, sizeof(deepsleeptime));
  Serial.print("bootcount= ");
  Serial.print(bootcount); 
  Serial.print(", deepsleeptime= ");
  Serial.println(deepsleeptime);

  take_measurement_and_store();

  ESP.deepSleep(deepsleeptime, WAKE_RF_DISABLED); 
  delay(100); 
}

void handle_first_start_and_deepsleep() {
  bootcount=0; 
  ESP.rtcUserMemoryWrite(RTCMEMORYSTART, &bootcount, sizeof(bootcount));

  ESP.rtcUserMemoryWrite(RTCMEMORYSTART+1, &deepsleeptime, sizeof(deepsleeptime));

  Serial.println("Mount LittleFS");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
  }

  take_measurement_and_store();

  Serial.println("going to the first deep Sleep");
  ESP.deepSleep(deepsleeptime, WAKE_RF_DISABLED); 
  delay(100);
}

void setup() {
  unsigned long t=micros();
  Serial.begin(115200);
  Serial.print(vers);
  Serial.print("\nWaking up..., ESP.getResetReason: "); 
  Serial.println(ESP.getResetReason().c_str());

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  pinMode(CTRLPIN_WIFI_AP, INPUT);
  bool startWifiAp = digitalRead(CTRLPIN_WIFI_AP);

  // die deepsleep Zeit wird in Mikrosekunden angegeben und beträgt maximal 2hoch32, also etwas mehr als 4 Milliarden, was ca. 70 Minuten entspricht
  // Der ESP soll aber 30minuten schlafen, also sorgt der nachfolgende Algorythmus dafür das der ESP nach knapp 30 Minuten kurz aufwacht 
  // (dauert ca 1,5 millisek.), den bootcounter erhöht und im nichtfluechtigem rtc Speicher ablegt und wieder schlafen geht. 
  // Ob das System aus dem deepsleep kommt oderneu eingeschaltet wurde, wird über ESP.getReason ermittelt.

  strcpy(buf,ESP.getResetReason().c_str());
  if(buf[0]=='D') { //D steht für DeepSleeep, weitere Faelle waeren z.B. 'P'ower on oder 'S'oftware watchdog . . . 
    handle_reset_from_deepsleep();
  } else if (startWifiAp) {
    wifi_server_setup();
  } else {
    handle_first_start_and_deepsleep();
  }
}

void wifi_server_setup(void) {
  Serial.println("");

  WiFi.printDiag(Serial);
  WiFi.softAP(ssid, password);
  WiFi.printDiag(Serial);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on(F("/"), []() {
    Serial.println(F(""));
    server.send(200, F("text/plain"), F("hello from esp8266!"));
  });

  server.on(F("/vcc"), []() {
    Serial.println(F(""));
    JsonDocument doc;

    doc[F("vcc")] = ESP.getVcc();

    String output;
    serializeJsonPretty(doc, output);
    server.send(200, F("text/json"), output);
  });

  server.on(F("/reboot"), []() {
    Serial.println(F(""));
    LittleFS.end();
    Serial.println("LittleFS umount");
    server.send(200, F("text/plain"), F("going to reboot!"));

    ESP.restart();
    delay(100); 
  });

  server.on(F("/deepsleep"), []() {
    Serial.println(F(""));
    LittleFS.end();
    Serial.println("LittleFS umount");
    server.send(200, F("text/plain"), F("going to deepsleep!"));

    ESP.deepSleep(deepsleeptime, WAKE_RF_DISABLED); 
    delay(100); 
  });

  server.on(F("/format"), []() {
    Serial.println(F(""));
    Serial.println("Formatting LittleFS filesystem");
    LittleFS.format();
    server.send(200, F("text/plain"), "formatting littlefs done!");
  });

  server.on(F("/df"), []() {
    Serial.println(F(""));
    Serial.println("Gettin LittleFS filesysteminfo");

    FSInfo fs_info;
    LittleFS.info(fs_info);

    JsonDocument doc;

    // Add values in the document
    doc[F("totalBytes")] = fs_info.totalBytes;
    doc[F("usedBytes")] = fs_info.usedBytes;
    doc[F("blockSize")] = fs_info.blockSize;
    doc[F("pageSize")] = fs_info.pageSize;
    doc[F("maxOpenFiles")] = fs_info.maxOpenFiles;
    doc[F("maxPathLength")] = fs_info.maxPathLength;

    String output;
    serializeJsonPretty(doc, output);
    server.send(200, F("text/json"), output);
  });

  server.on(F("/ls"), []() {
    Serial.println(F(""));
    listDir("/");
    server.send(200, "text/plain", "listing dir!");
  });

  server.on(UriBraces("/setconfig/{}/{}"), []() {
    Serial.println(F(""));
    String param = server.pathArg(0);
    String value = server.pathArg(1);

    JsonDocument doc;
    File file = LittleFS.open(F(CONFIG_FILENAME), "r");

    if (!file) {
      Serial.println("file open failed");
    } else {
      deserializeJson(doc, file);
      file.close();
    }
    doc[param] = value;
 
    file = LittleFS.open(F(CONFIG_FILENAME), "w");

    if (!file) {
      Serial.println("file open failed");
    } else {
      serializeJson(doc, file);
      file.close();
    }

    server.send(200, "text/plain", "config set");
  });

  server.on(UriBraces("/wget/{}"), []() {
    Serial.println(F(""));
    String filename = server.pathArg(0);

    File file = LittleFS.open(filename, "r");

    if (!file) {
      Serial.println("file open failed");
      server.send(200, F("text/text"), "does not exists");
    } else {
      JsonDocument doc;

      String content;
      Serial.println("Read from file: ");
      while (file.available()) { 
        char b=file.read();
        content+=b;
        Serial.write(b); 
      }
      file.close();

      JsonDocument contdoc;
      deserializeJson(contdoc, content);
      // Add values in the document
      doc[F("fileName")] = filename;
      doc[F("content")] = contdoc;

      String output;
      serializeJsonPretty(doc, output);
      server.send(200, F("text/json"), output);
    } 
  });

  server.on(UriBraces("/setcurrenttime/{}"), []() {
    Serial.println(F(""));
    String time = server.pathArg(0);
    server.send(200, "text/plain", "time: '" + time + "'");
  });

  server.on(UriRegex("^\\/users\\/([0-9]+)\\/devices\\/([0-9]+)$"), []() {
    Serial.println(F(""));
    String user = server.pathArg(0);
    String device = server.pathArg(1);
    server.send(200, "text/plain", "User: '" + user + "' and Device: '" + device + "'");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}
