/*
 * Draw Bitmap images on ST7735 TFT display using Arduino.
 * The Arduino loads BMP format images from SD card and display
 *   them on the ST7735 TFT.
 * Reference: Adafruit spitftbitmap.ino example.
 * This is a free software with NO WARRANTY.
 * https://simple-circuit.com/
 * https://simple-circuit.com/draw-bmp-images-arduino-sd-card-st7735/
 */

#include <Adafruit_GFX.h>  // include Adafruit graphics library
//#include <Adafruit_ST7735.h> // include Adafruit ST7735 display library
#include <Adafruit_ST7789.h>  // include Adafruit ST7789 display library
#include <SPI.h>              // include Arduino SPI library
#include <SD.h>               // include Arduino SD library
#include <Ch376msc.h>

#define DEBUG_BUFFER_SIZE 150
int               DEBUG = 1 ;                            // Debug on/off
SemaphoreHandle_t SPIsem = NULL ;                        // For exclusive SPI usage
TaskHandle_t      maintask ;                             // Taskhandle for main task

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


#define button 2  // button pin

// initialize Adafruit ST7735 TFT library
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// initialize Adafruit ST7789 TFT library
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);  // Create an instant for TFT
void setup(void) {
  Serial.begin(115200);
  SPI.begin();
  
  pinMode(TFT_CS, OUTPUT);
  pinMode(USB_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(USB_CS, HIGH);
  pinMode(button, INPUT_PULLUP);

  Serial.println("Initializing Flash USB Drive...");
  flashDrive.init();
  Serial.print(F("Firmware version: "));
  Serial.println(flashDrive.getChipVer());
  Serial.println("OK!");

  maintask = xTaskGetCurrentTaskHandle() ;               // My taskhandle
  SPIsem = xSemaphoreCreateMutex(); 

  // initialize ST7735S TFT display
  //tft.initR(INITR_BLACKTAB);
  tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor( 0, 0 );
  tft.println("TFT Display initiated");

  // Serial.print("Initializing SD card...");
  // if (!SD.begin()) {
  //   Serial.println("failed!");
  //   while(1);  // stay here
  // }
  // Serial.println("OK!");

  // File root = SD.open("/");  // open SD card main root
  // printDirectory(root, 0);   // print all files names and sizes
  // root.close();              // close the opened root

  //flashDrive.setFileName("PICS/HP128.BMP");  //set the file name
  //flashDrive.openFile();                     //open the file


                 
}

void loop() {

  bmpDraw("PICS/TEN128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/MS128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/MIRE2.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/HP128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/SIR128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/SQUID128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/RING128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/SKULL128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/BAG128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/HELM128.BMP", 0, 0);  // draw it
  delay(500);

  bmpDraw("PICS/RD128.BMP", 0, 0);  // draw it
  delay(500);


  //bmpDraw("PICS/TERM128.BMP", 0, 0);  // draw it
  //bmpDraw("PICS/TEST_UPS.BMP", 0, 0);  // draw it
  //bmpDraw("PICS/MIRE.BMP", 0, 0);  // draw it

  //File root = SD.open("/");  // open SD card main root

}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

void bmpDraw(String path , uint8_t x, uint16_t y) {

  File bmpFile;
  int bmpWidth, bmpHeight;             // W+H in pixels
  uint8_t bmpDepth;                    // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;             // Start of image data in file
  uint32_t rowSize;                    // Not always = bmpWidth; may have padding
  uint8_t USBbuffer[3 * BUFFPIXEL];     // pixel buffer (R+G+B per pixel)
  uint8_t Headbuffer[1];                  // 
  uint16_t buffidx = sizeof(USBbuffer);  // Current position in USBbuffer

  boolean goodBmp = false;             // Set to true on valid header parse
  boolean flip = true;                 // BMP is stored bottom-to-top
  boolean readMore = true;
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0, startTime = millis();

  String    dir ;                                           // Directory from full path
  int       inx ;                                           // For splitting full path
  String    filename ;                                      // filename from full path
  char*     p ;                                             // Pointer to filename
  
  if ((x >= tft.width()) || (y >= tft.height())) return;

  w = 128;
  h = 128;
  if ((x + w - 1) >= tft.width()) w = tft.width() - x;
  if ((y + h - 1) >= tft.height()) h = tft.height() - y;

  // Set TFT address window to clipped image bounds
  //claimSPI ( "tft" );
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  tft.endWrite();
  //releaseSPI(); 

  // Open requested file on SD card
  // if ((bmpFile = SD.open(filename)) == NULL) {
  //   Serial.print(F("File not found"));
  //   return;
  // }

  // byte adatBuffer[255];
  // flashDrive.setFileName(filename);  //set the file name
  // flashDrive.openFile();             //open the file
  //                                    //read data from flash drive until we reach EOF
  // while (!flashDrive.getEOF()) {
  //   flashDrive.readRaw(adatBuffer, sizeof(adatBuffer));
  //   for (int f = 0; f < sizeof(adatBuffer); f++) {  // here i print the raw image data to the serial port
  //     Serial.write(adatBuffer[f]);
  //   }
  // }
  // flashDrive.closeFile();
  // Serial.println("reading Done!");

  //claimSPI ( "usb" );

  inx = path.lastIndexOf ( "/" ) ;                          // Search for the last slash
  dir = path.substring ( 0, inx ) ;                         // Get the directory
  filename = path.substring ( inx + 1 ) ;                   // Get the filename
  p = (char*)path.c_str() + 1 ;                             // Point to filename (after the slash)
  flashDrive.cd( dir.c_str(), false ) ;                     // Change to this directory
  flashDrive.setFileName ( filename.c_str() ) ;             // Set filename
  
  Serial.println("Opening file... ");

  if(!(flashDrive.openFile() == ANSW_USB_INT_SUCCESS)){               //open the file
    //Serial.println("OK");
    //flashDrive.moveCursor(CURSOREND);     //if the file exist, move the "virtual" cursor at end of the file, with CURSORBEGIN we actually rewrite our old file
    //flashDrive.moveCursor(flashDrive.getFileSize()); // is almost the same as CURSOREND, because we put our cursor at end of the file
    while (1);  
  }
  //flashDrive.openFile();             //open the file
  //releaseSPI(); 

  Serial.print("Opened file: ");
  Serial.print(dir);
  Serial.print("/");
  Serial.println(filename);

  // tft.setCursor( 0, 232 );
  // tft.print("                                      ");
  // tft.setCursor( 0, 232 );
  // tft.print("Reading: " );
  // tft.println(dir);
  // tft.println("/");
  // tft.println(filename);

  //Parse BMP header
  Serial.println("Reading Headers...");
  
  for (int i = 0; i <= 54; i++) {
    flashDrive.readRaw(Headbuffer, sizeof(Headbuffer));
    //Serial.println(Headbuffer[0]);
  }
  // if (read16(bmpFile) == 0x4D42) {  // BMP signature
  // Serial.print(F("File size: "));
  // Serial.println(read32(bmpFile));
  // (void)read32(bmpFile);             // Read & ignore creator bytes
  // bmpImageoffset = read32(bmpFile);  // Start of image data
  // Serial.print(F("Image Offset: "));
  // Serial.println(bmpImageoffset, DEC);
  // // Read DIB header
  // Serial.print(F("Header size: "));
  // Serial.println(read32(bmpFile));
  // bmpWidth = read32(bmpFile);
  // bmpHeight = read32(bmpFile);
  // if (read16(bmpFile) == 1) {    // # planes -- must be '1'
  //   bmpDepth = read16(bmpFile);  // bits per pixel
  //   Serial.print(F("Bit Depth: "));
  //   Serial.println(bmpDepth);
  //   if ((bmpDepth == 24) && (read32(bmpFile) == 0)) {  // 0 = uncompressed

  //     goodBmp = true;  // Supported BMP format -- proceed!
  //     Serial.print(F("Image size: "));
  //     Serial.print(bmpWidth);
  //     Serial.print('x');
  //     Serial.println(bmpHeight);

  //     // BMP rows are padded (if needed) to 4-byte boundary
  //     rowSize = (bmpWidth * 3 + 3) & ~3;

  // If bmpHeight is negative, image is in top-down order.
  // This is not canon but has been observed in the wild.
  // if (bmpHeight < 0) {
  //   bmpHeight = -bmpHeight;
  //   flip = false;
  // }

  // Crop area to be loaded
  //w = bmpWidth;
  //h = bmpHeight;


  Serial.println("Beginning loop");

  for (row = 0; row < h; row++) {  // For each scanline...

    // Seek to start of scan line.  It might seem labor-
    // intensive to be doing this on every line, but this
    // method covers a lot of gritty details like cropping
    // and scanline padding.  Also, the seek only takes
    // place if the file position actually needs to change
    // (avoids a lot of cluster math in SD library).
    // if (flip)  // Bitmap is stored bottom-to-top order (normal BMP)
    //   pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
    // else  // Bitmap is stored top-to-bottom
    //   pos = bmpImageoffset + row * rowSize;
    // if (bmpFile.position() != pos) {  // Need seek?
    //   tft.endWrite();
    //   bmpFile.seek(pos);
    //   buffidx = sizeof(USBbuffer);  // Force buffer reload
    // }
    //Serial.println("Beginning col loop");
    for (col = 0; col < w; col++) {  // For each pixel...
      // Time to read more pixel data?
      //Serial.print("buffidx: ");
      //Serial.println(buffidx);
      //Serial.print("sizeof(USBbuffer): ");
      //Serial.println(sizeof(USBbuffer));

      if (buffidx >= sizeof(USBbuffer)) { // Indeed
        //tft.endWrite();
        //Serial.print("read buff");
        //claimSPI ( "usb" );                          // Claim SPI bus
        
        //while(readMore){
          flashDrive.readRaw(USBbuffer, sizeof(USBbuffer));
          //Serial.print(".");
          //Serial.print(USBbuffer[1]);
        //}
        //Serial.println(" done");
        //flashDrive.closeFile();
        //Serial.println("Closed file");
        //releaseSPI();  

        buffidx = 0;                                  // Set index to beginning
        // Release SPI bus
        //Serial.print("start write... ");
        //claimSPI ( "tft" );
        //Serial.print("SPI claimed for TFT... ");
        //tft.startWrite();
        //releaseSPI();  
        //Serial.println("done");
      }
        //bmpFile.read(USBbuffer, sizeof(USBbuffer));

      // Convert pixel from BMP to TFT format, push to display

      g = USBbuffer[buffidx++];
      r = USBbuffer[buffidx++];
      b = USBbuffer[buffidx++];

      //Serial.print(b);
      //Serial.print(" ");
      //Serial.print(g);
      //Serial.print(" ");
      //Serial.print(r);
      //Serial.println("");
      //Serial.print("start push... ");

      //claimSPI ( "tft" );
      //Serial.print("SPI claimed for TFT... ");

      tft.pushColor(tft.color565(r, g, b));
      //tft.pushColor(ST77XX_RED);
      //releaseSPI();
      //Serial.println("done");
    }  // end pixel
  }    // end scanline
  
  //claimSPI ( "tft" );
  tft.endWrite();
  //releaseSPI();
  flashDrive.closeFile();

  Serial.print(F("Loaded in "));
  Serial.print(millis() - startTime);
  Serial.println(" ms");
  //}  // end goodBmp
  //}
  //}

  //bmpFile.close();
  //if (!goodBmp) Serial.println(F("BMP format not recognized."));
  //flashDrive.closeFile();
  Serial.println("Reading Done!");
}



//**************************************************************************************************
//                                      C L A I M S P I                                            *
//**************************************************************************************************
// Claim the SPI bus.  Uses FreeRTOS semaphores.                                                   *
// If the semaphore cannot be claimed within the time-out period, the function continues without   *
// claiming the semaphore.  This is incorrect but allows debugging.                                *
//**************************************************************************************************
void claimSPI ( const char* p )
{
  const              TickType_t ctry = 10 ;                 // Time to wait for semaphore
  uint32_t           count = 0 ;                            // Wait time in ticks
  static const char* old_id = "none" ;                      // ID that holds the bus
  
  while ( xSemaphoreTake ( SPIsem, ctry ) != pdTRUE  )      // Claim SPI bus
  {
    if ( count++ > 25 )
    {
      dbgprint ( "SPI semaphore not taken within %d ticks by CPU %d, id %s",
                 count * ctry,
                 xPortGetCoreID(),
                 p ) ;
      dbgprint ( "Semaphore is claimed by %s", old_id ) ;
    }
    if ( count >= 100 )
    {
      return ;                                               // Continue without semaphore
    }
  }
  old_id = p ;                                               // Remember ID holding the semaphore
  //dbgprint ( "Semaphore taken by %s", p ) ;

}


//**************************************************************************************************
//                                   R E L E A S E S P I                                           *
//**************************************************************************************************
// Free the the SPI bus.  Uses FreeRTOS semaphores.                                                *
//**************************************************************************************************
void releaseSPI()
{
  xSemaphoreGive ( SPIsem ) ;                            // Release SPI bus
}



// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}


void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

// end of code.


//**************************************************************************************************
//                                          D B G P R I N T                                        *
//**************************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the DEBUG flag.        *
// Print only if DEBUG flag is true.  Always returns the formatted string.                         *
//**************************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( DEBUG )                                         // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}
