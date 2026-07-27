# 🏃 팀원을 위한 제스처(모션) 노드 연동 가이드

안녕하세요! 자이로/제스처(원 그리기, 좌우 흔들기) 인식을 담당하신 팀원 분께 드리는 연동 가이드입니다. 
우리의 팀 프로젝트 통합 대시보드는 인터넷 없이 동작하는 **오프라인 로컬 MQTT 통신**으로 두 보드의 데이터를 받아옵니다.

아래 내용을 참고하셔서 작성하신 코드(`ino` 파일)를 수정해 주세요!

---

## 1. 라이브러리 설치
아두이노 IDE의 라이브러리 매니저에서 **`PubSubClient`** (by Nick O'Leary)를 검색해서 설치해 주세요.

## 2. 기본 세팅 (전역 변수)
코드 상단 헤더 부분에 아래 코드를 추가합니다.
```cpp
#include <WiFi.h>
#include <PubSubClient.h>

// ⚠️ 중요: 지원(팀장)이 켜둔 핫스팟에 연결해야 합니다!
const char* ssid = "Ohzi_esp";
const char* password = "fivesupport";

// ⚠️ 중요: 지원(팀장)의 노트북(서버) IP 주소를 입력해야 합니다!
// (지원이가 핫스팟을 켜고 본인 IP를 확인해서 알려줄 것입니다. 예: 192.168.137.1)
const char* mqtt_server = "여기에_노트북_IP_입력";
const int mqtt_port = 1883;

// ⚠️ 중요: 제스처 노드 전용 토픽입니다!
const char* mqtt_topic = "teamX/node1/motion"; 

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
    String clientId = "MotionNode-";
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
  // 기존 IMU 및 Edge Impulse 초기화 코드...
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}
```

## 5. `loop()` 추론 결과 전송 로직 (추론 성공 시점)
Edge Impulse의 `run_classifier()`가 성공하고, 가장 확률이 높은 `best_label`과 `best_confidence`를 구하셨을 겁니다. 

```cpp
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // 기존 IMU 데이터 수집 및 run_classifier 로직...
  
  // 추론이 끝난 후 (예: 1초에 한 번만 전송되도록 delay나 타이머 사용 권장)
  // 가장 높은 확률의 라벨(예: "circle" 또는 "shake")과 점수를 찾았다고 가정합니다.
  String best_label = "circle";  // 예시 (실제 찾은 제스처 변수 대입)
  float best_confidence = 0.95;  // 예시 (실제 확률 변수 대입)
  
  // JSON 문자열 만들기 (대시보드가 이 형식을 파싱합니다!)
  String json = "{";
  json += "\"type\":\"motion\",";
  json += "\"label\":\"" + best_label + "\",";
  json += "\"confidence\":" + String(best_confidence, 2);
  json += "}";
  
  // 노트북 서버로 발송!
  client.publish(mqtt_topic, json.c_str());
  Serial.println("전송 완료: " + json);
}
```

끝입니다! 노트북 서버가 켜져 있는 상태에서 보드 전원을 켜면, 로컬 네트워크를 타고 대시보드 좌측(모션 패널)에 데이터가 꽂히게 됩니다! 🚀
