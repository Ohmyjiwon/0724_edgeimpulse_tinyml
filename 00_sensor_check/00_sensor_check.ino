#include <Wire.h>

uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(addr, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Wire.begin(5, 6);
}

void loop() {
  Serial.println("=== Sensor Identification Test ===");
  uint8_t c0 = readReg(0x29, 0xC0);
  uint8_t c1 = readReg(0x29, 0xC1);
  uint8_t c2 = readReg(0x29, 0xC2);
  uint8_t paj0 = readReg(0x29, 0x00);
  uint8_t paj1 = readReg(0x29, 0x01);
  
  Serial.print("Reg 0xC0 (VL53L0X ID): 0x"); Serial.println(c0, HEX);
  Serial.print("Reg 0xC1: 0x"); Serial.println(c1, HEX);
  Serial.print("Reg 0xC2: 0x"); Serial.println(c2, HEX);
  Serial.print("Reg 0x00: 0x"); Serial.println(paj0, HEX);
  Serial.print("Reg 0x01: 0x"); Serial.println(paj1, HEX);
  
  if (c0 == 0xEE && c1 == 0xAA) {
    Serial.println("DETECTED: VL53L0X Distance Sensor!");
  } else if (c0 == 0xEA) {
    Serial.println("DETECTED: VL53L1X Distance Sensor!");
  } else {
    Serial.println("DETECTED: Sensor responding at Address 0x29");
  }
  Serial.println("---------------------------------");
  delay(2000);
}
