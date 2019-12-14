#include <M5Stack.h>
#include <WiFi.h>
#include "FS.h"
#include "FFat.h"

#define TEXT_HEIGHT 30 // Height of text to be printed and scrolled
#define YMAX 240 // Bottom of screen area

// The initial y coordinate of the top of the scrolling area
uint16_t yStart = 0;
// The initial y coordinate of the top of the bottom text line
uint16_t yDraw = YMAX - TEXT_HEIGHT;

// Keep track of the drawing x coordinate
uint16_t xPos = 0;

// For the byte we read from the serial port
byte data = 0;
char buffer[4192] = "";
unsigned int bufferPos = 0;

bool screen = true;
bool wifi = false;

char fileName[10] = "";
File fileLog;
WiFiServer server(23);
WiFiClient serverClient;

// ##############################################################################################
// Call this function to scroll the display one text line
// ##############################################################################################
int scroll_line() {
  int yTemp = yStart; // Store the old yStart, this is where we draw the next line
  // Use the record of line lengths to optimise the rectangle size we need to erase the top line
  // M5.Lcd.fillRect(0,yStart,blank[(yStart-TOP_FIXED_AREA)/TEXT_HEIGHT],TEXT_HEIGHT, TFT_BLACK);
  M5.Lcd.fillRect(0,yStart,320,TEXT_HEIGHT, TFT_BLACK);

  // Change the top of the scroll area
  yStart+=TEXT_HEIGHT;
  // The value must wrap around as the screen memory is a circular buffer
  // if (yStart >= YMAX - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA + (yStart - YMAX + BOT_FIXED_AREA);
  if (yStart >= YMAX) yStart = 0;
  // Now we can scroll the display
  scrollAddress(yStart);
  return yTemp;
}

// ##############################################################################################
// Setup a portion of the screen for vertical scrolling
// ##############################################################################################
// We are using a hardware feature of the display, so we can only scroll in portrait orientation
void setupScrollArea(uint16_t tfa, uint16_t bfa) {
  M5.Lcd.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  M5.Lcd.writedata(tfa >> 8);           // Top Fixed Area line count
  M5.Lcd.writedata(tfa);
  M5.Lcd.writedata((YMAX-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  M5.Lcd.writedata(YMAX-tfa-bfa);
  M5.Lcd.writedata(bfa >> 8);           // Bottom Fixed Area line count
  M5.Lcd.writedata(bfa);
}

// ##############################################################################################
// Setup the vertical scrolling start address pointer
// ##############################################################################################
void scrollAddress(uint16_t vsp) {
  M5.Lcd.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  M5.Lcd.writedata(vsp>>8);
  M5.Lcd.writedata(vsp);
}

int getFileCount() {
  int count = 0;
  File directory = FFat.open("/");
  File entry = directory.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      count++;
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.rewindDirectory();
  return count;
}

void setup() {    
  M5.begin();
  M5.Power.begin();

  M5.Lcd.setRotation(2);
  
  sprintf(fileName, "/%d.txt", getFileCount());
  fileLog = FFat.open(fileName, FILE_WRITE);
      
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  
  setupScrollArea(0, 0);
}

void loop() {
  if(M5.BtnA.wasPressed()) {
    if (fileLog) {
      fileLog.close();
    }
    if (wifi && serverClient && serverClient.connected()) {
      serverClient.stop();
      WiFi.softAPdisconnect(true);
    }
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.printf("Stopped !");
    while(true);
  }
  
  if(M5.BtnB.wasPressed()) {
    if (screen) {
      M5.Lcd.setBrightness(0);
    } else {
      M5.Lcd.setBrightness(100);
    }
    screen = !screen;
  }
  
  if(M5.BtnC.wasPressed()) {
    WiFi.softAP("SerialLogger");
    IPAddress IP = WiFi.softAPIP();
    server.begin();
    server.setNoDelay(true);
    wifi = true;
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.print(F("IP (23): ")); M5.Lcd.println(IP);
  }

  if (wifi) {
    if (server.hasClient()){
      if (!serverClient || !serverClient.connected()) {
        if (serverClient) {
          serverClient.stop();
        }
        serverClient = server.available();
        M5.Lcd.println(F("New connection !"));
      } else {
        server.available().stop();
      }
    }
  }

  while (Serial.available()) {
    data = Serial.read();
    buffer[bufferPos++] = data;
    if (data == '\r') {
      buffer[bufferPos] = '\0';
      bufferPos = 0;
      
      if (fileLog) {
        fileLog.println(buffer);
      }
      if (wifi && serverClient && serverClient.connected()) {
        serverClient.print(buffer);
      }
    }
    if (data == '\r' || xPos > 311) {
      xPos = 0;
      yDraw = scroll_line();
    }
    if (data > 31 && data < 128) {
      xPos += M5.Lcd.drawChar(data,xPos,yDraw,2);
    }
  }
  
  M5.update();
}
