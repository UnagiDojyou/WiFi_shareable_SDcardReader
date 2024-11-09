// For ESP32 S2 and ESP32 S3
// UnagiDojyou https://unagidojyou.com

// ---------------user definition----------------------
/*
  SPI Pin (ESP32 S3)
CS-10
MOSI-11
MISO-13
SCLK-12

  SPI Pin (ESP32 S2)
CS-34
MOSI-35
MISO-37
SCLK-36

  What is USB_REFRESH?
USB_REFRESH disconnects the storage from the host for a moment.
When USBmsc(Mass Storage Class) is connected to Windows and you change files via WiFi, the changes are not applied to the Windows side.
This is because Windows has a cache of file lists. When the storage is reconnected, Windows refresh cache. So USB_REFRESH is need.

If neither SCSI_REFRESH nor USB_REFRESH is selected, USB_REFRESH don't occur.
*/
#define SCSI_REFRESH // (recommend) Temporarily disconnect the SCSI storage.
// #define USB_REFRESH  // Temporarily disable the USB.

#define REFRESH_TIME_LENGTH 2000  // length of disconnection time (ms)

#define RESET_COUNT UINT16_MAX

#define WIFI_BUTTON 0  // GPIO of WiFi reset button

#define WIFI_LED LED_BUILTIN
#define SD_LED LED_BUILTIN

#define CP_BOARDNAME "shareableSDReader"
#define CP_HTMLTITLE "WiFi Setting"
// -----------------------------------------------------

#if CONFIG_IDF_TARGET_ESP32S3
int sck = 12;
int miso = 13;
int mosi = 11;
int cs = 10;
#endif

#if CONFIG_IDF_TARGET_ESP32S2
int sck = 36;
int miso = 37;
int mosi = 35;
int cs = 34;
#endif

#if !SOC_USB_OTG_SUPPORTED || ARDUINO_USB_MODE
#error Device does not support USB_OTG or native USB CDC/JTAG is selected
#endif

#if defined SCSI_REFRESH && defined USB_REFRESH
#error Please select only one of SCSI_REFRESH and USB_REFRESH.
#endif

#include <SdFat.h>
#include <USB.h>
#include "USBMSC.h"
#include <WiFi.h>
#include "CheckAndResponse.h"
#include <esp_system.h>
#include <Captive_Portal_WiFi_connector.h>
#include <LittleFS.h>

CPWiFiConfigure CPWiFi(WIFI_BUTTON, WIFI_LED, Serial);
WiFiServer server(80);
SdFat sdUSB;
SdFat sd;
USBMSC msc;

uint16_t updateCount = 0;
bool POSTflagd = false;
bool needFlush = false;

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  updateCount = 0;
  needFlush = true;
  log_v("Write lba: %ld\toffset: %ld\tbufsize: %ld", lba, offset, bufsize);
  bool rc;
  rc = sdUSB.card()->writeSectors(lba, buffer, bufsize / 512);
  // return bufsize;
  return rc ? bufsize : -1;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  updateCount = 0;
  log_v("Read lba: %ld\toffset: %ld\tbufsize: %ld\tsector: %lu", lba, offset, bufsize, secSize);
  bool rc;
  rc = sdUSB.card()->readSectors(lba, (uint8_t *)buffer, bufsize / 512);
  return rc ? bufsize : -1;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  log_i("Start/Stop power: %u\tstart: %d\teject: %d", power_condition, start, load_eject);
  return true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT:
        Serial.println("USB PLUGGED");
        POSTflagd = false;
        break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT: Serial.println("USB RESUMED"); break;

      default: break;
    }
  }
}

void startUSB(void *pvParameters) {
#ifdef USB_REFRESH
  delay(REFRESH_TIME_LENGTH);
#endif
  Serial.println("Initializing MSC");
  // Initialize USB metadata and callbacks for MSC (Mass Storage Class)
  USB.usbPower(500);     // 500mA
  USB.usbAttributes(0);  // no Self powered
  msc.vendorID("ESP32");
  msc.productID("USB_MSC");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.mediaPresent(true);
  msc.begin(sdUSB.card()->sectorCount(), 512);
  POSTflagd = false;
  USB.begin();
  USB.onEvent(usbEventCallback);
  vTaskDelete(NULL);
}

void startWiFi() {
  sprintf(CPWiFi.boardName, CP_BOARDNAME);
  sprintf(CPWiFi.htmlTitle, CP_HTMLTITLE);
  if (!LittleFS.begin(true)) {
    Serial.println("Fail to start LittleFS");
  }
  if (!CPWiFi.begin()) {
    Serial.println("Fail to start Capitive_Portal_WiFi_configure");
    while (true) {}
  }
  WiFi.begin(CPWiFi.readSSID().c_str(), CPWiFi.readPASS().c_str());
  int count = 0;
  bool led = false;
  while (WiFi.status() != WL_CONNECTED) {
    if (count > 40) {
      Serial.println("WiFi connect Fail. reboot.");
      ESP.restart();
    }
    delay(1000);  //1sencods
    if (CPWiFi.readButton()) {
      ESP.restart();
    }
    Serial.print(".");
    if (!led) {
      digitalWrite(WIFI_LED, HIGH);
      led = true;
    } else {
      digitalWrite(WIFI_LED, LOW);
      led = false;
    }
    count++;
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP is ");
  Serial.println(WiFi.localIP());
  digitalWrite(WIFI_LED, LOW);
  led = false;
}

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Serial");

  pinMode(WIFI_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);
  SPI.begin(sck, miso, mosi, cs);
  Serial.println("Mounting SDcard");
  if (!sdUSB.begin() || !sd.begin()) {
    Serial.println("Mount Failed");
    uint8_t count = 0;
    while (count < 100) {
      digitalWrite(SD_LED, HIGH);
      delay(100);
      digitalWrite(SD_LED, LOW);
      delay(100);
      count++;
    }
    ESP.restart();
  }
  digitalWrite(WIFI_LED, LOW);
  xTaskCreate(startUSB, "startUSB", 4096, NULL, 2, NULL);

  startWiFi();
  server.begin();  //start the server
}

bool wait_media_present_accessed = false;
bool mediaPresent = true;

void loop() {
  if (CPWiFi.readButton()) {
    ESP.restart();
  }
  WiFiClient client = server.available();
  if (client.available()) {
    updateCount = 0;
    digitalWrite(SD_LED, HIGH);
  }
  CheckAndResponse(client);
  if (updateCount > 0) {
    digitalWrite(SD_LED, LOW);
  }
  if (updateCount >= RESET_COUNT && needFlush) {
    sdUSB.card()->syncDevice();
    // sdUSB.cacheClear();
    needFlush = false;
  }
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
      Serial.println("reset USB");
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
