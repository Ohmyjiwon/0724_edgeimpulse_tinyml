#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("--- Register Dump (Address 0x29) ---");
  Wire.begin(5, 6);
  
  for (uint8_t reg = 0; reg < 16; reg++) {
    Wire.beginTransmission(0x29);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom(0x29, 1);
    if (Wire.available()) {
      uint8_t val = Wire.read();
      Serial.print("Reg 0x");
      if (reg < 16) Serial.print("0");
      Serial.print(reg, HEX);
      Serial.print(": 0x");
      if (val < 16) Serial.print("0");
      Serial.println(val, HEX);
    }
  }
}

void loop() {
  delay(2000);
}
