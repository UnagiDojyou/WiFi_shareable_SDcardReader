// ---------------user definition----------------------
/*
  What is USB_REFRESH?
USB_REFRESH disconnects the storage from the host for a moment.
When USB is connected to Windows and you change files via WiFi, the changes are not applied to the Windows side.
Becase of Windows has cache of file list. When the storage is reconnected, Windows refresh cache. So, USB_REFRESH is need.

If neither SCSI_REFRESH nor USB_REFRESH is selected, no USB_REFRESH is occur.
*/
// #define SCSI_REFRESH  // (recommend) Temporarily disconnect the SCSI storage.
// #define USB_REFRESH  // Temporarily disable the USB.

#define REFRESH_TIME_LENGTH 2000  // length of disconnection time (ms)

#define WIFI_BUTTON 2  // GPIO of WiFi reset button

#define WIFI_LED LED_BUILTIN
#define SD_LED LED_BUILTIN

#define CP_BOARDNAME "shareableSDReader"
#define CP_HTMLTITLE "WiFi Setting"
// -----------------------------------------------------

#if defined SCSI_REFRESH && defined USB_REFRESH
#error Please select only one of SCSI_REFRESH and USB_REFRESH.
#endif

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
Adafruit_USBD_MSC msc;
SdFat sdUSB;
SdFat sd;

uint16_t updateCount = 0;
bool POSTflagd = false;

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  updateCount = 0;
  bool rc;
  rc = sdUSB.card()->writeSectors(lba, buffer, bufsize / 512);
  return rc ? bufsize : -1;
}

int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  updateCount = 0;
  bool rc;
  rc = sdUSB.card()->readSectors(lba, (uint8_t*)buffer, bufsize / 512);
  return rc ? bufsize : -1;
}

void msc_flush_cb(void) {
  updateCount = 0;
  sdUSB.card()->syncDevice();
  // sdUSB.cacheClear();
}

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
  // write what you want to do using WiFi
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
  if (!sdUSB.begin(chipSelect, SD_SCK_MHZ(50)) || !sd.begin(chipSelect, SD_SCK_MHZ(50))) {
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
  uint32_t block_count = sdUSB.card()->sectorCount();
  msc.setCapacity(block_count, 512);
  msc.begin();
}

void setup1() {
  startWiFi();
  server.begin();  //start the server
  Serial1.print("\nHTTP server started at: ");
  Serial1.println(WiFi.localIP());
}

bool wait_media_present_accessed = false;
bool mediaPresent = true;

void loop() {
}

void loop1() {
  if (CPWiFi.readButton()) {
    rp2040.reboot();
  }
  WiFiClient client = server.available();
  CheckAndResponse(client);

#ifdef SCSI_REFRESH
  if (updateCount >= UINT16_MAX && POSTflagd) {
    if (!wait_media_present_accessed) {                   // first
      media_present_accessed = false;                     // when accessed, automaticly true.
      wait_media_present_accessed = true;                 // flag of waiting media_present_accessed to be high
    } else if (media_present_accessed && mediaPresent) {  // second
      msc.setUnitReady(false);
      mediaPresent = false;
      Serial1.println("reset USB");
      media_present_accessed = false;
    } else if (media_present_accessed && !mediaPresent) {  // third
      delay(REFRESH_TIME_LENGTH);
      msc.setUnitReady(true);
      mediaPresent = true;
      wait_media_present_accessed = false;
      POSTflagd = false;
      updateCount = 0;
    }
  } else if (updateCount < UINT16_MAX) {
    updateCount++;
    if (POSTflagd && !mediaPresent) {
      media_present_accessed = true;
      wait_media_present_accessed = false;
    }
  }
#endif
#ifdef USB_REFRESH
  if ((updateCount >= UINT16_MAX) && POSTflagd) {
    if (!wait_media_present_accessed) {                   // first
      media_present_accessed = false;                     // when accessed, automaticly true.
      wait_media_present_accessed = true;                 // flag of waiting media_present_accessed to be high
    } else if (media_present_accessed && mediaPresent) {  // second
      msc.end();
      delay(REFRESH_TIME_LENGTH);
      msc.begin();
    }
  } else if (updateCount < UINT16_MAX) {
    updateCount++;
    if (POSTflagd && !mediaPresent) {
      media_present_accessed = true;
      wait_media_present_accessed = false;
    }
  }
#endif
}
