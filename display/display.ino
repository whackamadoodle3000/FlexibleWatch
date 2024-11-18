#include "Adafruit_EPD.h"
#include <Arduino_LSM6DSOX.h>
#include <SPI.h>
#include "MAX30105.h"
#include "heartRate.h"

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
long time_offset = 59505000;
int currentScreen = 0;
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_UPDATE_INTERVAL = 6000; // 6 seconds
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
  updateSensorReadings();




  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    nextScreen();
    lastSensorUpdate = millis();
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
  }
  
  
}

void displayTime() {
  display.clearBuffer();
  
  // Header text
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(60, 5);
  display.print("Time");

  // Draw clock icon in right corner
  drawClockIcon(250, 25);  // Moved to right side

  // Time display
  display.setTextSize(2);
  display.setCursor(30, 60);
  display.print(millisecondsToTimeString(millis() - start_time + time_offset));
  
  display.display();
}

void displayStepCount() {
  int steps = 7500;
  int calories = 300;

  display.clearBuffer();
  
  // Header text
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(60, 5);
  display.print("Steps");

  // Draw foot icon in right corner
  drawFootIcon(250, 25);  // Moved to right side

  // Stats
  display.setTextSize(2);
  display.setCursor(30, 60);
  display.print("Steps: ");
  display.print(steps);
  
  display.setCursor(30, 90);
  display.print("Cal: ");
  display.print(calories);
  
  display.display();
}

void displayHeartRate() {
  display.clearBuffer();
  
  // Header text
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(60, 5);
  display.print("Heart");

  // Draw heart icon in right corner
  drawHeartIcon(250, 25);  // Moved to right side

  // BPM display
  display.setTextSize(2);
  display.setCursor(30, 60);
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
  
  // Header text
  display.setTextSize(3);
  display.setTextColor(EPD_BLACK);
  display.setCursor(60, 5);
  display.print("SpO2");

  // Draw lungs icon in right corner
  drawOxygenIcon(250, 25);  // Moved to right side

  // O2 level display
  display.setTextSize(2);
  display.setCursor(30, 60);
  display.print("O2: ");
  if (currentSpO2 > 0) {
    display.print(currentSpO2);
    display.print("%");
  } else {
    display.print("--");
  }
  
  display.display();
}

// Modified icon drawing functions with slightly reduced sizes
void drawClockIcon(int x, int y) {
  int size = 15;  // Reduced from 20
  display.drawCircle(x, y, size, EPD_BLACK);
  display.drawCircle(x, y, size-2, EPD_BLACK);
  
  // Clock hands
  display.drawLine(x, y, x + size*0.6, y, EPD_BLACK);
  display.drawLine(x, y, x, y - size*0.8, EPD_BLACK);
  
  // Clock markers
  for(int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180;
    int markerLen = (i % 3 == 0) ? 4 : 2;
    int x1 = x + (size - 2) * cos(angle);
    int y1 = y + (size - 2) * sin(angle);
    int x2 = x + (size - markerLen) * cos(angle);
    int y2 = y + (size - markerLen) * sin(angle);
    display.drawLine(x1, y1, x2, y2, EPD_BLACK);
  }
}

void drawFootIcon(int x, int y) {
  int size = 18;  // Reduced from 25
  
  // Main foot shape
  display.fillRoundRect(x - size/2, y - size/3, size, size*1.2, size/3, EPD_BLACK);
  
  // Heel curve
  display.fillCircle(x, y + size/2, size/3, EPD_BLACK);
  
  // Toes
  int toeY = y - size/4;
  int toeSpacing = size/6;
  for(int i = 0; i < 5; i++) {
    display.fillCircle(x - size/3 + (i * toeSpacing), toeY, size/8, EPD_BLACK);
  }
  
  // Arch detail
  display.drawLine(x - size/3, y + size/3, x + size/4, y + size/2, EPD_WHITE);
}

void drawHeartIcon(int x, int y) {
  int size = 15;  // Reduced from 20
  
  display.fillCircle(x - size/2, y, size/2, EPD_BLACK);
  display.fillCircle(x + size/2, y, size/2, EPD_BLACK);
  
  display.fillTriangle(
    x - size, y,
    x + size, y,
    x, y + size*1.2,
    EPD_BLACK
  );
  
  display.fillCircle(x - size/3, y - size/4, size/6, EPD_WHITE);
}

void drawOxygenIcon(int x, int y) {
  int size = 18;  // Reduced from 25
  
  // Bronchi
  display.fillRoundRect(x - size/6, y - size/2, size/3, size/2, size/6, EPD_BLACK);
  
  // Left lung
  display.fillRoundRect(x - size*0.8, y, size*0.6, size, size/2, EPD_BLACK);
  display.fillCircle(x - size*0.5, y + size*0.8, size/3, EPD_BLACK);
  
  // Right lung
  display.fillRoundRect(x + size*0.2, y, size*0.6, size, size/2, EPD_BLACK);
  display.fillCircle(x + size*0.5, y + size*0.8, size/3, EPD_BLACK);
  
  // Detail lines
  display.drawLine(x - size*0.6, y + size*0.3, x - size*0.3, y + size*0.3, EPD_WHITE);
  display.drawLine(x + size*0.3, y + size*0.3, x + size*0.6, y + size*0.3, EPD_WHITE);
  
  // O2 text
  display.setTextSize(1);
  display.setTextColor(EPD_WHITE);
  display.setCursor(x - size/4, y + size/4);
  display.print("O2");
}


void updateSensorReadings() {
  // Get IR and Red light values from the particle sensor
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();
  
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

  // Calculate SpO2
  if (irValue > 50000) {  // Ensure there is enough signal
    float ratio = (float)redValue / irValue;
    float R = (ratio - 0.4) / 0.5;  // Calibration factor; adjust if needed
    currentSpO2 = 110 - (25 * R);   // Estimate SpO2 based on ratio

    if (currentSpO2 > 100) {
      currentSpO2 = 100;  // SpO2 should max out at 100%
    } else if (currentSpO2 < 0) {
      currentSpO2 = 0;    // SpO2 should not be negative
    }
  } else {
    currentSpO2 = 0; // No valid reading
  }
}




// Function to move to the next screen
void nextScreen() {
  currentScreen = (currentScreen + 1) % 4; // Cycle through 4 screens
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