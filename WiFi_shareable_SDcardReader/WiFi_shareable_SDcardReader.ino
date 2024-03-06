#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <SdFat.h>
#include <Adafruit_TinyUSB.h>

#define Blue_LED LED_BUILTIN  //BlueLED on DevKit(Need changes when using other boards)

WiFiServer server(80);

const char* ssid = "ssid";
const char* password = "password";

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

//defines for client
#define BUFFLEN 256        //length of the receive buffer
char buff[BUFFLEN];        //buffer
int count = 0;             //counter for the buffer
bool isBlankLine = false;  //if the last line is empty, the end of request
String path;

bool POSTflag = false;
String cmdfilename;
String newfilename;
int ContentLength;
String errormessage;
bool Uploadflag = false;
String boundary;
bool Header = true;  //when it is false, countdown ContentLength

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
  //while ( !Serial1 ) delay(10);   // wait for native usb

  Serial1.println("Adafruit TinyUSB Mass Storage SD Card example");

  Serial1.print("\nInitializing SD card ... ");
  Serial1.print("CS = "); Serial1.println(chipSelect);

  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);
  if ( !sd.begin(chipSelect, SD_SCK_MHZ(50)) )
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

void loop()
{
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
          sendHTTP(client, request);  //send HTTP response
          break;
        }
      }
    }
  }
}


void process_request(WiFiClient& client, String request) {
  if (request.startsWith("GET")) {
    //String path = request.substring(4).toInt();
    path = request;
    path.replace("GET ", "");
    path.replace(" HTTP/1.1", "");
    path = urlDecode(path);  //decode%
    POSTflag = false;
    Uploadflag = false;
  } else if (request.startsWith("POST")) {
    Serial1.print("post:");
    Serial1.println(request);
    path = request;
    path.replace("POST ", "");
    path.replace(" HTTP/1.1", "");
    path = urlDecode(path);  //decode%
    POSTflag = true;
    Uploadflag = false;
  } else if (request.startsWith("Content-Length") && POSTflag) {
    //Serial1.print("Content-Length:");
    //Serial1.println(request);
    request.replace("Content-Length: ", "");
    ContentLength = request.toInt();
    //Serial1.print("ContentLength=");
    //Serial1.println(ContentLength);
  } else if (request.startsWith("cmdfilename") && POSTflag) {
    //Serial1.print("cmd:");
    //Serial1.println(request);
    cmdfilename = request.substring(0, request.indexOf("&"));
    cmdfilename.replace("cmdfilename=", "");
    String secondIndex = request.substring(request.indexOf("&") + 1, request.lastIndexOf("&"));
    String thirdIndex = request.substring(request.lastIndexOf("&") + 1, request.length());
    if (secondIndex.startsWith("newfilename")) {
      newfilename = secondIndex;
    } else {
      newfilename = thirdIndex;
    }
    newfilename.replace("newfilename=", "");
    cmdfilename = urlDecode(cmdfilename);                              //decode
    newfilename = urlDecode(newfilename);                              //decode
    if (!checkfilename(cmdfilename) || !checkfilename(newfilename)) {  //OK > true, invalid > false
      POSTflag = false;
      errormessage = "Invalid characters are used.";
    } else if (cmdfilename.equals("")) {
      POSTflag = false;
      errormessage = "Prease enter file name to operate.";
    }
    //Serial1.print("cmdfilename:");
    //Serial1.println(cmdfilename);
    //Serial1.print("newfilename:");
    //Serial1.println(newfilename);
  } else if (request.startsWith("Content-Type") && POSTflag) {
    if (!Uploadflag) {  //serch boundary
      //serch "boundary="
      while (!request.startsWith("boundary=")) {
        request.remove(0, 1);  //remove first letter
        if (request.length() == 0) {
          break;
        }
      }
      if (request.length() != 0) {  //if boundary found
        request.replace("boundary=", "");
        boundary = request;
        //Serial1.print("boundary=");
        //Serial1.println(boundary);
        Uploadflag = true;
      } else {  //if boundary didn't find
        Uploadflag = false;
      }
    } else {  //upload file
      //ContentLength -= (request.length() + 1);
      String filename = path + request;
      //setting upload file
      newfilename = path + newfilename;
      File file = SD.open(newfilename, FILE_WRITE);
      while (client.available()) {
        char c = client.read();
        //Serial1.write(c);
        ContentLength -= 1;
        //Serial1.println(ContentLength);
        if (c == '\n') {
          break;
        }
      }
      //write file
      Serial1.println("Start writting uploadfile");
      //recive file with buffer
      const int BufferSize = 1024;
      //int finish_count = BufferSize + boundary.length() + 3 + 11; //use client check
      int finish_count = boundary.length() + 3 + 11;  //use client check
      //const int filesize = ContentLength - boundary.length() - 3 - 11; //use file check
      const size_t bufferSize = BufferSize;  //buffer size
      byte buffe[bufferSize];                //buffe
      int countb = 0;
      const int misslimit = 2048;
      int misscount = 0;

      while (ContentLength > finish_count && misscount < misslimit) {
        while (countb < BufferSize && ContentLength > finish_count) {
          if (client.available()) {
            buffe[countb] = client.read();
            countb += 1;
            ContentLength -= 1;
          } else {
            //Serial1.print(".");
            misscount += 1;
            delay(10);
            break;
          }
        }
        //Serial1.println(ContentLength);
        file.write(buffe, countb);
        countb = 0;
      }
      while (client.available() && misscount < misslimit) {  //write last boudary
        char c = client.read();
        Serial1.write(c);
      }
      file.close();
      Uploadflag = false;
      POSTflag = false;
      Header = true;

      if (!(misscount < misslimit)) {
        Serial1.println("To many packet loss.");
        client.println("HTTP/1.1 500 Internal Server Error");
        client.println("Connection: close");
        errormessage = "File will be broken. Delete & try again.";
        isBlankLine = false;
      } else {
        isBlankLine = true;
      }
    }
  } else if (request.endsWith(boundary) && Uploadflag) {  //start payload
    //Serial1.println("payload start");
    Serial1.println(request);
    //ContentLength -= (boundary.length()+3); //boundary,--,\n
  } else if (request.startsWith("Content-Disposition") && Uploadflag) {  //get upload filename
    //ContentLength -= (request.length() + 1);
    //serch filename=
    while (!request.startsWith("filename=")) {
      request.remove(0, 1);  //remove first letter
      if (request.length() == 0) {
        errormessage = "Not found filename.";
      }
    }
    request.replace("filename=\"", "");
    request.replace("\"", "");
    newfilename = request;
    Serial1.print("newfilename:");
    Serial1.println(newfilename);
  }
}

void sendHTTP(WiFiClient& client, const String& request) {
  //Serial1.println("");
  //Serial1.print("path:");
  //Serial1.println(path);
  if (path.equals("")) {
    path = "/";
  }
  if (path.endsWith("/") && !POSTflag) {  //readDirectory
    if (!path.equals("/")) {
      path.remove(path.length() - 1);  //delet last "/"
    }
    if (SD.exists(path)) {  //check path is exist
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("");
      client.println("");
      Serial1.print("Directroy:");
      Serial1.println(path);
      client.println("<!DOCTYPE html>");
      client.println("<html>");
      client.println("<head>");
      client.println("<title>SD Reader</title>");
      client.println("<meta charset=\"UTF-8\">");
      client.println("</head>");
      client.println("<body>");
      //display current path
      client.print("<h1>");
      client.print(path);
      if (!path.equals("/")) {  //add "/"
        client.print("/");
      }
      client.println("</h1>");
      //command
      client.print("<form action=\"");
      client.print(urlEncode(path));
      if (!path.equals("/")) {
        client.print("/");
      }
      client.println("\" method=\"POST\">");
      client.println("Enter File or Folder name to operate.<br>");
      client.println("<input type=\"text\" name=\"cmdfilename\">");
      client.println("<input type=\"submit\" name=\"mkdir\" value=\"mkdir\">");
      client.println("<input type=\"submit\" name=\"delete\" value=\"delete\"><br>");
      client.println("Enter new name.<br>");
      client.println("<input type=\"text\" name=\"newfilename\">");
      client.println("<input type=\"submit\" name=\"rename\" value=\"rename\">");
      client.println("</form>");
      //file upload
      client.print("<form action=\"");
      client.print(urlEncode(path));
      if (!path.equals("/")) {
        client.print("/");
      }
      client.println("\" method=\"POST\" enctype=\"multipart/form-data\">");
      client.println("Upload file. (Recommend < 20MB)<br>");
      client.println("<input type=\"file\" name=\"uploadfile\">");
      client.println("<input type=\"submit\" value=\"upload\">");
      client.println("</form>");
      //display Error message
      if (!errormessage.equals("")) {
        client.print("<p><font color=\"red\">");
        client.println(errormessage);
        client.print("</font></p>");
        errormessage = "";
      }
      //Parent Directory
      String IPaddr = ipToString(WiFi.localIP());
      if (!path.equals("/")) {
        client.print("<p><a href=\"");
        client.print("http://");
        client.print(IPaddr);
        client.print(urlEncode(path.substring(0, path.lastIndexOf("/") + 1)));  //Parent Directory path
        client.print("\" >");
        client.print("Parent Directory");
        client.print("</a>");
        client.println("</p>");
      }
      client.print("<hr>");
      //list up directory contents
      File dir = SD.open(path);
      while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
          break;
        }
        String filename = entry.name();
        if (entry.isDirectory()) {
          filename += "/";
        }
        String fileurl = "http://" + IPaddr + urlEncode(path);
        if (path.equals("/")) {
          fileurl += urlEncode(filename);
        } else {
          fileurl += "/" + urlEncode(filename);
        }
        client.print("<p>");
        client.print("<a href=\"");
        client.print(fileurl);
        client.print("\" >");
        client.print(filename);
        client.println("</a>");
        if (!filename.endsWith("/")) {  //display file size
          client.print(" ");
          client.println(kmgt(entry.size()));
        }
        client.println("</p>");
        entry.close();
      }
      dir.close();
      client.print("<hr>");
      client.print("<p>");
      client.print("Powered by ");
      client.print("<a href=\"https://github.com/UnagiDojyou/ArduinoIDE_SD_FAT32_Fileserver\">ArduinoIDE_SD_FAT32_Fileserver</a>");
      client.println("</p>");
      client.println("</body>");
      client.println("</html>");
    } else {  //Directory not Found
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
    }
  } else if (!POSTflag) {  //File
    if (SD.exists(path)) {
      Serial1.print("File:");
      Serial1.println(path);
      String extension = getExtension(getFilename(path));
      File file = SD.open(path);
      unsigned long filesize = file.size();
      client.println("HTTP/1.1 200 OK");
      client.print("Content-Length: ");
      client.println(filesize);
      if (extension.equals("other")) {
        client.print("Content-Disposition");
      } else {
        client.print("Content-Type: ");
        client.println(getType(extension));
      }
      client.println("Connection: close");
      client.println();
      //send file to client with 1024 buffer.
      const size_t bufferSize = 1024;  //buffer size
      byte buffe[bufferSize];          //buffe
      while (file.available()) {
        size_t bytesRead = file.read(buffe, bufferSize);
        client.write(buffe, bytesRead);
      }
      file.close();
    } else {  //File not Found
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
    }
  } else if (POSTflag && request.indexOf("mkdir=mkdir") > -1) {  //mkdri
    //Serial1.println("mkdir");
    String newdir = path + cmdfilename;
    Serial1.print("mkdir:");
    Serial1.println(newdir);
    if (!SD.mkdir(newdir)) {
      errormessage = "Cannot make " + newdir;
    }
  } else if (POSTflag && request.indexOf("delete=delete") > -1) {  //delete
    //Serial1.println("delete");
    String rmpath = path + cmdfilename;
    Serial1.print("delete:");
    Serial1.println(rmpath);
    if (cmdfilename.endsWith("/")) {  //directory
      if (!SD.rmdir(rmpath.substring(0, rmpath.length() - 1))) {
        errormessage = "Cannot delete " + rmpath;
      }
    } else {  //file
      if (!SD.remove(rmpath)) {
        errormessage = "Cannot delete " + rmpath + "/";
      }
    }
  } else if (POSTflag && request.indexOf("rename=rename") > -1) {  //rename
    Serial1.println("rename");
    String Newname = path + newfilename;
    String Oldname = path + cmdfilename;
    if (Newname.endsWith("/") ^ Oldname.endsWith("/")) {
      errormessage = "Either \"/\" is missing or surplus.";
    } else if (!SD.rename(Oldname, Newname)) {
      errormessage = "Cannot rename " + Oldname + "to" + "Newname";
    }
  }
  if (POSTflag) {  //send page(must be exexute if(path.endsWith("/") && !POSTflag))
    POSTflag = false;
    sendHTTP(client, request);
  }
  client.println("");
  client.stop();
  Serial1.println("Response finish");
  return;
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

// decode %URL
String urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";
  int str_len = str.length();

  for (int i = 0; i < str_len; i++) {
    if (str[i] == '%') {
      if (i + 2 < str_len) {
        temp[2] = str[i + 1];
        temp[3] = str[i + 2];
        decoded += (char)strtol(temp, NULL, 16);
        i += 2;
      }
    } else if (str[i] == '+') {
      decoded += ' ';
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}

// ecode %URL
String urlEncode(String str) {
  String encoded = "";
  char temp[] = "0x00";

  for (int i = 0; i < str.length(); i++) {
    char c = str[i];

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
      encoded += c;
      //} else if (c == ' ') {
      //  encoded += '+';
    } else {
      sprintf(temp, "%02X", c);
      encoded += '%' + String(temp);
    }
  }
  return encoded;
}

//put received char in buffer and check the GET command and empty line
String processReequest(char c) {
  if (c == '\r') {
    isBlankLine = false;
    return "";                       //if the code is CR, ignore it
  } else if (c == '\n') {            //if the code is NL, read the GET request
    buff[count] = '\0';              //put null character at the end
    String request = buff;           //convert to String
    if (!POSTflag && !Uploadflag) {  //nomarl Header
      isBlankLine = (count == 0);    //and check if the line is empty
    } else if (Uploadflag && Header) {
      Header = !(count == 0);
    } else if (!Header) {
      ContentLength -= count;  //count down ContenLength
    }
    //else if(!POSTflag && Uploadflag && count == 0){ //end header when fileupload
    //  isBlankLine = (count == 0);
    //}
    count = 0;
    return request;
  } else {  //if the code is not control code, record it
    isBlankLine = false;
    if (count >= (BUFFLEN - 1)) count = 0;      //if almost overflow, reset the counter
    buff[count++] = c;                          //add char at the end of buffer
    if (count == ContentLength && POSTflag) {   //POST Payload finished
      buff[count] = '\0';                       //put null character at the end
      String request = buff;                    //convert to String
      if (request.startsWith("cmdfilename")) {  //finish post payload
        Serial1.println("POST is finished");
        isBlankLine = true;
        count = 0;
        return request;
      }
    }
    return "";
  }
}

// https://qiita.com/dojyorin/items/ac56a1c2c620782d90a6
String ipToString(uint32_t ip) {
  String result = "";
  result += String((ip & 0xFF), 10);
  result += ".";
  result += String((ip & 0xFF00) >> 8, 10);
  result += ".";
  result += String((ip & 0xFF0000) >> 16, 10);
  result += ".";
  result += String((ip & 0xFF000000) >> 24, 10);
  return result;
}

String getExtension(const String& filename) {
  int dotIndex = filename.lastIndexOf('.');
  if (dotIndex == -1 || dotIndex == filename.length() - 1) {
    return "";
  }
  return filename.substring(dotIndex + 1);
}

String getFilename(const String& filename) {
  int dotIndex = filename.lastIndexOf('/');
  if (dotIndex == -1 || dotIndex == filename.length() - 1) {
    return "";
  }
  return filename.substring(dotIndex + 1);
}

//decide header by extension
String getType(const String& extension) {
  if (extension.equalsIgnoreCase("txt")) {
    return "text/plain";
  } else if (extension.equalsIgnoreCase("csv")) {
    return "text/csv";
  } else if (extension.equalsIgnoreCase("html") || extension.equalsIgnoreCase("htm")) {
    return "text/html";
  } else if (extension.equalsIgnoreCase("css")) {
    return "text/css";
  } else if (extension.equalsIgnoreCase("js")) {
    return "text/javascript";
  } else if (extension.equalsIgnoreCase("json")) {
    return "application/json";
  } else if (extension.equalsIgnoreCase("pdf")) {
    return "application/pdf";
  } else if (extension.equalsIgnoreCase("jpg") || extension.equalsIgnoreCase("jpeg")) {
    return "image/jpeg";
  } else if (extension.equalsIgnoreCase("png")) {
    return "image/png";
  } else if (extension.equalsIgnoreCase("gif")) {
    return "image/gif";
  } else if (extension.equalsIgnoreCase("svg")) {
    return "image/svg+xml";
  } else if (extension.equalsIgnoreCase("zip")) {
    return "application/zip";
  } else if (extension.equalsIgnoreCase("mpeg") || extension.equalsIgnoreCase("mpg")) {
    return "video/mpeg";
  } else {
    return "other";
  }
}

String kmgt(unsigned long bytes) {
  if (bytes < 1000) {
    return String(bytes) + "B";
  } else if (1000 <= bytes && bytes < 1000000) {
    return String(int(bytes / 1000)) + "KB";
  } else if (1000000 <= bytes && bytes < 1000000000) {
    return String(int(bytes / 1000000)) + "MB";
  } else{
    return String(int(bytes / 1000000000)) + "GB";
  }
}

//check not use Forbidden characters
bool checkfilename(String checkstr) {                                                   //OK > true, invalid > false
  if (checkstr.indexOf("/") == checkstr.length() - 1 || checkstr.indexOf("/") == -1) {  //If "/" is used as the last character
    return checkstr.indexOf("\\") == -1 && checkstr.indexOf(":") == -1 && checkstr.indexOf("*") == -1 && checkstr.indexOf("?") == -1 && checkstr.indexOf("\"") == -1 && checkstr.indexOf("<") == -1 && checkstr.indexOf(">") == -1 && checkstr.indexOf("|") == -1;
  }
  return false;
}