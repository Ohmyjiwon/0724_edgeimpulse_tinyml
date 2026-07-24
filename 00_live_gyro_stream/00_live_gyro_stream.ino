#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Wire.begin(5, 6);
  Serial.println("=== Live Sensor Data Stream (Address 0x29) ===");
}

void loop() {
  Wire.beginTransmission(0x29);
  Wire.write(0x00);
  Wire.endTransmission();
  
  Wire.requestFrom(0x29, 6);
  if (Wire.available() >= 6) {
    uint8_t d0 = Wire.read();
    uint8_t d1 = Wire.read();
    uint8_t d2 = Wire.read();
    uint8_t d3 = Wire.read();
    uint8_t d4 = Wire.read();
    uint8_t d5 = Wire.read();
    
    Serial.print("Sensor Stream -> D0: "); Serial.print(d0);
    Serial.print(" | D1: "); Serial.print(d1);
    Serial.print(" | D2: "); Serial.print(d2);
    Serial.print(" | D3: "); Serial.print(d3);
    Serial.print(" | D4: "); Serial.print(d4);
    Serial.print(" | D5: "); Serial.println(d5);
  }
  delay(300);
}
