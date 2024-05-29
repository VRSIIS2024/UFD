/*--------------------------------------------------------------------------------------
 Demo for RGB panels

 DMD_STM32a example code for STM32 and RP2040 boards
 ------------------------------------------------------------------------------------- */
#define DMD_PARA
/*--------------------------------------------------------------------------------------
  Includes
--------------------------------------------------------------------------------------*/
#if defined(DMD_PARA)
#include "DMD_Monochrome_Parallel.h"
 #else
 #include "DMD_MonoChrome_SPI.h"
#endif


#include "DMD_RGB.h"

// Fonts includes
#include "st_fonts/UkrRusArial14.h"
#include "st_fonts/Arial14.h"
#include "st_fonts/Arial_black_16.h"
#include "st_fonts/SystemFont5x7.h"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Woverflow"
#include "gfx_fonts/GlametrixLight12pt7b.h"
#include "gfx_fonts/GlametrixBold12pt7b.h"
#pragma GCC diagnostic warning "-Wnarrowing"
#pragma GCC diagnostic warning "-Woverflow"

//Number of panels in x and y axis
#define DISPLAYS_ACROSS 2
#define DISPLAYS_DOWN 1

// Enable of output buffering
// if true, changes only outputs to matrix after
// swapBuffers(true) command
// If dual buffer not enabled, all output draw at matrix directly
// and swapBuffers(true) cimmand do nothing
#define ENABLE_DUAL_BUFFER true


// Monochrome
#if defined(ARDUINO_ARCH_RP2040)
#if defined(DMD_PARA)
#define DMD_PIN_A 1      //6
#define DMD_PIN_B 2     //7
#define DMD_PIN_nOE 14   //10
#define DMD_PIN_SCLK 15  //12

// These pins must be consecutive in ascending order
// uint8_t pins[] = { 6, 7, 8 };  // CLK , row1, row 2
uint8_t pins[] = { 6, 7 };  // CLK , row1, row 2
//#error Monochrome_SPI mode unsupported for Rasberry Pico RP2040
#endif
#endif

#if defined(DMD_PARA)
//Fire up the DMD object as dmd
DMD_Monochrome_Parallel dmd(DMD_PIN_A, DMD_PIN_B, DMD_PIN_nOE, DMD_PIN_SCLK, pins, DISPLAYS_ACROSS, DISPLAYS_DOWN, ENABLE_DUAL_BUFFER);
#else

//=== Config for SPI connect ====
SPIClass dmd_spi(1);
DMD_MonoChrome_SPI dmd(DMD_PIN_A, DMD_PIN_B, DMD_PIN_nOE, DMD_PIN_SCLK, DISPLAYS_ACROSS, DISPLAYS_DOWN, dmd_spi, ENABLE_DUAL_BUFFER);
#endif

// --- Define fonts ----
// DMD.h old style font
DMD_Standard_Font UkrRusArial_F(UkrRusArial_14);
DMD_Standard_Font Arial_F(Arial_14);
DMD_Standard_Font SystemFont5x7_F(SystemFont5x7);
DMD_Standard_Font Arial_Black_16_F(Arial_Black_16);

// GFX font with sepatate parts for Latin and Cyrillic chars
DMD_GFX_Font GlametrixL((uint8_t*)&GlametrixLight12pt7b, (uint8_t*)&GlametrixLight12pt8b_rus, 0x80, 13);


#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#define ETHERNET_SS_PIN 17
byte SERVER_MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // Replace with your MAC address
const int ip_array[4] = { 192, 168, 2, 125 };                // Replace with your Arduino's IP address
//EthernetServer server(1000); // Listen on port 80

#define SERVER_PORT 1000
#define SERIAL_BAUDRATE 115200
#define SERIAL_INTERFACE Serial
#define COM_PORT Serial
EthernetServer server = EthernetServer(SERVER_PORT);
EthernetClient client;
String getMessageEthernet();
const int BUFFER_SIZE = 64;
//char tollPlazaMessage[BUFFER_SIZE] = "To Toll Plaza";

void processMessage(EthernetClient& client);

/*--------------------------------------------------------------------------------------
  UTF8 char recoding

--------------------------------------------------------------------------------------*/
int utf8_rus(char* dest, const unsigned char* src) {

  uint16_t i, j;
  for (i = 0, j = 0; src[i]; i++) {
    if ((src[i] == 0xD0) && src[i + 1]) {
      dest[j++] = src[++i] - 0x10;
    } else if ((src[i] == 0xD1) && src[i + 1]) {
      dest[j++] = src[++i] + 0x30;
    } else dest[j++] = src[i];
  }
  dest[j] = '\0';
  return j;
}

/*--------------------------------------------------------------------------------------
  setup
  Called by the Arduino architecture before the main loop begins
--------------------------------------------------------------------------------------*/

void setup(void) {
  Serial.begin(115200);
  EEPROM.begin(64);
  // initialize DMD objects
  dmd.init();
  dmd.setBrightness(80);
  Ethernet.init(ETHERNET_SS_PIN);
  Serial.print("PORT:");
  int port = SERVER_PORT;
  if (EEPROM.read(5) < 254) {
    port = EEPROM.read(5);
  }
  Serial.println(port);
  if (EEPROM.read(0) != 255 && EEPROM.read(1) != 255 && EEPROM.read(2) != 255 && EEPROM.read(3) != 255) {
    IPAddress SERVER_IP(EEPROM.read(0), EEPROM.read(1), EEPROM.read(2), EEPROM.read(3));
    Ethernet.begin(SERVER_MAC, SERVER_IP);
  } else {
    IPAddress SERVER_IP(ip_array[0], ip_array[1], ip_array[2], ip_array[3]);
    Ethernet.begin(SERVER_MAC, SERVER_IP);
  }
  server.begin();

  Serial.print("My IP:");
  server.println(Ethernet.localIP());
  Serial.println(Ethernet.localIP());
}


/*--------------------------------------------------------------------------------------
  loop
  Arduino architecture main loop
--------------------------------------------------------------------------------------*/

void loop() {
  String message = getMessageEthernet();
  if (message.length() > 0) {
    Serial.println("Received message: " + message);
    server.println(message);
    parseCommand(message);
  } else {
    dev1();
  }
}

String getMessageEthernet() {
  String message = "";
  char ch;
  client = server.available();
  while (client.connected() && !client.available()) {
    delay(1);  // Wait for data to be available
  }
  while (client.available()) {
    ch = client.read();
    if (ch == '\n') {
      break;  // End of message
    }
    message += ch;
  }
  return message;
}

String welcomeMessage = "Welcome"; // Define global variable for welcome message
String tollPlazaMessage = "To VRSIIS"; // Define global variable for toll plaza message

void parseCommand(String message) {
  if (message.length() > 0) {
    int commaIndex = message.indexOf(',');
    if (commaIndex != -1) {
      String command = message.substring(0, commaIndex);
      if (command == "WELCOME") {
        String newWelcomeMessage = message.substring(commaIndex + 1);
        updateWelcomeMessage(newWelcomeMessage);
      } else if (command == "TOLLPLAZA") {
        String newTollPlazaMessage = message.substring(commaIndex + 1);
        updateTollPlazaMessage(newTollPlazaMessage);
      }
    }
  }
}

void updateWelcomeMessage(String newMessage) {
  welcomeMessage = newMessage; // Update global variable
}

void updateTollPlazaMessage(String newMessage) {
  tollPlazaMessage = newMessage; // Update global variable
}

void display(String message1, String message2) {
  char message1Array[message1.length() + 1];
  message1.toCharArray(message1Array, message1.length() + 1);

  char message2Array[message2.length() + 1];
  message2.toCharArray(message2Array, message2.length() + 1);

  dmd.clearScreen(true);
  dmd.selectFont(&SystemFont5x7_F);
  dmd.drawMarqueeX(message1Array, 0, 0);
  dmd.drawMarqueeX(message2Array, 0, 8);
  dmd.swapBuffers(true);
}



void dev1() {
  int tollPlazaX = dmd.width();
  dmd.selectFont(&SystemFont5x7_F);
  while (tollPlazaX >= -dmd.stringWidth(tollPlazaMessage.c_str())) {
    dmd.clearScreen(true);
    dmd.drawMarqueeX(welcomeMessage.c_str(), 0, 0);
    dmd.drawMarqueeX(tollPlazaMessage.c_str(), tollPlazaX, 8);
    tollPlazaX--;
    dmd.swapBuffers(true);
    delay(50);
  }
}
