/*
======================================================================
Interactive Table reservation Sign
======================================================================

This interactive table reservation sign should show the features of
an colored eink display. It is dynamically adjusting its content to
the time and status of the table.
The backend is currently a simple webserver where a name, start and
endtime can be configured, but in the future it can be modified to
be able to recieve data over mqtt or any other server-client connection.

This project was made for the "make a sign" competition of SeeedStudio.

Created by: S. Perrevoort
Date of last modification: 29.07.2026


********** Logic Diagram **********


======================================================================
                         [ SETUP FUNCTION (setup) ]
======================================================================
                                   |
                                   v
             [ Init: Hardware, Display & load saved data ]
                                   |
                                   v
         +---------------- ( WiFi known? ) ------------------+
         |                                                   |
      [ Yes ]                                              [ No ]
         |                                                   |
         |                                                   v
         |                                [ start AP-Mode (Table_Reservation_AP) ]
         |                                [ display shows IP & QR-Code           ]
         |                                                   |
         |<--------------------------------------------------+ (After the setup from web portal)
         v
 [ get time (NTP)  ]
         |
 [ start webserver ]
         |
         v
======================================================================
                        [ MAIN FUNCTION (loop) ]
======================================================================
         |
         +
         |                                                       
         v                                                        
[ Check Web interfaces ] --> (New Data?) -> [ save and force Update]
         |                                                       
         v                                                       
[ Check Button ] ------> (3 Sec. pressed?) -> [ reset WiFi & Restart ]
         |                                                      
         v                                                       
( 10 seconds elapsed OR forced screen update? ) ------[ No ] -----+           
         |                                                        |
       [ Yes ]                                                    |
         |                                                        |
         v                                                        |
[ check time & State (updateDisplayLogik) ]                       |
         |                                                        |
         +---> (time < starttime AND > 60 Min) ---> [ AVAILABLE ] |
         +---> (time < starttime AND <= 60 Min) --> [ UPCOMING ]  |
         +---> (time >= starttime AND < endtime) -> [ ACTIVE ]    |
         +---> (time >= endtime) -----------------> [ AVAILABLE ] |
         |                                                        |
         v                                                        |
( state changed or display update is forced? ) -------[ No ] -----+  
         |                                                        |
       [ Yes ]                                                    |
         |                                                        |
         v                                                        |
[ update eInk display ]                                           |
         |                                                        |
[ set eInk in Hibernate mode ]                                    |
         |                                                        |
         +--------------------------------------------------------+
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <qrcode_gen.h>

// --- E-Ink Display Setup (GxEPD2) ---
// JD79661 - GDEY029F52
// 296 width x 128 height

#include <GxEPD2_4C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h> // bold
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h> // bold
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h> // bold
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h> // bold

#define MAX_DISPLAY_BUFFER_SIZE 65536ul 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

// Configuration Constants
const char* WIFI_AP_NAME = "Table_Reservation_AP";

// SPI Pins für ESP32-S3 (HSPI)
const int EPD_CS = D7;  
const int EPD_DC = 10;  
const int EPD_RST = 38;  
const int EPD_BUSY = D3;  
const int SPI_SCK = D8;  
const int SPI_MOSI = D10; 
const int SPI_MISO = 8;  

GxEPD2_4C<GxEPD2_290c_GDEY029F52, MAX_HEIGHT(GxEPD2_290c_GDEY029F52)> display(GxEPD2_290c_GDEY029F52(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));

// --- Pushbutton Pins ---
const int BTN_WAKE_1 = 2; // to be defined
const int BTN_WAKE_2 = 3; // to be defined
const int BTN_CONFIG = 5; // reset WiFi

// --- Webserver ---
WebServer server(80);

// --- variables & memory ---
Preferences preferences;
String resName = "John Doe";
String startTime = "18:00";
String endTime = "20:00";

const char* ntpServer = "pool.ntp.org";
const char* tzInfo = "CET-1CEST,M3.5.0,M10.5.0/3"; 

// --- state machine for the display ---
enum DisplayState { STATE_UNKNOWN, STATE_AVAILABLE, STATE_UPCOMING, STATE_ACTIVE };
DisplayState currentDisplayState = STATE_UNKNOWN;
bool forceDisplayUpdate = true; // force update at the beginning

unsigned long lastTimeCheck = 0;
const unsigned long timeCheckInterval = 10000; // check every 10 seconds

// --- HELPER functions ---
int getMinutesFromMidnight(String timeStr) {
  int h, m;
  if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2) {
    return h * 60 + m;
  }
  return 0;
}

void drawCenteredText(const String& text, int yPos) {
  // 1. variables for the textrecognition
  int16_t tbx, tby;
  uint16_t tbw, tbh;

  // 2. Calculate Textboundaries
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

  // 3. calculate X-coordinate for the middle of the screen
  int centerX = (display.width() - tbw) / 2 - tbx;

  // 4. draw text
  display.setCursor(centerX, yPos);
  display.print(text);
}

void drawOutlinedText(String text, int x, int y, uint16_t fillingcolor) {
  // 1. Draw BLACK text with an offset
  display.setTextColor(GxEPD_BLACK);
  
  display.setCursor(x - 1, y); display.print(text);
  display.setCursor(x + 1, y); display.print(text);
  display.setCursor(x, y - 1); display.print(text); 
  display.setCursor(x, y + 1); display.print(text);
  
  // Optional: diagonal offset for a smoother transition
  display.setCursor(x - 1, y - 1); display.print(text);
  display.setCursor(x + 1, y - 1); display.print(text);
  display.setCursor(x - 1, y + 1); display.print(text);
  display.setCursor(x + 1, y + 1); display.print(text);

  // 2. Fill in the Text in the middle in the defined color
  display.setTextColor(fillingcolor);
  display.setCursor(x, y);
  display.print(text);
}

void drawQRCode(const char* url, int offsetX, int offsetY, int scale) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, url);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(offsetX + (x * scale), offsetY + (y * scale), scale, scale, GxEPD_BLACK);
      }
    }
  }
}

// --- HTML for the webinterface ---
const char* htmlForm = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Table-Reservation</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f9; color: #333; }
    .container { max-width: 400px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    h2 { text-align: center; color: #444; }
    label { font-weight: bold; display: block; margin-top: 15px; }
    input[type="text"], input[type="time"] { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
    input[type="submit"] { width: 100%; padding: 10px; margin-top: 20px; background-color: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
    input[type="submit"]:hover { background-color: #218838; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Reservation</h2>
    <form action="/save" method="POST">
      <label>Name:</label>
      <input type="text" name="name" value="%NAME%" maxlength="40" required>
      <label>Start-Time:</label>
      <input type="time" name="start" value="%START%" required>
      <label>End-Time:</label>
      <input type="time" name="end" value="%END%" required>
      <input type="submit" value="Save & Refresh">
    </form>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  String page = htmlForm;
  page.replace("%NAME%", resName);
  page.replace("%START%", startTime);
  page.replace("%END%", endTime);
  server.send(200, "text/html", page);
}

void handleSave() {
  if (server.hasArg("name")) resName = server.arg("name");
  if (server.hasArg("start")) startTime = server.arg("start");
  if (server.hasArg("end")) endTime = server.arg("end");

  // Save in Preferences
  preferences.begin("reservations", false);
  preferences.putString("name", resName);
  preferences.putString("start", startTime);
  preferences.putString("end", endTime);
  preferences.end();

  // force the display to be updated, because the data has been refreshed
  forceDisplayUpdate = true; 
  
  // send back to main-page
  server.sendHeader("Location", "/");
  server.send(303);
}



/*
======================================================================
                         [ SYSTEMSTART (ESP32) ]
======================================================================
*/

void setup() {
  Serial.begin(115200);
  
  pinMode(BTN_WAKE_1, INPUT_PULLUP);
  pinMode(BTN_WAKE_2, INPUT_PULLUP);
  pinMode(BTN_CONFIG, INPUT_PULLUP);

  // 1. Load memory
  preferences.begin("reservations", false);
  resName = preferences.getString("name", resName);
  startTime = preferences.getString("start", startTime);
  endTime = preferences.getString("end", endTime);
  preferences.end();

  // 2. initialize display
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, EPD_CS);
  display.init(115200);
  display.setRotation(1);

  // 3. WLAN configuration
  WiFiManager wm;
  
  // If GPIO 5 is pressed at start, reset WLAN configuration and start AP
  if (digitalRead(BTN_CONFIG) == LOW) {
    Serial.println("GPIO 5 pressed: Reset WLAN!");
    wm.resetSettings();
  }

  // A Callback (eine Funktion) for the WiFiManager to check if it is in AP-Mode:
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("AP-Mode started - Update Display...");
    
    // Init Display and show AP informations
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      
      display.setFont(&FreeSansBold9pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(10, 30);
      display.print("Connect to:");

      display.setFont(&FreeSans12pt7b);
      display.setCursor(10, 60);
      display.print(WIFI_AP_NAME);

      display.setFont(&FreeSansBold9pt7b);
      display.setCursor(10, 90);
      display.print("IP: ");
      
      display.setFont(&FreeSans12pt7b);
      display.setTextColor(GxEPD_RED);
      display.setCursor(10, 120);
      display.print("192.168.4.1");

      String qrText = String("WIFI:T:nopass;S:") + WIFI_AP_NAME + ";;";
      drawQRCode(qrText.c_str(), 230, 70, 2);

    } while (display.nextPage());
    display.hibernate();
  });

  // connects automatically or starts AP-Mode
  if (!wm.autoConnect(WIFI_AP_NAME)) {
    Serial.println("WLAN connection failed. Restart.");
    ESP.restart();
  }

  Serial.print("WLAN connected. Web-Interface can be reached unter: http://");
  Serial.println(WiFi.localIP());

  // get current time
  configTzTime(tzInfo, ntpServer);

  // 4. start Webserver 
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}




/*
======================================================================
                        [ HAUPTSCHLEIFE (loop) ]
======================================================================
*/

void loop() {
  // handle Webserver requests
  server.handleClient();

  // WLAN Reset while in run (Button 3 Seconds pressed)
  if (digitalRead(BTN_CONFIG) == LOW) {
    delay(3000); 
    if (digitalRead(BTN_CONFIG) == LOW) {
      Serial.println("WLAN Reset through button. Restart...");
      WiFiManager wm;
      wm.resetSettings();
      ESP.restart();
    }
  }

  // Check every 10 seconds if status is changed
  if (millis() - lastTimeCheck > timeCheckInterval || forceDisplayUpdate) {
    lastTimeCheck = millis();
    updateDisplayLogik();
  }
}


