#include <WiFi.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_TinyUSB.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <LittleFS.h>
#include "CheckAndResponse.h"

const char* wifi_config = "/WiFi.txt";
bool softap = true;
bool led = false;
int APcount = 0;
bool startupAP = false;

char baseMacChr[18] = {0};

WebServer softserver(80);
WiFiServer server(80);
WiFiClient client;
DNSServer dnsServer;

const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int chipSelect = 5;
const int _SCK = 6;

// File system on SD Card
SdFat sd;

SdFile rootS;
SdFile fileS;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

bool USBworking = false;
bool WEBworking = false;

void handleRoot() {
  char htmlForm[500];
  snprintf(htmlForm,500,"<!DOCTYPE html>\
<html>\
<head>\
<title>Raspberry Pi Pico W WiFi Setting</title>\
<meta name=\"viewport\" content=\"width=300\">\
</head>\
<body>\
<h2>Raspberry Pi Pico W WiFi Setting</h2>\
<p>MACaddress<br>%s</p>\
<form action=\"/submit\" method=\"POST\">\
SSID<br>\
<input type=\"text\" name=\"SSID\" required\">\<br>\
Password<br>\
<input type=\"text\" name=\"Password\" required\">\<br>\
<input type=\"submit\" value=\"send\">\
</form>\
</body>\
</html>",baseMacChr);
  softserver.send(200, "text/html", htmlForm);
}

void handleSubmit() {
  if (softserver.hasArg("SSID") && softserver.hasArg("Password")) {
    String staSSID = softserver.arg("SSID");
    String staPassword = softserver.arg("Password");
    Serial1.println(staSSID);
    Serial1.println(staPassword);

    char htmlForm[500];
    snprintf(htmlForm,500,"<!DOCTYPE html>\
<html>\
<head>\
<title>Raspberry Pi Pico W WiFi Setting</title>\
<meta name=\"viewport\" content=\"width=300\">\
</head>\
<body>\
<h2>Raspberry Pi Pico W WiFi Setting</h2>\
<p>MACaddress<br>%s</p>\
<p>Attempts to connect to %s after 10 seconds.</p>\
<p>When led blinks at 1 second intervals, Raspberry Pi Pico W is trying to connect to WiFi.<br>\
If it continues for a long time, press and hold the reset button for more than 5 seconds and setting WiFi again.</p>\
</body>\
</html>",baseMacChr,staSSID.c_str());
    softserver.send(200, "text/html", htmlForm);

    File file = LittleFS.open(wifi_config, "w");
    file.println(staSSID); //SSID
    file.println(staPassword); //password
    file.close();
    delay(10000); //10seconds
    rp2040.reboot();

  } else {
    softserver.send(200, "text/plain", "Message not received");
  }
}

void handleNotFound() {
  String IPaddr = ipToString(softserver.client().localIP());
  softserver.sendHeader("Location", String("http://") + IPaddr, true);
  softserver.send(302, "text/plain", "");
  softserver.client().stop();
}

void setup()
{
  Serial1.begin(115200);
  Serial.end();
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "SD Card", "1.0");

  // Set read write callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Still initialize MSC but tell usb stack that MSC is not ready to read/write
  // If we don't initialize, board will be enumerated as CDC only
  usb_msc.setUnitReady(false);
  usb_msc.begin();

  Serial1.println("WiFi shareable SDcard Reader");

  Serial1.print("\nInitializing SD card ... ");
  Serial1.print("CS = "); Serial1.println(chipSelect);

  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);
  
  if ( !sd.begin(chipSelect, SD_SCK_MHZ(50)))
  {
    Serial1.println("initialization failed. Things to check:");
    Serial1.println("* is a card inserted?");
    Serial1.println("* is your wiring correct?");
    Serial1.println("* did you change the chipSelect pin to match your shield or module?");
    while (1) delay(1);
  }

  // Size in blocks (512 bytes)
#if SD_FAT_VERSION >= 20000
  uint32_t block_count = sd.card()->sectorCount();
#else
  uint32_t block_count = sd.card()->cardSize();
#endif

  Serial1.print("Volume size (MB):  ");
  Serial1.println((block_count/2) / 1024);

  // Set disk size, SD block size is always 512
  usb_msc.setCapacity(block_count, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);
}

void setup1(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  if (!LittleFS.begin()) {
    Serial1.println("SPIFFS failed, or not present");
    return;
  }
  if(LittleFS.exists(wifi_config)){
    //Setup staAP mode
    softap = false;
    Serial1.println("try connect to WiFi");
    File file = LittleFS.open(wifi_config, "r");
    String staSSID = file.readStringUntil('\n');
    String staPassword = "";
    if (file.available()){
      staPassword = file.readStringUntil('\n');
    }
    file.close();
    staSSID.replace("\r", "");
    staPassword.replace("\r", "");
    Serial1.println(staSSID);
    Serial1.println(staPassword);
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000); //1sencods
      readbutton();
      Serial1.print(".");
      if(!led && !USBworking){
        digitalWrite(LED_BUILTIN, HIGH);
        led = true;
      }else if(!USBworking){
        digitalWrite(LED_BUILTIN, LOW);
        led = false;
      }
    }
    digitalWrite(LED_BUILTIN, LOW);
    Serial1.println("");
    Serial1.println("WiFi connected.");
    server.begin();  //start the server
    Serial1.print("\nHTTP server started at: ");
    Serial1.println(WiFi.localIP());
    startupAP = true;
  }else{
    //Setup SoftAP mode
    softap = true;
    Serial1.println("start AP");
    // Get MAC address for WiFi station
    uint8_t baseMac[6];
    WiFi.macAddress(baseMac);
    sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    char softSSID[10] = {0};
    sprintf(softSSID, "RaspberryPiPiciW-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);

    Serial1.print("MAC: ");
    Serial1.println(baseMacChr);
    Serial1.print("SSID: ");
    Serial1.println(softSSID);

    WiFi.softAP(softSSID);
    IPAddress IP = WiFi.softAPIP();
    Serial1.print("AP IP address: ");
    Serial1.println(IP);

    softserver.on("/", HTTP_GET, handleRoot);
    softserver.on("/submit", HTTP_POST, handleSubmit);
    softserver.onNotFound(handleNotFound);

    dnsServer.start(53, "*", IP);
    softserver.begin();
  }
}

void loop() {
  if(startupAP){
    if(!led && (USBworking || WEBworking)){
      digitalWrite(LED_BUILTIN, HIGH);
      led = true;
    }
    else if(led && !USBworking && !WEBworking){
      digitalWrite(LED_BUILTIN, LOW);
      led = false;
    }
    delay(10);
  }
}

void readbutton(){
  if(BOOTSEL){
    APcount = 0;
    while(BOOTSEL){
      delay(10);
      APcount++;
      if(APcount > 500){ //5seconds
        Serial1.println("Erase wifi_config and reboot");
        LittleFS.remove(wifi_config);
        rp2040.reboot();
        break;
      }
    }
  }
  return;
}

void loop1() {
  if(softap){
    softserver.handleClient();
    dnsServer.processNextRequest();
    APcount++;
    delay(1);
    if(APcount > 300){//about 0.3seconds
      if(!led && !USBworking){
        digitalWrite(LED_BUILTIN, HIGH);
        led = true;
      }
      else if(!USBworking){
        digitalWrite(LED_BUILTIN, LOW);
        led = false;
      }
      APcount = 0;
    }
  }else{
    readbutton();
    WiFiClient client = server.available();
    if (client) {
      Serial1.println("new client");
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          //Serial1.write(c); //Output Request from client
          String request = processReequest(c);
          if (!request.equals("")) {
            process_request(client, request);
          }
          //if the line is blank, the request has ended.
          if (isBlankLine) {
            for(int i = 0; i <= 1000; i++){ //1000*10ms
              if (!USBworking){
                sendHTTP(client, request);  //send HTTP response
                WEBworking = false;
                break;
              }
              if(i >= 1000){
                Serial1.println("[Web]timeout in loop");
              }
              delay(10);
            }
            break;
          }
        }
      }
    }
    WEBworking = false;
  }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  USBworking = true;
  if(WEBworking){
    int i = 0;
    while(WEBworking){
      if(i > 100){
        Serial1.println("[USB]timeout");
        USBworking = false;
        return -1; //実行されるとフリーズ
      }
      delay(10);
      i++;
    }
    delay(10);
  }

  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sd.card()->readSectors(lba, (uint8_t*) buffer, bufsize/512);
#else
  rc = sd.card()->readBlocks(lba, (uint8_t*) buffer, bufsize/512);
#endif

  //Serial1.print("R");
  //Serial1.println(rc);

  USBworking = false;

  return rc ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  USBworking = true;
  if(WEBworking){
    int i = 0;
    while(WEBworking){
      if(i > 100){
        Serial1.println("[USB]timeout");
        USBworking = false;
        return -1;
      }
      delay(10);
      i++;
    }
  }
  
  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sd.card()->writeSectors(lba, buffer, bufsize/512);
#else
  rc = sd.card()->writeBlocks(lba, buffer, bufsize/512);
#endif

  //Serial1.print("W");
  //Serial1.println(rc);
  
  return rc ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
#if SD_FAT_VERSION >= 20000
  sd.card()->syncDevice();
#else
  sd.card()->syncBlocks();
#endif

  // clear file system's cache to force refresh
  sd.cacheClear();

  USBworking = false;
}
