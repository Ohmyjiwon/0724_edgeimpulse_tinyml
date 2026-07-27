# 📸 나의 비전(카메라) 노드 연동 가이드

지원(팀장)님이 이전에 만들어두신 **"안경 & 에어팟 카메라 인식"** 코드를 통합 대시보드에 연동하기 위해 추가해야 할 MQTT 코드입니다.
기존에 쓰시던 카메라 `.ino` 파일에 아래 내용을 추가해 주세요!

---

## 1. 라이브러리 추가
기존 카메라 코드 맨 위에 아래 두 줄을 추가합니다.
```cpp
#include <WiFi.h>
#include <PubSubClient.h>

// ⚠️ 대시보드를 띄운 노트북과 동일한 와이파이(또는 핫스팟) 이름
const char* ssid = "Ohzi_esp";
const char* password = "fivesupport";

// ⚠️ 로컬 서버(노트북) IP 주소
const char* mqtt_server = "192.168.0.45";
const int mqtt_port = 1883;

// ⚠️ 중요: 내 카메라 전용 토픽입니다!
const char* mqtt_topic = "teamX/node2/vision"; 

WiFiClient espClient;
PubSubClient client(espClient);
```

## 2. 셋업 함수
`setup()` 함수 안에 아두이노가 와이파이와 MQTT 서버에 접속하도록 코드를 넣습니다.
```cpp
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected!");
}

void setup() {
  // 기존 카메라 & 엣지 임펄스 초기화 코드...
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}
```

## 3. 루프 함수 (데이터 쏘기)
카메라로 사진을 찍고 `run_classifier()` 함수가 돌아가서 "안경"이나 "에어팟"을 인식한 직후에 아래 코드를 넣습니다.

```cpp
void reconnect() {
  while (!client.connected()) {
    String clientId = "VisionNode-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT Connected!");
    } else {
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // 기존 카메라 캡처 및 추론(run_classifier) 코드...
  
  // 추론이 끝난 후, 가장 확률이 높은 라벨을 찾습니다.
  String best_label = "airpods";  // 예시: 실제 결과 변수(ei_classifier_inferencing_categories[i]) 대입
  float best_confidence = 0.98;   // 예시: 실제 확률 대입
  
  // JSON 문자열 만들기
  String json = "{";
  json += "\"type\":\"vision\",";
  json += "\"label\":\"" + best_label + "\",";
  json += "\"confidence\":" + String(best_confidence, 2);
  json += "}";
  
  // 대시보드로 발송!
  client.publish(mqtt_topic, json.c_str());
  Serial.println("비전 전송 완료: " + json);
}
```

위 코드를 지원님의 카메라 펌웨어에 합쳐서 보드에 업로드하시면, 노트북 대시보드 우측 패널에 에어팟/안경 데이터가 실시간으로 들어오기 시작합니다!
