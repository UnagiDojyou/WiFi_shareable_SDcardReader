#include "CheckAndResponse.h"

#ifdef UseWiFi
  #include <WiFi.h>
#else
  #include <Ethernet.h>
#endif

#include <SdFat.h>
#include <SPI.h>

#if USE_UTF8_LONG_NAMES == 0
#error Enable USE_UTF8_LONG_NAMES in \libraries\SdFat\src\SdFatConfig.h
#endif

//SdFat sd;
SdFile file;
SdFile root;

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

//defines for client
#define BUFFLEN 256        //length of the receive buffer
char buff[BUFFLEN];        //buffer
int count = 0;             //counter for the buffer
bool isBlankLine = false;  //if the last line is empty, the end of request
String path;

bool POSTflag = false;
#ifndef readonly
String cmdfilename;
String newfilename;
int ContentLength;
String errormessage;
bool Uploadflag = false;
String boundary;
bool Header = true;  //when it is false, countdown ContentLength
#endif

#ifndef readonly
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
#else
String processReequest(char c) {
  if(c == '\r'){ //if the code is CR, ignore it
    isBlankLine = false;
    return "";
  }
  else if(c == '\n') {  //if the code is NL, read the GET request
    buff[count]='\0'; //put null character at the end
    String request = buff; //convert to String
    isBlankLine = (count == 0); //and check if the line is empty
    count=0;
    return request;
  }
  else { //if the code is not control code, record it
    isBlankLine = false;
    if(count >= (BUFFLEN - 1) ) count=0; //if almost overflow, reset the counter
      buff[count++]=c; //add char at the end of buffer
    return "";
  }
}
#endif

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

#ifndef readonly
//check not use Forbidden characters
bool checkfilename(String checkstr) {                                                   //OK > true, invalid > false
  if (checkstr.indexOf("/") == checkstr.length() - 1 || checkstr.indexOf("/") == -1) {  //If "/" is used as the last character
    return checkstr.indexOf("\\") == -1 && checkstr.indexOf(":") == -1 && checkstr.indexOf("*") == -1 && checkstr.indexOf("?") == -1 && checkstr.indexOf("\"") == -1 && checkstr.indexOf("<") == -1 && checkstr.indexOf(">") == -1 && checkstr.indexOf("|") == -1;
  }
  return false;
}
#endif

bool CheckAndResponse(WiFiEthernetClient &client){
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
    return true;
  }
  return false;
}

#ifndef readonly
void process_request(WiFiEthernetClient& client, String request) {
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
      File file = file.open(newfilename, FILE_WRITE);
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
#else
void process_request(WiFiEthernetClient &client,String request){
  if(request.startsWith("GET")){
    //String path = request.substring(4).toInt();
    path = request;
    path.replace("GET ","");
    path.replace(" HTTP/1.1","");
    path = urlDecode(path); //decode%
  }
}
#endif

bool wait(){
  int i = 0;
  while(USBworking){
    //Serial1.println("[WEB]waiting");
    if(i > 1000){
      Serial1.println("[WEB]timeout in wait()");
#ifndef readonly
      errormessage = "USB is busy.";
#endif
      return false;
    }
    delay(10);
    i++;
  }
  return true;
}

bool sdexists(String sdpath){
  if(wait()){
    WEBworking = true;
    root.close();
    file.close();
    bool result = sd.exists(sdpath);
    WEBworking = false;
    return result;
  }else{
    return false; //timeout
  }
}

void sendHTTP(WiFiEthernetClient& client, const String& request) {
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
    if (sdexists(path)) {  //check path is exist
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

  #ifndef readonly
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

  #ifndef DisableRename
      client.println("Enter new name.<br>");
      client.println("<input type=\"text\" name=\"newfilename\">");
      client.println("<input type=\"submit\" name=\"rename\" value=\"rename\">");
  #endif
      
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
  #endif

      //Parent Directory
      String IPaddr = ipToString(WiFiEthernet.localIP());
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
      int len = path.length() + 1; //string to char
      char pathchar[len]; 
      path.toCharArray(pathchar, len);

      if(wait()){
        WEBworking = true;
        root.open(pathchar);
        WEBworking = false;
      }else{
        return;
      }
      //root.getName(char *name, size_t size)
      while(true){
        if(wait()){
          WEBworking = true;
          if(!file.openNext(&root, O_RDONLY)){
            file.close();
            root.close();
            WEBworking = false;
            break;
          }
          char filenamechar[200];
          file.getName(filenamechar,sizeof(filenamechar));
          String filename = filenamechar;
          int size;
          if (file.isDir()) {
            filename += "/";
          }else{
            size = file.fileSize();
          }
          file.close();
          WEBworking = false;
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
            client.println(kmgt(size));
          }
          client.println("</p>");
        }else{
          return; //timeout
        }
      }
      if(wait()){
        WEBworking = true;
        root.close();
        WEBworking = false;
      }else{
        return;
      }
      client.print("<hr>");
      client.print("<p>");
      client.print("Powered by ");
      client.print("<a href=\"https://github.com/UnagiDojyou/ArduinoIDE_SdFat_FAT32_Fileserver\">ArduinoIDE_SdFat_FAT32_Fileserver</a>");
      client.println("</p>");
      client.println("</body>");
      client.println("</html>");
    } else {  //Directory not Found
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
    }
  } else if (!POSTflag) {  //File
    if (sdexists(path)) {
      Serial1.print("File:");
      Serial1.println(path);
      String extension = getExtension(getFilename(path));

      int len = path.length() + 1; //string to char
      char pathchar[len]; 
      path.toCharArray(pathchar, len);

      unsigned long filesize;
      if(wait()){
        WEBworking = true;
        file.open(pathchar);
        filesize = file.fileSize();
        WEBworking = false;
      }else{
        return;
      }
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
      while(true){
        if(wait()){
          WEBworking = true;
          if(!file.available() || !client.connected()){
            file.close();
            WEBworking = false;
            break;
          }else{
            size_t bytesRead = file.read(buffe, bufferSize);
            WEBworking = false;
            client.write(buffe, bytesRead);
          }
        }else{
          return;
        }
      }
    } else {  //File not Found
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
    }
  }

  #ifndef readonly
  else if (POSTflag && request.indexOf("mkdir=mkdir") > -1) {  //mkdri
    //Serial1.println("mkdir");
    String newdir = path + cmdfilename;
    Serial1.print("mkdir:");
    Serial1.println(newdir);
    if (!sd.mkdir(newdir)) {
      errormessage = "Cannot make " + newdir;
    }
  } else if (POSTflag && request.indexOf("delete=delete") > -1) {  //delete
    //Serial1.println("delete");
    String rmpath = path + cmdfilename;
    Serial1.print("delete:");
    Serial1.println(rmpath);
    if (cmdfilename.endsWith("/")) {  //directory
      if (!sd.rmdir(rmpath.substring(0, rmpath.length() - 1))) {
        errormessage = "Cannot delete " + rmpath;
      }
    } else {  //file
      if (!SD.remove(rmpath)) {
        errormessage = "Cannot delete " + rmpath + "/";
      }
    }
  } 
  
  #ifndef DisableRename
  else if (POSTflag && request.indexOf("rename=rename") > -1) {  //rename
    Serial1.println("rename");
    String Newname = path + newfilename;
    String Oldname = path + cmdfilename;
    if (Newname.endsWith("/") ^ Oldname.endsWith("/")) {
      errormessage = "Either \"/\" is missing or surplus.";
    } else if (!SD.rename(Oldname, Newname)) {
      errormessage = "Cannot rename " + Oldname + "to" + "Newname";
    }
  }
  #endif

  if (POSTflag) {  //send page(must be exexute if(path.endsWith("/") && !POSTflag))
    POSTflag = false;
    sendHTTP(client, request);
  }
  #endif

  client.println("");
  client.stop();
  Serial1.println("Response finish");
  return;
}