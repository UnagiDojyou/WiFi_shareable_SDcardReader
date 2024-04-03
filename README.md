# WiFi_shareable_SDcardReader_RaspberryPiPicoW
Make Raspberry Pi Pico W work as a USB SD card reader. In addition, you can also view and edit the content on the SD card from other devices via a browser using WiFi.

## Feature
* Supports simultaneous access via browser and USB.
* Enter WiFi SSID and password using Captive Portal.

## Example of use
* Uploading data to 3D printer.
* Viewing oscilloscope data.

## Limitation
* Slow access.
* Simultaneous access to the same file.
* Sort function in browser.
* File/Fold edit on browser. (Under construction. Coming soon.)

## How to install
* Use the code with Arduino IDE.
* The Arduino core uses [Earle F. Philhower, III's Arduino-Pico](https://github.com/earlephilhower/arduino-pico).
* Select tool→Flash Size→"2MB (Sketch: 1984KB, FS 64KB)"
* Select tool→USB Stack→"Adafruit TinyUSB"
* Initialize the SD card to FAT32 in advance.
* Connect the SD card properly

## How to use
### How to set up WiFi.
1. Open WiFi settings on any device.
2. Look for the SSID RaspberryPiPiciW-XXXXXX.<br>
If it cannot be found, there is a high possibility that the FS settings are not configured correctly. Please reset or review the Arduino IDE settings.
3. Connect to RaspberryPiPiciW-XXXXXX.
4. Captive Portal will display the WiFi input fields for SSID and password.
5. Remember your MAC address. It will be used later to identify the IP address.
6. Enter the SSID and password of the WiFi to which you want to connect Raspberry Pi Pico W.
7. Press "send" button.
8. wait 30 second.
9. Check LED status. If the slow blinking continues, please do "Reset WiFi Setting" and review your WiFi settings.
10. Find IP address from MAC address using ARP.<br>
Windows(powershell) : `Get-NetNeighbor -LinkLayerAddress "XX-XX-XX-XX-XX-XX"`<br>
Linux&MAC : `arp -a | grep XX:XX:XX:XX:XX:XX`<br>
11. Access that IP address in your browser.

### Reset WiFi Setting
Please try reboot (unplugging the power) first.
Press and hold the BOOTSEL button for more than 5 seconds.<br>
After that, please reboot the Raspberry Pi Pico (reconnect the power supply).

## LED status
### Blink quickly
WiFi settings have not been completed.
### Blink slowly
Try to connect to WiFi.<br>
If the slow blinking continues, please do "Reset WiFi Setting" and review your WiFi settings.
### Blink random
Shows access.
## Related projects
This WiFi_shareable_SDcardReader_RaspberryPiPicoW is made up of
* [ArduinoIDE_SD_FAT32_Fileserver](https://github.com/UnagiDojyou/ArduinoIDE_SD_FAT32_Fileserver)
* Captive portal WiFi connector
* [Adafruit_TinyUSB_Arduino msc_sdfat.ino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/e2918652339aa3986f66b32c0a592c1aa72aabc8/examples/MassStorage/msc_sdfat/msc_sdfat.ino)
