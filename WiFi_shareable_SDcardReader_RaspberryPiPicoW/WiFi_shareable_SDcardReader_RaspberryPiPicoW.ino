// For Raspberry Pi Pico W
// UnagiDojyou https://unagidojyou.com

// ---------------user definition----------------------
/*
  What is USB_REFRESH?
USB_REFRESH disconnects the storage from the host for a moment.
When USBmsc(Mass Storage Class) is connected to Windows and you change files via WiFi, the changes are not applied to the Windows side.
This is because Windows has a cache of file lists. When the storage is reconnected, Windows refresh cache. So USB_REFRESH is need.

If neither SCSI_REFRESH nor USB_REFRESH is selected, USB_REFRESH don't occur.
*/
// #define SCSI_REFRESH // (recommend) Temporarily disconnect the SCSI storage.
// #define USB_REFRESH  // Temporarily disable the USB.

#define REFRESH_TIME_LENGTH 2000  // length of disconnection time (ms)

#define RESET_COUNT UINT16_MAX

#define WIFI_BUTTON 2  // GPIO of WiFi reset button

#define WIFI_LED LED_BUILTIN
#define SD_LED LED_BUILTIN

#define CP_BOARDNAME "shareableSDReader"
#define CP_HTMLTITLE "WiFi Setting"

const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int chipSelect = 5;
const int _SCK = 6;
// -----------------------------------------------------

//#include "board.h"
#include <SdFat.h>
//#include "Adafruit_TinyUSB_Arduino/src/Adafruit_TinyUSB.h"
#include <Adafruit_TinyUSB.h>
#include <WiFi.h>
#include "CheckAndResponse.h"
#include <Captive_Portal_WiFi_connector.h>
#include <FreeRTOS.h>
#include <task.h>

CPWiFiConfigure CPWiFi(WIFI_BUTTON, WIFI_LED, Serial1);
WiFiServer server(80);
SdFat sdUSB;
SdFat sd;
Adafruit_USBD_MSC msc;

uint16_t updateCount = 0;
bool POSTflagd = false;
bool needFlush = false;

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize) {
  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sdUSB.card()->readSectors(lba, (uint8_t*) buffer, bufsize/512);
#else
  rc = sdUSB.card()->readBlocks(lba, (uint8_t*) buffer, bufsize/512);
#endif

  return rc ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sdUSB.card()->writeSectors(lba, buffer, bufsize/512);
#else
  rc = sdUSB.card()->writeBlocks(lba, buffer, bufsize/512);
#endif

  return rc ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void) {
#if SD_FAT_VERSION >= 20000
  sdUSB.card()->syncDevice();
#else
  sdUSB.card()->syncBlocks();
#endif

  // clear file system's cache to force refresh
  //sdUSB.cacheClear();  
}

void startUSB(void *pvParameters) {
#ifdef USB_REFRESH
  delay(REFRESH_TIME_LENGTH);
#endif
  Serial1.println("Initializing MSC");
  // Initialize USB metadata and callbacks for MSC (Mass Storage Class)
  msc.setID("Adafruit", "SD Card", "1.0");
  msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  msc.setUnitReady(true);
  uint32_t block_count = sdUSB.card()->sectorCount();
  msc.setCapacity(block_count, 512);
  msc.begin();
  POSTflagd = false;
  vTaskDelete(NULL);
}

void startWiFi() {
  sprintf(CPWiFi.boardName, CP_BOARDNAME);
  sprintf(CPWiFi.htmlTitle, CP_HTMLTITLE);
  if (!CPWiFi.begin()) {
    Serial1.println("Fail to start Capitive_Portal_WiFi_configure");
    while (true) {}
  }
  WiFi.begin(CPWiFi.readSSID().c_str(), CPWiFi.readPASS().c_str());
  int count = 0;
  bool led = false;
  while (WiFi.status() != WL_CONNECTED) {
    if (count > 40) {
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
  Serial1.println("");
  Serial1.println("WiFi connected.");
  Serial1.print("IP is ");
  Serial1.println(WiFi.localIP());
  digitalWrite(WIFI_LED, LOW);
  led = false;
}

void setup() {
  Serial1.begin(115200);
  Serial.end();

  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);

  pinMode(WIFI_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);
  Serial1.println("Mounting SDcard");
  if (!sdUSB.begin(chipSelect, SD_SCK_MHZ(50)) || !sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    Serial1.println("Mount Failed");
    uint8_t count = 0;
    while (count < 100) {
      digitalWrite(SD_LED, HIGH);
      delay(100);
      digitalWrite(SD_LED, LOW);
      delay(100);
      count++;
    }
    rp2040.reboot();
  }
  digitalWrite(WIFI_LED, LOW);
  xTaskCreate(startUSB, "startUSB", 4096, NULL, 2, NULL);
  POSTflagd = false;

  startWiFi();
  server.begin();  //start the server
}

bool wait_media_present_accessed = false;
bool mediaPresent = true;

void loop() {
  if (CPWiFi.readButton()) {
    rp2040.restart();
  }
  //WiFiClient client = server.available();
  WiFiClient client = server.accept();

  if (client.available()) {
    updateCount = 0;
    digitalWrite(SD_LED, HIGH);
  }
  CheckAndResponse(client);
  if (updateCount > 0) {
    digitalWrite(SD_LED, LOW);
  }
  // if (updateCount >= RESET_COUNT && needFlush) {
  //   sdUSB.card()->syncDevice();
  //   // sdUSB.cacheClear();
  //   needFlush = false;
  // }
#if !defined SCSI_REFRESH && !defined USB_REFRESH
  else if (updateCount < RESET_COUNT) {
    updateCount++;
  }
#endif

#ifdef SCSI_REFRESH
  if (updateCount >= RESET_COUNT && POSTflagd) {
    if (!wait_media_present_accessed) {                   // first
      media_present_accessed = false;                     // when accessed, automaticly true.
      wait_media_present_accessed = true;                 // flag of waiting media_present_accessed to be high
    } else if (media_present_accessed && mediaPresent) {  // second
      msc.mediaPresent(false);
      mediaPresent = false;
      Serial1.println("reset USB");
      media_present_accessed = false;
    } else if (media_present_accessed && !mediaPresent) {  // third
      delay(REFRESH_TIME_LENGTH);
      msc.mediaPresent(true);
      mediaPresent = true;
      wait_media_present_accessed = false;
      POSTflagd = false;
      updateCount = 0;
    }
  } else if (updateCount < RESET_COUNT) {
    updateCount++;
    if (POSTflagd && !mediaPresent) {
      media_present_accessed = true;
      wait_media_present_accessed = false;
    }
  }
#endif
#ifdef USB_REFRESH
  if ((updateCount >= RESET_COUNT) && POSTflagd) {
    sdUSB.card()->syncDevice();
    // delay(1000);
    // ESP.restart();
    if (!wait_media_present_accessed) {                   // first
      media_present_accessed = false;                     // when accessed, automaticly true.
      wait_media_present_accessed = true;                 // flag of waiting media_present_accessed to be high
    } else if (media_present_accessed && mediaPresent) {  // second
      ESP.restart();
    }
  } else if (updateCount < RESET_COUNT) {
    updateCount++;
    }
  }
#endif
}