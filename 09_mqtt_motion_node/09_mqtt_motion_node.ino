#include <Oh_Test_inferencing.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// ⚠️ 인터넷이 되는 Wi-Fi 공유기 정보를 입력하세요! (Public 브로커에 접속하기 위함)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Broker 설정 (HiveMQ Public Broker)
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "teamX/node1/motion";

WiFiClient espClient;
PubSubClient client(espClient);

// BNO055 데이터 배열
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
int16_t curAx = 0, curAy = 0, curAz = 0;
int16_t curYaw = 0, curRoll = 0, curPitch = 0;

unsigned long lastSample = 0;
unsigned long lastInference = 0;
static float smoothed_scores[EI_CLASSIFIER_LABEL_COUNT] = {0.0f};

static int get_feature_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32MotionNode-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize BNO055 (NDOF mode)
  Wire.begin(5, 6);
  Wire.beginTransmission(0x29);
  Wire.write(0x3D);
  Wire.write(0x0C);
  Wire.endTransmission();
  delay(100);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 1. 센서 50Hz 수집
  if (millis() - lastSample >= 20) {
    lastSample = millis();
    
    int16_t gx = 0, gy = 0, gz = 0;
    
    // 가속도 읽기 (0x08)
    Wire.beginTransmission(0x29); Wire.write(0x08); Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      curAx = Wire.read() | (Wire.read() << 8);
      curAy = Wire.read() | (Wire.read() << 8);
      curAz = Wire.read() | (Wire.read() << 8);
    }
    
    // 자이로 읽기 (0x14)
    Wire.beginTransmission(0x29); Wire.write(0x14); Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      gx = Wire.read() | (Wire.read() << 8);
      gy = Wire.read() | (Wire.read() << 8);
      gz = Wire.read() | (Wire.read() << 8);
    }
    
    // 오일러 각도 읽기 (0x1A)
    Wire.beginTransmission(0x29); Wire.write(0x1A); Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      curYaw = Wire.read() | (Wire.read() << 8);
      curRoll = Wire.read() | (Wire.read() << 8);
      curPitch = Wire.read() | (Wire.read() << 8);
    }
    
    // Edge Impulse 버퍼 시프트
    memmove(features, features + 6, (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6) * sizeof(float));
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6] = (float)curAx;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 5] = (float)curAy;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 4] = (float)curAz;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3] = (float)gx;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2] = (float)gy;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = (float)gz;
  }

  // 2. 추론 및 MQTT 발행 (500ms 간격)
  if (millis() - lastInference >= 500) {
    lastInference = millis();
    
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_feature_data;
    
    ei_impulse_result_t result = {0};
    if (run_classifier(&signal, &result, false) == EI_IMPULSE_OK) {
      // EMA Smoothing
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        smoothed_scores[i] = smoothed_scores[i] * 0.60f + result.classification[i].value * 0.40f;
      }

      int best = 0;
      float max_s = smoothed_scores[0];
      float min_s = smoothed_scores[0];
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (smoothed_scores[i] > max_s) { max_s = smoothed_scores[i]; best = i; }
        if (smoothed_scores[i] < min_s) { min_s = smoothed_scores[i]; }
      }

      // Idle Fallback
      if ((max_s - min_s) < 0.18f || max_s < 0.45f) {
        for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
          if (String(ei_classifier_inferencing_categories[i]) == "idle") { best = i; break; }
        }
      }

      // JSON 생성
      String json = "{";
      json += "\"type\":\"motion\",";
      json += "\"label\":\"" + String(ei_classifier_inferencing_categories[best]) + "\",";
      json += "\"confidence\":" + String(smoothed_scores[best], 3) + ",";
      json += "\"ax\":" + String(curAx) + ",";
      json += "\"ay\":" + String(curAy) + ",";
      json += "\"az\":" + String(curAz) + ",";
      json += "\"yaw\":" + String(curYaw) + ",";
      json += "\"roll\":" + String(curRoll) + ",";
      json += "\"pitch\":" + String(curPitch);
      json += "}";
      
      // MQTT 서버로 발송!
      client.publish(mqtt_topic, json.c_str());
      Serial.println("Published: " + json);
    }
  }
}
