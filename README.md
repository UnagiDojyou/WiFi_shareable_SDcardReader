# WiFi_shareable_SDcardReader
Enable Raspberry Pi Pico W and ESP32 S2, S3 to function as a USB SD card reader. At the same time, you can also view and edit the content on the SD card from other devices via a browser using WiFi.

## Feature
* Supports simultaneous access via browser and USB.
* Enter WiFi SSID and password using Captive Portal.
* When files are edited via a browser, automatically reload USB.

## Example of use
* Uploading data to 3D printer.
* Viewing oscilloscope data.
* Old Digital Photo Frame

## Limitation
* Slow access.
* Simultaneous access to the same file.
* Sort function in browser.
* Raspberry Pi Pico W only : File/Folder editing on the browser.

## How to install
* Use the code with Arduino IDE.
* install library→[ArduinoIDE_Captive_Portal_WiFi_configure](https://github.com/UnagiDojyou/ArduinoIDE_Captive_Portal_WiFi_configure)
* Initialize the SD card to FAT32 in advance.
* Connect the SD card properly
### ESP32 S2,S3
* Board have to be 'ESP32S2 Dev Module' or 'ESP32S3 Dev Module'.
* Select tool→Flash Size→Choose the one that suits your board.
* Select tool→Partition Scheme→Select the one that included SPIFFS and appropriate Flash Size.
* Select tool→USB Stack→"USB-OTG (TinyUSB)"
* "Events Run On" and "Arduino Runs On" must be same Core.

### Raspberry Pi Pico W
* Select tool→Flash Size→"2MB (Sketch: 1984KB, FS 64KB)"
* Select tool→USB Stack→"Adafruit TinyUSB"

## How to use
### How to set up WiFi.
1. Open WiFi settings on any device.
2. Look for the SSID shareableSDReader-XXXXXX.<br>
If it cannot be found, the FS settings may not be configured correctly. Please reset or review your Arduino IDE settings.
3. Connect to boardName-XXXXXX.
4. Captive Portal will display the WiFi input fields for SSID and password.
5. Remember your MAC address. It will be used later to identify the IP address.
6. Enter the SSID and password of the WiFi to which you want to connect board.
7. Press "send" button.
8. Wait 30 seconds.
9. Check LED status. If the slow blinking continues, please do "Reset WiFi Setting" and review your WiFi settings.
10. Access that IP address in your browser.

### Reset WiFi Setting
1. Press and hold the WIFI_BUTTON for more than 5 seconds.<br>
2. Release the WIFI_BUTTON when the LED lights up.

## WIFI_LED status
### Blinks quickly
WiFi settings have not been completed.
### Blinks slowly
Try to connect to WiFi.<br>
If the slow blinking continues, perform a "Reset WiFi Setting" and review your WiFi settings.

## Related projects
This WiFi_shareable_SDcardReader_RaspberryPiPicoW is made up of
* [ArduinoIDE_SD_FAT32_Fileserver](https://github.com/UnagiDojyou/ArduinoIDE_SD_FAT32_Fileserver)
* [ArduinoIDE_Captive_Portal_WiFi_configure](https://github.com/UnagiDojyou/ArduinoIDE_Captive_Portal_WiFi_configure)
* [Adafruit_TinyUSB_Arduino msc_sdfat.ino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/e2918652339aa3986f66b32c0a592c1aa72aabc8/examples/MassStorage/msc_sdfat/msc_sdfat.ino)