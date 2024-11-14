#include "Adafruit_EPD.h"
#include <Arduino_LSM6DSOX.h>
#include <SPI.h>
#include "MAX30105.h"

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

MAX30105 particleSensor;

// Global variables for sensor readings
long start_time;
long time_offset = 60660000;
int currentScreen = 0;
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_UPDATE_INTERVAL = 30000; // 30 seconds
int currentHeartRate = 0;
int currentSpO2 = 0;
const byte RATE_SIZE = 4; // Increase this for more averaging
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;


// Function prototypes
void updateSensorReadings();
void displayTime();
void displayStepCount();
void displayHeartRate();
void displayOximeter();
void nextScreen();
String millisecondsToTimeString(unsigned long millis);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Flexible eInk Watch Interface");
  start_time = millis();

  display.begin();
  display.setBlackBuffer(1, true);
  display.setColorBuffer(1, true);

  // Initialize MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found!");
    while (1);
  }

  // Configure MAX30102 for heart rate and SpO2 monitoring
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  updateSensorReadings();

  // Initial display
  displayTime(); // Start with time screen
}

void loop() {
  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    updateSensorReadings();
    lastSensorUpdate = millis();
  }

  switch (currentScreen) {
    case 0:
      displayTime();
      break;
    case 1:
      displayStepCount();
      break;
    case 2:
      displayHeartRate();  // This will now show the updated BPM
      break;
    case 3:
      displayOximeter();
      break;
  }

  delay(10000);
  nextScreen();
}



// Function to draw a clock icon
void drawClockIcon(int x, int y) {
  display.drawCircle(x, y, 10, EPD_BLACK);
  display.drawLine(x, y, x + 6, y, EPD_BLACK);  // Hour hand
  display.drawLine(x, y, x, y - 7, EPD_BLACK);  // Minute hand
}

// Function to draw a foot icon for steps
void drawFootIcon(int x, int y) {
  display.drawCircle(x, y, 6, EPD_BLACK);       // Foot outline
  display.fillCircle(x - 2, y - 4, 2, EPD_BLACK); // Toes
  display.fillCircle(x + 2, y - 4, 2, EPD_BLACK);
}

// Function to draw a heart icon for heart rate
void drawHeartIcon(int x, int y) {
  display.fillCircle(x - 3, y - 2, 3, EPD_BLACK);
  display.fillCircle(x + 3, y - 2, 3, EPD_BLACK);
  display.fillTriangle(x - 6, y, x + 6, y, x, y + 8, EPD_BLACK);
}

// Function to draw an O2 icon for oxygen saturation
void drawOxygenIcon(int x, int y) {
  display.drawCircle(x, y, 5, EPD_BLACK);    // O symbol
  display.drawLine(x + 8, y - 3, x + 10, y - 3, EPD_BLACK);  // 2 symbol
  display.drawLine(x + 9, y - 3, x + 9, y + 3, EPD_BLACK);
  display.drawLine(x + 8, y + 3, x + 10, y + 3, EPD_BLACK);
}

// Functions to render each screen with icons and text
void displayTime() {
  display.clearBuffer();
  drawClockIcon(10, 10);
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(30, 5);
  display.print("Time");

  display.setTextSize(2);
  display.setCursor(30, 40);
  display.print(millisecondsToTimeString(millis() - start_time + time_offset));
  
  display.display();
}

void displayStepCount() {
  int steps = 7500;
  int calories = 300;

  display.clearBuffer();
  drawFootIcon(10, 10);
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(30, 5);
  display.print("Steps");

  display.setTextSize(2);
  display.setCursor(30, 40);
  display.print("Steps: ");
  display.print(steps);
  
  display.setCursor(30, 70);
  display.print("Calories: ");
  display.print(calories);
  
  display.display();
}
// Modified display functions to use real sensor data
void displayHeartRate() {
  display.clearBuffer();
  drawHeartIcon(10, 10);
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(30, 5);
  display.print("Heart Rate");

  display.setTextSize(2);
  display.setCursor(30, 40);
  display.print("BPM: ");
  if (currentHeartRate > 0) {
    display.print(currentHeartRate);
  } else {
    display.print("--");
  }
  
  display.display();
}

void displayOximeter() {
  display.clearBuffer();
  drawOxygenIcon(10, 10);
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(30, 5);
  display.print("Oximeter");

  display.setTextSize(2);
  display.setCursor(30, 40);
  display.print("O2: ");
  if (currentSpO2 > 0) {
    display.print(currentSpO2);
    display.print("%");
  } else {
    display.print("--");
  }
  
  display.display();
}




void updateSensorReadings() {
  long irValue = particleSensor.getIR();
  
  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);
    
    if (beatsPerMinute > 20 && beatsPerMinute < 255) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++) {
        beatAvg += rates[x];
      }
      beatAvg /= RATE_SIZE;
      currentHeartRate = beatAvg;  // Update the global variable for display
    }
  }
}





// Function to move to the next screen
void nextScreen() {
  currentScreen = (currentScreen + 1) % 4; // Cycle through 4 screens
}

// Helper function to format time as hh:mm:ss
String millisecondsToTimeString(unsigned long millis) {
    unsigned long seconds = millis / 1000;
    unsigned int hours = (seconds / 3600) % 24;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;

    char timeString[9];
    snprintf(timeString, sizeof(timeString), "%02u:%02u:%02u", hours, minutes, secs);

    return String(timeString);
}
