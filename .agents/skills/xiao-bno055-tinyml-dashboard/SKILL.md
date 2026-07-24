---
name: xiao-bno055-tinyml-dashboard
description: End-to-end guide and instructions for building a TinyML motion classifier (idle/walk/run) with XIAO ESP32S3 and BNO055 9-axis IMU, training via Edge Impulse API, fixing memory limits, and serving a live Dual BLE + Wi-Fi AP Web Dashboard. Use whenever the user asks for BNO055 gesture/motion recognition, TinyML motion classification skill, or wireless IoT dashboards.
---

# XIAO ESP32S3 + BNO055 TinyML Motion Classifier & Wireless Dashboard Skill

This skill provides a complete, verified pipeline for building, training, and deploying a real-time motion classification system using the **Seeed XIAO ESP32S3**, a **Bosch BNO055 9-Axis IMU sensor**, **Edge Impulse REST API**, and a **Dual BLE + Wi-Fi AP Web Dashboard**.

---

## 1. Hardware Architecture & BNO055 Setup

### Hardware Wiring
- **Board**: Seeed XIAO ESP32S3 (Sense)
- **Sensor**: Bosch BNO055 9-Axis IMU
- **I2C SDA Pin**: GPIO 5 (D4)
- **I2C SCL Pin**: GPIO 6 (D5)
- **I2C Address**: `0x29` (default for BNO055)

### BNO055 Register Reference & NDOF Initialization
- `0x00`: `CHIP_ID` (`0xA0`)
- `0x3D`: `OPR_MODE` (Operation Mode). **Must write `0x0C` (NDOF mode)** in `setup()` to activate sensor fusion registers.
- `0x08..0x0D`: 3-Axis Accelerometer LSB/MSB (`accX`, `accY`, `accZ`).
- `0x14..0x19`: 3-Axis Gyroscope LSB/MSB (`gyrX`, `gyrY`, `gyrZ`).

```cpp
Wire.begin(5, 6);
Wire.beginTransmission(0x29);
Wire.write(0x3D);
Wire.write(0x0C); // NDOF Sensor Fusion Mode
Wire.endTransmission();
delay(50);
```

---

## 2. Data Collection (AP Web Server)

To capture 5-second motion CSV files (`timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ`) at 50Hz:
- Flash `04_gyro_collect_ap.ino` on board.
- Connect phone/PC to Wi-Fi `Ohzi_esp` (`fivesupport`).
- Open `http://192.168.4.1`, select label (`idle`, `walk`, `run`), record 5 seconds, and click **Download CSV**.

---

## 3. Edge Impulse Training Pipeline (REST API)

### ⚠️ Critical Dataset Gotcha
When uploading a small dataset (e.g. 1-3 files per class), **do NOT call `/rebalance`**! Rebalancing will move 1 class entirely to the `testing` dataset, causing the Neural Network to be trained without that class!
- Upload files directly to `https://ingestion.edgeimpulse.com/api/training/files` with `-H "x-label: <label>"`.

### REST API Training Commands
```bash
KEY="ei_88aadd5f0c467791b2626e8bace11a95a279909c7956ae5c"
PROJ="1068317"

# 1. Update Impulse for 6-axis Time Series (50Hz, 1000ms window, 500ms slide)
curl -s -X POST -H "x-api-key: $KEY" -H "Content-Type: application/json" \
  -d '{"inputBlocks":[{"id":1,"type":"time-series","windowSizeMs":1000,"windowIncreaseMs":500,"frequencyHz":50}],"dspBlocks":[{"id":10,"type":"spectral-analysis","axes":["accX","accY","accZ","gyrX","gyrY","gyrZ"]}],"learnBlocks":[{"id":20,"type":"keras","dsp":[10]}]}' \
  https://studio.edgeimpulse.com/v1/api/$PROJ/impulse

# 2. Trigger Feature Generation & Keras Training
curl -s -X POST -H "x-api-key: $KEY" -H "Content-Type: application/json" -d '{"dspId":10}' https://studio.edgeimpulse.com/v1/api/$PROJ/jobs/generate-features
curl -s -X POST -H "x-api-key: $KEY" -H "Content-Type: application/json" -d '{"trainingCycles":40,"learningRate":0.005}' https://studio.edgeimpulse.com/v1/api/$PROJ/jobs/train/keras/20

# 3. Build & Download Arduino Library
curl -s -X POST -H "x-api-key: $KEY" -H "Content-Type: application/json" -d '{"engine":"tflite-eon"}' "https://studio.edgeimpulse.com/v1/api/$PROJ/jobs/build-ondevice-model?type=arduino"
curl -s -X GET -H "x-api-key: $KEY" "https://studio.edgeimpulse.com/v1/api/$PROJ/deployment/download?type=arduino" -o Oh_Test_inferencing.zip
```

### ⚠️ Mandatory ESP32S3 Memory Patch
In `/Users/jiwon/Documents/Arduino/libraries/Oh_Test_inferencing/src/edge-impulse-sdk/porting/ei_classifier_porting.h` line 374:
Change:
```cpp
#define EI_MAX_OVERFLOW_BUFFER_COUNT  30
```
To:
```cpp
#define EI_MAX_OVERFLOW_BUFFER_COUNT  2048
```

---

## 4. Dual BLE + Wi-Fi AP Web Dashboard System

### ESP32S3 Dual Wireless Firmware (`07_gyro_dual_ap_ble.ino`)
- Broadcasts **BLE GATT Service** (`0000ffe0-0000-1000-8000-00805f9b34fb`) & Characteristic (`0000ffe1-0000-1000-8000-00805f9b34fb`).
- Operates **Wi-Fi AP (`Ohzi_esp`)** at `http://192.168.4.1/classify`.
- **Smooth Prediction Filtering**: Uses **500ms inference interval** and **Exponential Moving Average (EMA)** smoothing (`smoothed = smoothed * 0.6 + new * 0.4`) to prevent jittery/flickering state changes from subtle hand movements.
- **Idle Fallback Rule (Tie-Breaker)**: When probabilities are nearly equal (`max_score - min_score < 0.18`) or confidence is low (`max_score < 0.45`), defaults the predicted state to `idle` (멈춤) to guarantee UI stability and eliminate false positives.
- Transmits live JSON inference payloads over BLE Notification and HTTP GET `/classify`:
  `{"label":"walk","confidence":0.98,"scores":{"idle":0.01,"walk":0.98,"run":0.01},"ax":-1140,"ay":-318,"az":275}`

### Local Motion Dashboard (`dashboard_app/server.js`)
- Runs a zero-dependency Node.js HTTP server on `http://localhost:8080`.
- Features Web Bluetooth API for one-click pairing on Chrome/Edge.
- Features automatic Wi-Fi AP HTTP Polling fallback for Safari & iOS devices without Web Bluetooth.
- Displays live Glassmorphism state badges, Chart.js probability timeline graphs, 3-axis accel telemetry, and state transition history logs.
