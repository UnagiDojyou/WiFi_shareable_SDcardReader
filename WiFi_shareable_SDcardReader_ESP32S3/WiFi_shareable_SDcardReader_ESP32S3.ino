#if !SOC_USB_OTG_SUPPORTED || ARDUINO_USB_MODE
#error Device does not support USB_OTG or native USB CDC/JTAG is selected
#endif

#include <SD.h>
#include <sd_diskio.h>
#include <USB.h>
#include "USBMSC.h"
#include <WiFi.h>
#include "CheckAndResponse.h"
#include <esp_system.h>
#include <Captive_Portal_WiFi_connector.h>
#include <LittleFS.h>
#include <FS.h>
#include <esp32-hal-tinyusb.h>

#define BOOT_SW 0

CPWiFiConfigure CPWiFi(BOOT_SW, LED_BUILTIN, Serial);
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
      case ARDUINO_USB_STARTED_EVENT: Serial.println("USB PLUGGED"); break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT: Serial.println("USB RESUMED"); break;

      default: break;
    }
  }
}

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Serial");

  Serial.println("Mounting SDcard");
  if (!SD.begin()) {
    Serial.println("Mount Failed");
    return;
  }

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
  USB.begin();
  USB.onEvent(usbEventCallback);
}

bool first = true;

void startWiFi() {
  sprintf(CPWiFi.boardName, "ESP32");
  sprintf(CPWiFi.htmlTitle, "Capitive_Portal_WiFi_configure sample code on ESP32");
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
      digitalWrite(LED_BUILTIN, HIGH);
      led = true;
    } else {
      digitalWrite(LED_BUILTIN, LOW);
      led = false;
    }
    count++;
  }
  digitalWrite(LED_BUILTIN, LOW);
  led = false;
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP is ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (first) {
    startWiFi();
    server.begin();  //start the server
    Serial.print("\nHTTP server started at: ");
    Serial.println(WiFi.localIP());
    first = false;
  }
  if (CPWiFi.readButton()) {
    ESP.restart();
  }
  WiFiClient client = server.available();
  CheckAndResponse(client);
  if (POSTflagd && (updateCount >= UINT16_MAX)) {
    msc.mediaPresent(false);
    media_present_accessed = false;
    Serial.println("reset USB");
    while (!media_present_accessed) {
      delay(1);
    }
    msc.mediaPresent(true);
    POSTflagd = false;
    updateCount = 0;
  } else if (updateCount < UINT16_MAX) {
    updateCount++;
  }
}
