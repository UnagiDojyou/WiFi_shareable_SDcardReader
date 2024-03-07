#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <SdFat.h>
#include <Adafruit_TinyUSB.h>
#include "CheckAndResponse.h"

#define Blue_LED LED_BUILTIN  //BlueLED on DevKit(Need changes when using other boards)

WiFiServer server(80);

const char* ssid = "Write your SSID";
const char* password = "Write your Password";

const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int chipSelect = 5;
const int _SCK = 6;

// File system on SD Card
SdFat sd;

SdFile root;
SdFile file;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Set to true when PC write to flash
bool fs_changed;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "SD Card", "1.0");

  // Set read write callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Still initialize MSC but tell usb stack that MSC is not ready to read/write
  // If we don't initialize, board will be enumerated as CDC only
  usb_msc.setUnitReady(false);
  usb_msc.begin();

  Serial1.begin(115200);
  //while ( !Serial ) delay(10);   // wait for native usb

  Serial1.println("Adafruit TinyUSB Mass Storage SD Card example");

  Serial1.print("\nInitializing SD card ... ");
  Serial1.print("CS = "); Serial1.println(chipSelect);

  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);
  
  if ( !sd.begin(chipSelect, SD_SCK_MHZ(50)) || !SD.begin(chipSelect))
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

  fs_changed = true; // to print contents initially

  // connect to WiFi
  Serial1.println("try to connect WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial1.print(".");
    /*if (digitalRead(Blue_LED)) {
      digitalWrite(Blue_LED, HIGH);
    } else {
      digitalWrite(Blue_LED, LOW);
    }*/
  }
  Serial1.println("");
  Serial1.println("WiFi connected.");
  server.begin();  //start the server
  Serial1.print("\nHTTP server started at: ");
  Serial1.println(WiFi.localIP());

}

void loop() {
  if ( fs_changed )
  {
    root.open("/");
    Serial1.println("SD contents:");

    // Open next file in root.
    // Warning, openNext starts at the current directory position
    // so a rewind of the directory may be required.
    while ( file.openNext(&root, O_RDONLY) )
    {
      file.printFileSize(&Serial1);
      Serial1.write(' ');
      file.printName(&Serial1);
      if ( file.isDir() )
      {
        // Indicate a directory.
        Serial1.write('/');
      }
      Serial1.println();
      file.close();
    }

    root.close();

    Serial1.println();

    fs_changed = false;
    delay(1000); // refresh every 0.5 second
  }
}

void loop1() {
  WiFiClient client = server.available();
  CheckAndResponse(client);
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  bool rc;

#if SD_FAT_VERSION >= 20000
  rc = sd.card()->readSectors(lba, (uint8_t*) buffer, bufsize/512);
#else
  rc = sd.card()->readBlocks(lba, (uint8_t*) buffer, bufsize/512);
#endif

  return rc ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  bool rc;

  digitalWrite(LED_BUILTIN, HIGH);

#if SD_FAT_VERSION >= 20000
  rc = sd.card()->writeSectors(lba, buffer, bufsize/512);
#else
  rc = sd.card()->writeBlocks(lba, buffer, bufsize/512);
#endif

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

  fs_changed = true;

  digitalWrite(LED_BUILTIN, LOW);
}
