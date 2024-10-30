// For Raspberry Pi Pico W
// WiFi is Read only
// UnagiDojyou https://unagidojyou.com

// ---------------user definition----------------------
#define WIFI_BUTTON 2  // GPIO of WiFi reset button

#define WIFI_LED LED_BUILTIN
#define SD_LED LED_BUILTIN

#define CP_BOARDNAME "shareable SDcardReader"
#define CP_HTMLTITLE "WiFi Setting"
// -----------------------------------------------------

#include <SdFat.h>
#include <Adafruit_TinyUSB.h>
#include <WiFi.h>
#include "CheckAndResponse.h"
#include <Captive_Portal_WiFi_connector.h>
const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int chipSelect = 5;
const int _SCK = 6;

CPWiFiConfigure CPWiFi(WIFI_BUTTON, WIFI_LED, Serial1);
WiFiServer server(80);
SdFat sd;
SdFile rootS;
SdFile fileS;
Adafruit_USBD_MSC msc;

bool USBworking = false;
bool WEBworking = false;

void startWiFi() {
  sprintf(CPWiFi.boardName, CP_BOARDNAME);
  sprintf(CPWiFi.htmlTitle, CP_HTMLTITLE);
  if (!CPWiFi.begin()) {
    Serial1.println("Fail to start Capitive_Portal_WiFi_configure");
    return;
  }
  WiFi.begin(CPWiFi.readSSID().c_str(), CPWiFi.readPASS().c_str());
  int count = 0;
  bool led = false;
  while (WiFi.status() != WL_CONNECTED) {
    if (count > 20) {
      Serial1.println("WiFi connect Fail. reboot.");
      rp2040.reboot();
    }
    delay(1000);  //1sencods
    if (CPWiFi.readButton()) {
      rp2040.reboot();
    }
    Serial1.print(".");
    if (!led) {
      digitalWrite(WIFI_LED, HIGH);
      led = true;
    } else {
      digitalWrite(WIFI_LED, LOW);
      led = false;
    }
    count++;
  }
  digitalWrite(WIFI_LED, LOW);
  led = false;
  Serial1.println("");
  Serial1.println("WiFi connected.");
  Serial1.print("IP is ");
  Serial1.println(WiFi.localIP());
}

void setup() {
  Serial1.begin(115200);
  Serial.end();

  pinMode(WIFI_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);

  Serial1.println("Mounting SDcard");
  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    Serial1.println("Mount Failed");
    uint8_t count = 0;
    while (count < 100) {
      digitalWrite(SD_LED, HIGH);
      delay(500);
      digitalWrite(SD_LED, LOW);
      delay(500);
      count++;
    }
    rp2040.reboot();
  }

  Serial1.println("Initializing MSC");
  msc.setID("Adafruit", "SD Card", "1.0");
  msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  msc.setUnitReady(true);
#if SD_FAT_VERSION >= 20000
  uint32_t block_count = sd.card()->sectorCount();
#else
  uint32_t block_count = sd.card()->cardSize();
#endif
  msc.setCapacity(block_count, 512);
  msc.begin();
}

void setup1() {
  delay(10);
  startWiFi();
  server.begin();  //start the server
  Serial1.print("\nHTTP server started at: ");
  Serial1.println(WiFi.localIP());
}

bool led = false;

void loop() {
  if (!led && (USBworking || WEBworking)) {
    digitalWrite(SD_LED, HIGH);
    led = true;
  } else if (led && !USBworking && !WEBworking) {
    digitalWrite(SD_LED, LOW);
    led = false;
  }
  delay(1);
}

void loop1() {
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
          for (int i = 0; i <= 1000; i++) {  //1000*10ms
            if (!USBworking) {
              sendHTTP(client, request);  //send HTTP response
              WEBworking = false;
              break;
            }
            if (i >= 1000) {
              Serial1.println("[Web]timeout in loop");
            }
            delay(10);
          }
          break;
        }
      }
    }
    WEBworking = false;
  }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  USBworking = true;
  if (WEBworking) {
    int i = 0;
    while (WEBworking) {
      if (i > 100) {
        Serial1.println("[USB]timeout");
        USBworking = false;
        return -1;
      }
      delay(10);
      i++;
    }
    delay(10);
  }

  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sd.card()->readSectors(lba, (uint8_t*)buffer, bufsize / 512);
#else
  rc = sd.card()->readBlocks(lba, (uint8_t*)buffer, bufsize / 512);
#endif
  USBworking = false;

  return rc ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  USBworking = true;
  if (WEBworking) {
    int i = 0;
    while (WEBworking) {
      if (i > 100) {
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
  rc = sd.card()->writeSectors(lba, buffer, bufsize / 512);
#else
  rc = sd.card()->writeBlocks(lba, buffer, bufsize / 512);
#endif
  return rc ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb(void) {
#if SD_FAT_VERSION >= 20000
  sd.card()->syncDevice();
#else
  sd.card()->syncBlocks();
#endif

  // clear file system's cache to force refresh
  sd.cacheClear();

  USBworking = false;
}
