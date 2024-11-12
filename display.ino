#include "Adafruit_EPD.h"
#include <Arduino_LSM6DSOX.h>
#include <SPI.h>

// QT Py pins - minimal setup
#define EPD_DC      A1    // Data/Command
#define EPD_CS      A2    // Chip Select
#define EPD_BUSY    -1    // Not connected
#define EPD_RESET   A3    // Not connected
#define SRAM_CS     -1    // No SRAM chip

// Initialize for 2.9" flexible display
Adafruit_UC8151D display(296, 128,    // Width, height
                        EPD_DC,       // DC pin
                        EPD_RESET,    // Reset pin (not used)
                        EPD_CS,       // CS pin
                        SRAM_CS,      // SRAM CS (-1)
                        EPD_BUSY);    // Busy pin (not used)

long time;
long start_time;
long offset = 60660000;

String millisecondsToTimeString(unsigned long millis) {
    // Calculate hours, minutes, and seconds
    unsigned long seconds = millis / 1000;
    unsigned int hours = (seconds / 3600) % 24;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;

    // Format as hh:mm:ss
    char timeString[9];
    snprintf(timeString, sizeof(timeString), "%02u:%02u:%02u", hours, minutes, secs);

    return String(timeString);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Flexible eInk test");
  start_time = millis();

  // Important: Set the correct buffer settings for flexible display
  display.begin();
  display.setBlackBuffer(1, false);  // Required for flexible displays
  display.setColorBuffer(1, false);  // Required for flexible displays

  // Draw a test pattern
  display.clearBuffer();
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(10, 10);
  display.print("Test!");
  
  // Draw a rectangle to make sure display is working
  display.drawRect(10, 50, 100, 40, EPD_BLACK);
  
  display.display();
}

void loop() {
    // Draw a test pattern
  display.clearBuffer();
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(10, 10);
  display.print(millisecondsToTimeString(millis() - start_time + offset));
  
  // Draw a rectangle to make sure display is working
// Draw face outline
display.drawCircle(60, 70, 40, EPD_BLACK); // Head of the smiley face

// Draw eyes
display.drawCircle(45, 55, 5, EPD_BLACK);  // Left eye
display.drawCircle(75, 55, 5, EPD_BLACK);  // Right eye

// Draw mouth as an arc
display.drawRect(45, 85, 30, 5, EPD_BLACK);  // Smiley mouth
  
  display.display();
  delay(10000);  // Wait 5 seconds between updates if you add any
}