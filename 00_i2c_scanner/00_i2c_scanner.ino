#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("--- I2C Scanner ---");
  Wire.begin(5, 6);
}

void loop() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Scanning D4(GPIO5) / D5(GPIO6)...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Found device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found!");
  }
  Serial.println("-------------------");
  delay(3000);
}
