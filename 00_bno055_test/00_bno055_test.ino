#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(3000);
  Wire.begin(5, 6);
  Serial.println("=== BNO055 Gyro/Acc Live Test ===");
  
  // Set BNO055 to NDOF mode or ACCGYRO mode (Operation Mode Reg 0x3D = 0x0C NDOF, or 0x07 AMG mode)
  Wire.beginTransmission(0x29);
  Wire.write(0x3D); // OPR_MODE
  Wire.write(0x0C); // NDOF mode
  Wire.endTransmission();
  delay(50);
}

void loop() {
  // Read 6 bytes of Accelerometer data starting at 0x08
  Wire.beginTransmission(0x29);
  Wire.write(0x08);
  Wire.endTransmission();
  
  Wire.requestFrom(0x29, 6);
  if (Wire.available() >= 6) {
    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);
    int16_t z = Wire.read() | (Wire.read() << 8);
    
    Serial.print("AccX: "); Serial.print(x);
    Serial.print(" | AccY: "); Serial.print(y);
    Serial.print(" | AccZ: "); Serial.println(z);
  }
  delay(200);
}
