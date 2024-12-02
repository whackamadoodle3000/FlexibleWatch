#include "Adafruit_EPD.h"
#include <Arduino_LSM6DSOX.h>
#include <SPI.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"
#include "AS_LSM6DSOX.h"

// QT Py pins - minimal setup
#define EPD_DC      A1    
#define EPD_CS      A2    
#define EPD_BUSY    -1    
#define EPD_RESET   A3    
#define SRAM_CS     -1    

// Initialize for 2.9" flexible display
Adafruit_UC8151D display(212, 104, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

MAX30105 particleSensor;
AS_LSM6DSOX sox;

// Global variables
long start_time;
long time_offset = 59505000;
int currentScreen = 0;

// Improved heart rate and SpO2 variables
#define MAX_BRIGHTNESS 255
#define RATE_SIZE 8

// Buffer for SpO2 calculation
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100]; //red LED sensor data

// Heart rate and SpO2 variables
int32_t spo2; //SpO2 value
int8_t validSPO2; //indicator to show if the SpO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

// Step counting variables
int stepCount = 0;
float lastMagnitude = 0;
float threshold = 10; // Sensitivity threshold for detecting steps
unsigned long lastStepTime = 0; // To prevent double-counting
unsigned long stepDelay = 300; // Minimum time between steps (milliseconds)

// Screen update variables
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
void updateStepCount();

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Initialize display
  display.begin();
  display.setBlackBuffer(1, true);
  display.setColorBuffer(1, true);

  // Initialize MAX30105 particle sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found!");
    while (1);
  }

  // Configure particle sensor
  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  // Initialize LSM6DSOX for step counting and tap detection
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

  // Initial display
  displayDigitalWatch();
}

void loop() {
  if (millis() - lastCheck >= 10) {
    updateSensorReadings();
    updateStepCount();
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

void updateStepCount() {
  sensors_event_t accel, gyro, temp;
  sox.getEvent(&accel, &gyro, &temp);

  // Calculate the magnitude of acceleration (ignoring gravity for simplicity)
  float magnitude = sqrt(
      accel.acceleration.x * accel.acceleration.x +
      accel.acceleration.y * accel.acceleration.y +
      accel.acceleration.z * accel.acceleration.z);

  // Check for a step: peak detection
  unsigned long currentTime = millis();
  if (magnitude > threshold && (magnitude - lastMagnitude) > 2 &&
      (currentTime - lastStepTime) > stepDelay) {
    stepCount++;
    lastStepTime = currentTime;
  }

  lastMagnitude = magnitude;
}

void updateSensorReadings() {
  static byte bufferIndex = 0;
  
  // Shift the existing data 1 position to the left
  for (byte i = 1; i < 100; i++) {
    redBuffer[i-1] = redBuffer[i];
    irBuffer[i-1] = irBuffer[i];
  }

  // Get the newest sample
  redBuffer[99] = particleSensor.getRed();
  irBuffer[99] = particleSensor.getIR();

  // Calculate heart rate and SpO2 every 25 samples (1 second)
  if (++bufferIndex >= 25) {
    bufferIndex = 0;
    
    // Perform heart rate and SpO2 calculation
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, 100, redBuffer, 
      &spo2, &validSPO2, 
      &heartRate, &validHeartRate
    );
    
    // Validate and update display-ready variables
    if (validHeartRate == 1) {
      // Ensure heart rate is within reasonable range
      if (heartRate > 0 && heartRate < 255) {
        Serial.print("Heart Rate: ");
        Serial.println(heartRate);
      }
    }
    
    if (validSPO2 == 1) {
      // Ensure SpO2 is within valid range
      if (spo2 > 0 && spo2 <= 100) {
        Serial.print("SpO2: ");
        Serial.println(spo2);
      }
    }
  }
}

void displayDigitalWatch() {
  display.clearBuffer();
  
  // Decorative border
  display.drawRect(0, 0, 212, 104, EPD_BLACK);
  display.drawRect(2, 2, 208, 100, EPD_BLACK);
  
  // Large time display with improved sizing and centering
  display.setTextSize(3);  // Increased text size
  display.setTextColor(EPD_BLACK);
  
  String timeStr = millisecondsToTimeString(millis() - start_time + time_offset);
  
  // Center the time horizontally
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (212 - w) / 2;  // Center horizontally
  display.setCursor(x, 30);  // Vertically positioned
  display.print(timeStr);
  
  // Smaller, centered date
  display.setTextSize(1);
  String dateStr = "12/02/2024";  // Replace with dynamic date method if needed
  display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
  x = (212 - w) / 2;  // Center horizontally
  display.setCursor(x, 60);  // Positioned below time
  display.print(dateStr);
  
  // Subtle bottom text
  display.setCursor(40, 90);
  display.print("Double tap for stats");
  
  display.display();
}

void displayStatsScreen() {
  display.clearBuffer();
  
  // Decorative border
  display.drawRect(0, 0, 212, 104, EPD_BLACK);
  display.drawRect(2, 2, 208, 100, EPD_BLACK);
  
  // Header with larger text
  display.setTextSize(2);
  display.setTextColor(EPD_BLACK);
  display.setCursor(10, 5);
  display.print("Biometrics");
  display.drawLine(10, 25, 200, 25, EPD_BLACK);

  // Icons and stats
  // Heart Rate
  drawHeartIcon(5, 40);
  display.setTextSize(1);
  display.setCursor(30, 35);
  display.print("Heart Rate:");
  display.setCursor(140, 35);
  display.print((validHeartRate == 1 && heartRate > 0 && heartRate < 255) 
                ? String(heartRate) + " BPM" : "--");

  // Steps
  drawStepsIcon(5, 60);
  display.setCursor(30, 55);
  display.print("Steps:");
  display.setCursor(140, 55);
  display.print(stepCount);

  // Oxygen
  drawOxygenIcon(5, 80);
  display.setCursor(30, 75);
  display.print("Oxygen:");
  display.setCursor(140, 75);
  display.print((validSPO2 == 1 && spo2 > 0 && spo2 <= 100) 
                ? String(spo2) + "%" : "--");
  
  display.display();
}

void drawHeartIcon(int x, int y) {
  display.fillCircle(x, y, 5, EPD_BLACK);
  display.fillCircle(x + 8, y, 5, EPD_BLACK);
  display.fillTriangle(x - 5, y, x + 13, y, x + 4, y + 10, EPD_BLACK);
}

void drawStepsIcon(int x, int y) {
  display.fillRect(x, y, 10, 5, EPD_BLACK);  // Base
  display.fillRect(x + 3, y - 4, 7, 4, EPD_BLACK);  // Toe section
  display.fillRect(x + 7, y - 8, 4, 4, EPD_BLACK);  // Toe tip
}

void drawOxygenIcon(int x, int y) {
  display.fillRoundRect(x, y, 15, 8, 2, EPD_BLACK);  // Bronchi
  display.fillCircle(x + 4, y + 10, 4, EPD_BLACK);   // Left lung
  display.fillCircle(x + 11, y + 10, 4, EPD_BLACK);  // Right lung
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
    snprintf(timeString, sizeof(timeString), "%02u:%02u %s", hours, minutes, period.c_str());

    return String(timeString);
}