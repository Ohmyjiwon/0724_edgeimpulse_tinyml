#include <Oh_Test_inferencing.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE Client Connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE Client Disconnected!");
    }
};

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static int get_feature_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

unsigned long lastSample = 0;
unsigned long lastInference = 0;

int16_t curAx = 0, curAy = 0, curAz = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(5, 6);
  
  // Set BNO055 to NDOF mode (0x3D = 0x0C)
  Wire.beginTransmission(0x29);
  Wire.write(0x3D);
  Wire.write(0x0C);
  Wire.endTransmission();
  delay(50);
  
  // Init BLE
  BLEDevice::init("Ohzi_ESP32S3_Motion");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertisementData advertisementData;
  advertisementData.setName("Ohzi_ESP32S3_Motion");
  advertisementData.setCompleteServices(BLEUUID(SERVICE_UUID));

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE Server Ready & Advertising: Ohzi_ESP32S3_Motion");
}

void loop() {
  // 1. Sample 50Hz BNO055 data
  if (millis() - lastSample >= 20) {
    lastSample = millis();
    
    int16_t gx = 0, gy = 0, gz = 0;
    Wire.beginTransmission(0x29);
    Wire.write(0x08);
    Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      curAx = Wire.read() | (Wire.read() << 8);
      curAy = Wire.read() | (Wire.read() << 8);
      curAz = Wire.read() | (Wire.read() << 8);
    }
    
    Wire.beginTransmission(0x29);
    Wire.write(0x14);
    Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      gx = Wire.read() | (Wire.read() << 8);
      gy = Wire.read() | (Wire.read() << 8);
      gz = Wire.read() | (Wire.read() << 8);
    }
    
    memmove(features, features + 6, (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6) * sizeof(float));
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6] = (float)curAx;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 5] = (float)curAy;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 4] = (float)curAz;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3] = (float)gx;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2] = (float)gy;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = (float)gz;
  }

  // 2. Run Inference every 200ms & Notify BLE
  if (millis() - lastInference >= 200) {
    lastInference = millis();
    
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_feature_data;
    
    ei_impulse_result_t result = {0};
    if (run_classifier(&signal, &result, false) == EI_IMPULSE_OK) {
      int best = 0;
      String json = "{\"scores\":{";
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > result.classification[best].value) best = i;
        json += "\"" + String(ei_classifier_inferencing_categories[i]) + "\":" + String(result.classification[i].value, 3);
        if (i < EI_CLASSIFIER_LABEL_COUNT - 1) json += ",";
      }
      json += "},\"label\":\"" + String(ei_classifier_inferencing_categories[best]) + "\"";
      json += ",\"confidence\":" + String(result.classification[best].value, 3);
      json += ",\"ax\":" + String(curAx);
      json += ",\"ay\":" + String(curAy);
      json += ",\"az\":" + String(curAz) + "}";
      
      if (deviceConnected) {
        pCharacteristic->setValue(json.c_str());
        pCharacteristic->notify();
      }
    }
  }

  // Handle BLE disconnect / reconnect advertising
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restart advertising...");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}
