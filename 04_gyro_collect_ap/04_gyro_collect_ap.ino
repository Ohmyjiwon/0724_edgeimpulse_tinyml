#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

WebServer server(80);

const int SAMPLES = 250; // 5 seconds at 50Hz (every 20ms)
int16_t buffer[SAMPLES][6]; // AccX, AccY, AccZ, GyrX, GyrY, GyrZ
bool isRecording = false;
int recordedSamples = 0;
String currentLabel = "idle";

static const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BNO055 Gyro Data Collector</title>
<style>
body{font-family:sans-serif;background:#181824;color:#eee;text-align:center;padding:20px;margin:0}
.card{background:#232334;border-radius:12px;padding:20px;max-width:400px;margin:10px auto;box-shadow:0 4px 15px rgba(0,0,0,0.3)}
button{background:#4c8bf5;color:#fff;border:none;padding:12px 24px;font-size:16px;border-radius:8px;cursor:pointer;margin:8px;font-weight:bold;width:85%}
button:disabled{background:#555;cursor:not-allowed}
select{padding:12px;font-size:16px;border-radius:8px;width:85%;margin-bottom:12px;background:#333;color:#fff;border:1px solid #555}
#status{font-weight:bold;font-size:18px;margin:15px 0;color:#ffcc00}
#progress{font-size:24px;color:#4cf58b;font-weight:bold;margin-top:10px}
</style></head><body>
<div class="card">
<h2>🌀 BNO055 자이로/가속도 데이터 수집기</h2>
<p>수집할 동작 라벨을 선택하고 녹화 시작을 누르세요.</p>
<select id="labelSelect">
  <option value="idle">🛑 멈춤 (idle)</option>
  <option value="walk">🚶 걷기 (walk)</option>
  <option value="run">🏃 뛰기 (run)</option>
</select><br>
<button id="recBtn" onclick="startRecord()">⏺️ 5초간 수집 시작</button>
<div id="status">준비 완료</div>
<div id="progress"></div>
<br>
<button id="dlBtn" onclick="downloadCSV()" style="background:#28a745;display:none">📥 CSV 다운로드 (Edge Impulse용)</button>
</div>
<script>
let label = 'idle';
async function startRecord() {
  label = document.getElementById('labelSelect').value;
  document.getElementById('recBtn').disabled = true;
  document.getElementById('dlBtn').style.display = 'none';
  document.getElementById('status').textContent = '수집 중... 보드를 움직이세요!';
  await fetch('/start?label=' + label);
  let poll = setInterval(async () => {
    let r = await fetch('/status');
    let d = await r.json();
    document.getElementById('progress').textContent = d.count + ' / 250 샘플';
    if (!d.recording) {
      clearInterval(poll);
      document.getElementById('status').textContent = '✅ 5초 수집 완료!';
      document.getElementById('recBtn').disabled = false;
      document.getElementById('dlBtn').style.display = 'inline-block';
    }
  }, 150);
}
function downloadCSV() {
  window.location.href = '/csv';
}
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleStart() {
  if (server.hasArg("label")) currentLabel = server.arg("label");
  recordedSamples = 0;
  isRecording = true;
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleStatus() {
  String json = "{\"recording\":" + String(isRecording ? "true" : "false") + ",\"count\":" + String(recordedSamples) + "}";
  server.send(200, "application/json", json);
}

void handleCSV() {
  String csv = "timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ\n";
  for (int i = 0; i < recordedSamples; i++) {
    csv += String(i * 20) + "," + String(buffer[i][0]) + "," + String(buffer[i][1]) + "," + String(buffer[i][2]) + "," + String(buffer[i][3]) + "," + String(buffer[i][4]) + "," + String(buffer[i][5]) + "\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=" + currentLabel + "." + String(millis()) + ".csv");
  server.send(200, "text/csv", csv);
}

unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Wire.begin(5, 6);
  
  // Set BNO055 to NDOF mode (Operation Mode Reg 0x3D = 0x0C NDOF)
  Wire.beginTransmission(0x29);
  Wire.write(0x3D);
  Wire.write(0x0C);
  Wire.endTransmission();
  delay(50);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Ohzi_esp", "fivesupport", 1);
  
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/status", handleStatus);
  server.on("/csv", handleCSV);
  server.begin();
  Serial.println("Gyro BNO055 Collector AP ready: http://192.168.4.1");
}

void loop() {
  server.handleClient();
  
  if (isRecording) {
    if (millis() - lastSampleTime >= 20) {
      lastSampleTime = millis();
      Wire.beginTransmission(0x29);
      Wire.write(0x08);
      Wire.endTransmission();
      Wire.requestFrom(0x29, 6);
      if (Wire.available() >= 6) {
        buffer[recordedSamples][0] = Wire.read() | (Wire.read() << 8);
        buffer[recordedSamples][1] = Wire.read() | (Wire.read() << 8);
        buffer[recordedSamples][2] = Wire.read() | (Wire.read() << 8);
      }
      Wire.beginTransmission(0x29);
      Wire.write(0x14);
      Wire.endTransmission();
      Wire.requestFrom(0x29, 6);
      if (Wire.available() >= 6) {
        buffer[recordedSamples][3] = Wire.read() | (Wire.read() << 8);
        buffer[recordedSamples][4] = Wire.read() | (Wire.read() << 8);
        buffer[recordedSamples][5] = Wire.read() | (Wire.read() << 8);
      }
      recordedSamples++;
      if (recordedSamples >= SAMPLES) {
        isRecording = false;
      }
    }
  }
}
