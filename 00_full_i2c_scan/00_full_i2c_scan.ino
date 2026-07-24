#include <Wire.h>

void scanBus(int sda, int scl, const char* label) {
  Serial.print("--- Scanning "); Serial.print(label);
  Serial.print(" (SDA="); Serial.print(sda);
  Serial.print(", SCL="); Serial.print(scl); Serial.println(") ---");
  
  Wire.begin(sda, scl);
  int count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C Device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      if (addr == 0x68 || addr == 0x69) Serial.print(" (MPU6050 Gyro!)");
      if (addr == 0x6A || addr == 0x6B) Serial.print(" (LSM6DS3 Gyro!)");
      Serial.println();
      count++;
    }
  }
  if (count == 0) Serial.println("No devices found on this bus.");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  scanBus(5, 6, "D4/D5 Default Pins");
  scanBus(1, 2, "D0/D1 Alternative Pins");
  scanBus(43, 44, "D6/D7 Serial Pins");
}

void loop() {
  delay(3000);
}
