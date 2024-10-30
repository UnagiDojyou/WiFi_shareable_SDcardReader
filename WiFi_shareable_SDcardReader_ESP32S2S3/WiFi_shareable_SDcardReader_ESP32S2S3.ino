// For ESP32 S2 and ESP32 S3
// UnagiDojyou https://unagidojyou.com

// ---------------user definition----------------------
/*
  What is USB_REFRESH?
USB_REFRESH disconnects the storage from the host for a moment.
When USB is connected to Windows and you change files via WiFi, the changes are not applied to the Windows side.
Becase of Windows has cache of file list. When the storage is reconnected, Windows refresh cache. So, USB_REFRESH is need.

If neither SCSI_REFRESH nor USB_REFRESH is selected, no USB_REFRESH is occur.
*/
#define SCSI_REFRESH // (recommend) Temporarily disconnect the SCSI storage.
// #define USB_REFRESH  // Temporarily disable the USB.

#define REFRESH_TIME_LENGTH 2000  // length of disconnection time (ms)

#define WIFI_BUTTON 0  // GPIO of WiFi reset button

#define WIFI_LED 48
#define SD_LED 48

#define CP_BOARDNAME "shareable SDcardReader"
#define CP_HTMLTITLE "WiFi Setting"
// -----------------------------------------------------

#if !SOC_USB_OTG_SUPPORTED || ARDUINO_USB_MODE
#error Device does not support USB_OTG or native USB CDC/JTAG is selected
#endif

#if defined SCSI_REFRESH && defined USB_REFRESH
#error Please select only one of SCSI_REFRESH and USB_REFRESH.
#endif

#include <SD.h>
#include <USB.h>
#include "USBMSC.h"
#include <WiFi.h>
#include "CheckAndResponse.h"
#include <esp_system.h>
#include <Captive_Portal_WiFi_connector.h>
#include <LittleFS.h>

CPWiFiConfigure CPWiFi(WIFI_BUTTON, WIFI_LED, Serial);
WiFiServer server(80);
USBMSC msc;

uint16_t updateCount = 0;
bool POSTflagd = false;

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  updateCount = 0;
  uint32_t secSize = SD.sectorSize();
  if (!secSize) {
    return false;  // disk error
  }
  log_v("Write lba: %ld\toffset: %ld\tbufsize: %ld", lba, offset, bufsize);
  for (int x = 0; x < bufsize / secSize; x++) {
    uint8_t blkbuffer[secSize];
    memcpy(blkbuffer, (uint8_t *)buffer + secSize * x, secSize);
    if (!SD.writeRAW(blkbuffer, lba + x)) {
      return false;
    }
  }
  return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  updateCount = 0;
  uint32_t secSize = SD.sectorSize();
  if (!secSize) {
    return false;  // disk error
  }
  log_v("Read lba: %ld\toffset: %ld\tbufsize: %ld\tsector: %lu", lba, offset, bufsize, secSize);
  for (int x = 0; x < bufsize / secSize; x++) {
    if (!SD.readRAW((uint8_t *)buffer + (x * secSize), lba + x)) {
      return false;  // outside of volume boundary
    }
  }
  return bufsize;
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
  msc.vendorID("ESP32");
  msc.productID("USB_MSC");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.mediaPresent(true);
  msc.begin(SD.numSectors(), SD.sectorSize());
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
  digitalWrite(WIFI_LED, LOW);
  led = false;
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP is ");
  Serial.println(WiFi.localIP());
}

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Serial");

  pinMode(WIFI_LED, OUTPUT);
  pinMode(SD_LED, OUTPUT);

  Serial.println("Mounting SDcard");
  if (!SD.begin()) {
    Serial.println("Mount Failed");
    uint8_t count = 0;
    while (count < 100) {
      digitalWrite(SD_LED, HIGH);
      delay(500);
      digitalWrite(SD_LED, LOW);
      delay(500);
      count++;
    }
    ESP.restart();
  }

  xTaskCreate(startUSB, "startUSB", 4096, NULL, 2, NULL);

  startWiFi();
  server.begin();  //start the server
  Serial.print("\nHTTP server started at: ");
  Serial.println(WiFi.localIP());
}

bool wait_media_present_accessed = false;
bool mediaPresent = true;

void loop() {
  if (CPWiFi.readButton()) {
    ESP.restart();
  }
  WiFiClient client = server.available();
  CheckAndResponse(client);

#ifdef SCSI_REFRESH
  if (updateCount >= UINT16_MAX && POSTflagd) {
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
      ESP.restart();
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
