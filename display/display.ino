#include "Adafruit_EPD.h"
#include <Arduino_LSM6DSOX.h>
#include <SPI.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "AS_LSM6DSOX.h"

// QT Py pins - minimal setup
#define EPD_DC      A1    
#define EPD_CS      A2    
#define EPD_BUSY    -1    
#define EPD_RESET   A3    
#define SRAM_CS     -1    

// Initialize for 2.9" flexible display
Adafruit_UC8151D display(296, 128, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

MAX30105 particleSensor;
AS_LSM6DSOX sox;

// Global variables
long start_time;
long time_offset = 59505000;
int currentScreen = 0;
volatile int currentHeartRate = 0;
volatile int currentSpO2 = 0;
const byte RATE_SIZE = 8;
volatile byte rates[RATE_SIZE];
volatile byte rateSpot = 0;
volatile long lastBeat = 0;
volatile float beatsPerMinute;
volatile int beatAvg;
volatile unsigned long lastCheck = 0;

// Function prototypes
bool checkForDoubleTap();
void updateSensorReadings();
void displayDigitalWatch();
void displayStatsScreen();
void nextScreen();
String millisecondsToTimeString(unsigned long millis);
void drawHeartIcon(int x, int y);
void drawStepsIcon(int x, int y);
void drawOxygenIcon(int x, int y);

void setup() {
  Serial.begin(115200);
  // while (!Serial) delay(10);
  delay(2000);
  
  display.begin();
  display.setBlackBuffer(1, true);
  display.setColorBuffer(1, true);

  // Sensor initialization (same as previous code)
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
   Serial.println("MAX30102 not found!");
    while (1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  if (!sox.begin_I2C()) {
    Serial.println("LSM6DSOX Not Found?");
    while (1) delay(10);
  }

  sox.reset();
  sox.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  sox.setAccelDataRate(LSM6DS_RATE_833_HZ);
  sox.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  sox.setGyroDataRate(LSM6DS_RATE_1_66K_HZ);
  sox.setupTaps();

  Serial.println("INITKAL DIPSLAY");

  // Initial display
  displayDigitalWatch();
}

void loop() {
  if (millis() - lastCheck >= 10) {
    updateSensorReadings();
    lastCheck = millis();
  }
  
  if (checkForDoubleTap()) {
    nextScreen();
    
    switch (currentScreen) {
      case 0:
        displayDigitalWatch();
        break;
      case 1:
        displayStatsScreen();
        break;
    }
    delay(1);
  }
}

void displayDigitalWatch() {
  display.clearBuffer();
  
  // Decorative border
  display.drawRect(0, 0, 296, 128, EPD_BLACK);
  display.drawRect(2, 2, 292, 124, EPD_BLACK);
  
  // Large time display with modern font-like styling
  display.setTextSize(4);
  display.setTextColor(EPD_BLACK);
  display.setCursor(40, 40);
  
  String timeStr = millisecondsToTimeString(millis() - start_time + time_offset);
  display.print(timeStr);
  
  // Subtle bottom text
  display.setTextSize(1);
  display.setCursor(90, 110);
  display.print("Double tap for stats");
  
  // Decorative dots or separators
  for(int i = 0; i < 5; i++) {
    display.fillCircle(140 + i*20, 75, 2, EPD_BLACK);
  }
  
  display.display();
}

void displayStatsScreen() {
  int steps = 7500;
  int calories = 300;

  display.clearBuffer();
  
  // Decorative border
  display.drawRect(0, 0, 296, 128, EPD_BLACK);
  display.drawRect(2, 2, 292, 124, EPD_BLACK);
  
  // Header with larger, bolder text
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(10, 5);
  display.print("Biometrics");
  display.drawLine(10, 30, 250, 30, EPD_BLACK);

  // Icons and stats with larger text
  // Heart Rate
  drawHeartIcon(20, 50);
  display.setTextSize(2);
  display.setCursor(50, 45);
  display.print("Heart Rate:");
  display.setCursor(180, 45);
  display.print(currentHeartRate > 0 ? String(currentHeartRate) + " BPM" : "--");

  // Steps
  drawStepsIcon(20, 80);
  display.setCursor(50, 75);
  display.print("Steps:");
  display.setCursor(180, 75);
  display.print(steps);

  // Oxygen
  drawOxygenIcon(20, 110);
  display.setCursor(50, 105);
  display.print("Oxygen:");
  display.setCursor(180, 105);
  display.print(currentSpO2 > 0 ? String(currentSpO2) + "%" : "--");
  
  display.display();
}

// Improved icon drawing functions
void drawHeartIcon(int x, int y) {
  // More recognizable heart icon
  display.fillCircle(x, y, 8, EPD_BLACK);
  display.fillCircle(x + 12, y, 8, EPD_BLACK);
  display.fillTriangle(x - 8, y, x + 20, y, x + 6, y + 15, EPD_BLACK);
}

void drawStepsIcon(int x, int y) {
  // Foot-like steps icon
  display.fillRect(x, y, 15, 8, EPD_BLACK);  // Base
  display.fillRect(x + 5, y - 5, 10, 5, EPD_BLACK);  // Toe section
  display.fillRect(x + 10, y - 10, 5, 5, EPD_BLACK);  // Toe tip
}

void drawOxygenIcon(int x, int y) {
  // More detailed lung-like icon
  display.fillRoundRect(x, y, 20, 10, 3, EPD_BLACK);  // Bronchi
  display.fillCircle(x + 5, y + 15, 5, EPD_BLACK);    // Left lung
  display.fillCircle(x + 15, y + 15, 5, EPD_BLACK);   // Right lung
}


void updateSensorReadings() {
  // Get IR and Red light values from the particle sensor
  long irValue = particleSensor.getIR();
  // long redValue = particleSensor.getRed();
  
  // Update heart rate
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

    if (irValue < 50000)
    Serial.print(" No finger?");

  // Calculate SpO2
  // if (irValue > 50000) {  // Ensure there is enough signal
  //   float ratio = (float)redValue / irValue;
  //   float R = (ratio - 0.4) / 0.5;  // Calibration factor; adjust if needed
  //   currentSpO2 = 110 - (25 * R);   // Estimate SpO2 based on ratio

  //   if (currentSpO2 > 100) {
  //     currentSpO2 = 100;  // SpO2 should max out at 100%
  //   } else if (currentSpO2 < 0) {
  //     currentSpO2 = 0;    // SpO2 should not be negative
  //   }
  // } else {
  //   currentSpO2 = 0; // No valid reading
  // }
}




// Function to move to the next screen
void nextScreen() {
  currentScreen = (currentScreen + 1) % 4; // Cycle through 4 screens
}

// Function to check for double tap
bool checkForDoubleTap() {
  static unsigned long tapWait = 0; // Decay value to prevent refiring immediately
  uint8_t reg;

  if (millis() > tapWait) {
    reg = sox.getRegister(0x1C); // Read the status register for tap events
    if (((reg & 0x40) == 0x40) && ((reg & 0x10) == 0x10)) { // Double tap condition
      tapWait = millis() + 1500; // Prevent multiple detections for 1.5 seconds
      return true;
    }
  }

  return false;
}

String millisecondsToTimeString(unsigned long millis) {
    unsigned long seconds = millis / 1000;
    unsigned int hours = (seconds / 3600) % 24;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;
    
    // Determine AM or PM
    String period = (hours >= 12) ? "PM" : "AM";
    
    // Convert hours from 24-hour format to 12-hour format
    if (hours == 0) {
        hours = 12;  // Midnight case
    } else if (hours > 12) {
        hours -= 12;  // PM hours
    }

    char timeString[11];  // Increase size for AM/PM
    snprintf(timeString, sizeof(timeString), "%02u:%02u:%02u %s", hours, minutes, secs, period);

    return String(timeString);
}