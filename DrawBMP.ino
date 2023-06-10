/*
 * Draw Bitmap images on ST7735 TFT display using Arduino.
 * The Arduino loads BMP format images from SD card and display
 *   them on the ST7735 TFT.
 * Reference: Adafruit spitftbitmap.ino example.
 * This is a free software with NO WARRANTY.
 * https://simple-circuit.com/
 * https://simple-circuit.com/draw-bmp-images-arduino-sd-card-st7735/
 * modified 10/06/2023
 */

#include <Adafruit_GFX.h>  // include Adafruit graphics library
//#include <Adafruit_ST7735.h> // include Adafruit ST7735 display library
#include <Adafruit_ST7789.h>  // include Adafruit ST7789 display library
#include <SPI.h>              // include Arduino SPI library
#include <SD.h>               // include Arduino SD library
#include <Ch376msc.h>

#define DEBUG_BUFFER_SIZE 150

// define ST7735 TFT display connections
#define TFT_RST -1  // reset line (optional, pass -1 if not used)
#define TFT_CS 4    // chip select line
#define TFT_DC 2    // data/command line
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define BUFFPIXEL 85
//#define BUFFHEADER 16


#define USB_CS  16    // chip select line
#define USB_INT 17    // interrupt line


//..............................................................................................................................
// Connect to SPI port: MISO, MOSI, SCK
// Connect MISO : ESP32 19 -> CH376 MISO D7
// Connect MOSI : ESP32 23 -> CH376 MOSI D6
// Connect SCK  : ESP32 18 -> CH376 SCK  D5
// Connect CS   : ESP32 16 -> CH376 SS   D3
// Connect INT  : ESP32 17 -> CH376 INT

// use this if no other device are attached to SPI port(MISO pin used as interrupt)
//Ch376msc flashDrive(10); // chipSelect

//If the SPI port shared with other devices e.g SD card, display, etc. remove from comment the code below and put the code above in a comment
//Ch376msc flashDrive(USB_CS, USB_INT, SPI_SCK_MHZ(4) );  // chipSelect, interrupt pin
Ch376msc flashDrive(USB_CS, USB_INT);  // chipSelect, interrupt pin

// initialize Adafruit ST7735 TFT library
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// initialize Adafruit ST7789 TFT library
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);  // Create an instant for TFT

void setup(void) {
  Serial.begin(115200);
  SPI.begin();
  
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(USB_CS, OUTPUT);
  digitalWrite(USB_CS, HIGH);

  Serial.println("Initializing Flash USB Drive...");
  flashDrive.init();
  Serial.print(F("Firmware version: "));
  Serial.println(flashDrive.getChipVer());
  Serial.println("OK!");

  // initialize ST7789 TFT display
  tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor( 0, 0 );
  tft.println("TFT Display initiated");
                 
}

void loop() {

  bmpDraw("PICS/TEN128.BMP", 0, 0);  // draw it
  while (1);  

}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.

void bmpDraw(String path , uint8_t x, uint16_t y) {

  uint8_t USBbuffer[3 * BUFFPIXEL];     // pixel buffer (R+G+B per pixel)
  uint8_t Headbuffer[1];                  // 
  uint16_t buffidx = sizeof(USBbuffer);  // Current position in USBbuffer

  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t startTime = millis();

  String    dir ;                                           // Directory from full path
  int       inx ;                                           // For splitting full path
  String    filename ;                                      // filename from full path
  char*     p ;                                             // Pointer to filename
  
  if ((x >= tft.width()) || (y >= tft.height())) return;

  w = 128;                                                  // size is fixed at the moment
  h = 128;                                                  // size is fixed at the moment
  if ((x + w - 1) >= tft.width()) w = tft.width() - x;
  if ((y + h - 1) >= tft.height()) h = tft.height() - y;

  // Set TFT address window to clipped image bounds
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  tft.endWrite();

  inx = path.lastIndexOf ( "/" ) ;                          // Search for the last slash
  dir = path.substring ( 0, inx ) ;                         // Get the directory
  filename = path.substring ( inx + 1 ) ;                   // Get the filename
  p = (char*)path.c_str() + 1 ;                             // Point to filename (after the slash)
  flashDrive.cd( dir.c_str(), false ) ;                     // Change to this directory
  flashDrive.setFileName ( filename.c_str() ) ;             // Set filename
  
  Serial.println("Opening file... ");

  if(!(flashDrive.openFile() == ANSW_USB_INT_SUCCESS)){               //open the file
    
    while (1);  
  }

  Serial.print("Opened file: ");
  Serial.print(dir);
  Serial.print("/");
  Serial.println(filename);

  //Parse BMP header
  Serial.println("Reading Headers...");
  
  for (int i = 0; i <= 54; i++) {
    flashDrive.readRaw(Headbuffer, sizeof(Headbuffer));
    //Serial.println(Headbuffer[0]);
  }
  
  Serial.println("Beginning loop");

  for (row = 0; row < h; row++) {  // For each scanline...

    //Serial.println("Beginning col loop");
    for (col = 0; col < w; col++) {  // For each pixel...
      // Time to read more pixel data?
      //Serial.print("buffidx: ");
      //Serial.println(buffidx);
      //Serial.print("sizeof(USBbuffer): ");
      //Serial.println(sizeof(USBbuffer));

      if (buffidx >= sizeof(USBbuffer)) { // Indeed

        flashDrive.readRaw(USBbuffer, sizeof(USBbuffer));

        buffidx = 0;                                  // Set index to beginning

      }
  
      // Convert pixel from BMP to TFT format, push to display
      g = USBbuffer[buffidx++];
      r = USBbuffer[buffidx++];
      b = USBbuffer[buffidx++];

      tft.pushColor(tft.color565(r, g, b));

    }  // end pixel
  }    // end scanline

  tft.endWrite();

  flashDrive.closeFile();

  Serial.print(F("Loaded in "));
  Serial.print(millis() - startTime);
  Serial.println(" ms");

  Serial.println("Reading Done!");
}
