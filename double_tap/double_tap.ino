#include <Wire.h>
#include "AS_LSM6DSOX.h"

// Create an instance of the sensor
AS_LSM6DSOX sox;

void setup() {
  // Initialize Serial Communication for Debugging
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize the sensor
  if (!sox.begin_I2C()) { // Initialize the gyro board
    Serial.println("LSM6DSOX Not Found?");
    while (1) {
      delay(10);
    }
  }

  Serial.println("LSM6DSOX Found");

  // Reset and configure the sensor
  sox.reset();
  sox.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  sox.setAccelDataRate(LSM6DS_RATE_833_HZ);
  sox.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  sox.setGyroDataRate(LSM6DS_RATE_1_66K_HZ);
  sox.setupTaps(); // Set up tap detection

  // Stabilize the sensor by capturing a few readings
  sensors_event_t accel, gyro, temp;
  for (int i = 0; i < 100; i++) {
    sox.getEvent(&accel, &gyro, &temp);
    delay(10);
  }

  Serial.println("Sensor initialized and ready for tap detection");
}

void loop() {
  // Check for double tap detection
  if (checkForDoubleTap()) {
    Serial.println("Double tap detected!");
  }

  // Add a small delay to avoid overwhelming the serial monitor
  delay(50);
}

// Function to check for double tap
bool checkForDoubleTap() {
  static unsigned long tapWait = 0; // Decay value to prevent refiring immediately
  uint8_t reg;

  if (millis() > tapWait) {
    reg = sox.getRegister(0x1C); // Read the status register for tap events
    if (((reg & 0x40) == 0x40) && ((reg & 0x10) == 0x10)) { // Double tap condition
      tapWait = millis() + 1000; // Prevent multiple detections for 1.5 seconds
      return true;
    }
  }

  return false;
}
