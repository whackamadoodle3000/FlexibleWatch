#include "AS_LSM6DSOX.h"

AS_LSM6DSOX sox;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for Serial connection
  }

  Serial.println("LSM6DSOX Step Count Example");

  if (!sox.begin_I2C()) {
    Serial.println("LSM6DSOX Not Found");
    while (1) delay(10);
  }
  Serial.println("LSM6DSOX Found");

  // Example: Write to PEDO_DEB_STEPS_CONF at address 0x84 on page 1
  uint8_t debounceValue = 0x0A; // Set debounce step count to 10
  sox.writeAdvancedRegister(0x84, debounceValue);
  Serial.print("Set PEDO_DEB_STEPS_CONF to 0x");
  Serial.println(debounceValue, HEX);

  // Read back the value to confirm
  uint8_t readValue = sox.readAdvancedRegister(0x84);
  Serial.print("Read PEDO_DEB_STEPS_CONF: 0x");
  Serial.println(readValue, HEX);

  Serial.println("Setup complete.");
}

void loop() {
  // Retrieve and display step count
  uint16_t steps = sox.getStepCount();
  Serial.print("Steps counted: ");
  Serial.println(steps);

  delay(1000); // Update every second
}
