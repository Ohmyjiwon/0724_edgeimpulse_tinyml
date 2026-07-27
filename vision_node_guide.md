# 📸 팀원을 위한 비전(카메라) 노드 연동 가이드

안녕하세요! 카메라 객체 인식(Object Detection) 파트를 담당하신 팀원 분께 드리는 연동 가이드입니다. 
우리의 팀 프로젝트 통합 대시보드는 **MQTT 통신**을 통해 두 보드의 데이터를 받아옵니다. 

아래 내용을 참고하셔서 작성하신 카메라 추론 코드(`ino` 파일)의 `loop()` 마지막 부분에 MQTT 전송 로직을 추가해 주세요!

---

## 1. 라이브러리 설치
아두이노 IDE의 라이브러리 매니저에서 **`PubSubClient`** (by Nick O'Leary)를 검색해서 설치해 주세요.

## 2. 기본 세팅 (전역 변수)
코드 상단 헤더 부분에 아래 코드를 추가합니다.
```cpp
#include <WiFi.h>
#include <PubSubClient.h>

// 와이파이 설정 (대시보드가 띄워진 환경과 통신하기 위함, 핫스팟도 가능)
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

// 공용 MQTT 브로커 설정
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// ⚠️ 중요: 비전 노드 전용 토픽입니다!
const char* mqtt_topic = "teamX/node2/vision"; 

WiFiClient espClient;
PubSubClient client(espClient);
```

## 3. Wi-Fi 및 MQTT 연결 함수 (setup 아래에 추가)
```cpp
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected!");
}

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
```

## 4. `setup()` 함수에 추가할 내용
```cpp
void setup() {
  // 기존 카메라 및 Edge Impulse 초기화 코드...
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}
```

## 5. `loop()` 추론 결과 전송 로직 (추론 성공 시점)
Edge Impulse의 `run_classifier()`가 성공하고, 가장 확률이 높은 `best_label`과 `best_confidence`를 구하셨을 겁니다. 그 직후에 아래 코드를 넣어주세요!

```cpp
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // 기존 카메라 캡처 및 run_classifier 로직...
  
  // 추론이 끝난 후 (예: 1초에 한 번만 전송되도록 delay나 타이머 사용 권장)
  // 가장 높은 확률의 라벨과 점수를 찾았다고 가정합니다.
  String best_label = "apple";  // 예시 (실제 찾은 사물 변수 대입)
  float best_confidence = 0.95; // 예시 (실제 확률 변수 대입)
  
  // JSON 문자열 만들기 (이 형식을 맞춰주셔야 대시보드에 예쁘게 뜹니다!)
  String json = "{";
  json += "\"type\":\"vision\",";
  json += "\"label\":\"" + best_label + "\",";
  json += "\"confidence\":" + String(best_confidence, 2);
  json += "}";
  
  // MQTT 서버로 발송!
  client.publish(mqtt_topic, json.c_str());
  Serial.println("전송 완료: " + json);
}
```

끝입니다! 위 코드만 추가해서 플래싱하시면 대시보드 우측 패널에 카메라가 인식한 데이터가 실시간으로 쏙쏙 들어옵니다! 화이팅! 🚀
