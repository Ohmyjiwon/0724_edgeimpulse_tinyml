#include <Oh_Test_inferencing.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

WebServer server(80);

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static int get_feature_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

static const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BNO055 Live Motion Classifier</title>
<style>
body{font-family:sans-serif;background:#12121a;color:#eee;text-align:center;margin:0;padding:16px}
.card{background:#1f1f2e;border-radius:16px;padding:24px;max-width:420px;margin:20px auto;box-shadow:0 8px 30px rgba(0,0,0,0.5)}
#mainLabel{font-size:36px;font-weight:bold;margin:15px 0;color:#ffcc00;transition:all 0.3s}
.bar{display:flex;align-items:center;margin:8px auto;width:95%;font-size:15px}
.bar span{width:80px;text-align:right;padding-right:10px;font-weight:bold}
.bar .track{flex:1;background:#333;border-radius:6px;height:20px;overflow:hidden}
.bar .fill{height:100%;background:#4c8bf5;width:0%;transition:width 0.2s}
.bar b{width:50px;text-align:left;padding-left:8px}
#lat{color:#888;font-size:13px;margin-top:16px}
</style></head><body>
<div class="card">
<h2>🌀 BNO055 모션 인식 (AP 모드)</h2>
<div id="mainLabel">측정 중...</div>
<div id="bars"></div>
<div id="lat"></div>
</div>
<script>
const COLORS = { idle: '#ff4757', walk: '#ffa502', run: '#2ed573' };
const LABELS_KR = { idle: '🛑 멈춤 (Idle)', walk: '🚶 걷기 (Walk)', run: '🏃 뛰기 (Run)' };

async function tick() {
  try {
    const r = await fetch('/classify');
    const d = await r.json();
    let html = '';
    for (const [k, v] of Object.entries(d.scores)) {
      const c = COLORS[k] || '#4c8bf5';
      const pct = (v * 100).toFixed(0);
      html += `<div class="bar"><span>${k}</span><div class="track"><div class="fill" style="width:${pct}%;background:${c}"></div></div><b>${pct}%</b></div>`;
    }
    document.getElementById('bars').innerHTML = html;
    const kr = LABELS_KR[d.label] || d.label;
    const mainEl = document.getElementById('mainLabel');
    mainEl.textContent = kr;
    mainEl.style.color = COLORS[d.label] || '#ffcc00';
    document.getElementById('lat').textContent = `DSP: ${d.dsp_ms}ms | Classification: ${d.nn_ms}ms`;
  } catch(e) {}
  setTimeout(tick, 150);
}
tick();
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleClassify() {
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &get_feature_data;
  
  ei_impulse_result_t result = {0};
  if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) {
    server.send(500, "application/json", "{\"error\":\"classifier_failed\"}");
    return;
  }

  int best = 0;
  String json = "{\"scores\":{";
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > result.classification[best].value) best = i;
    json += "\"" + String(ei_classifier_inferencing_categories[i]) + "\":" + String(result.classification[i].value, 3);
    if (i < EI_CLASSIFIER_LABEL_COUNT - 1) json += ",";
  }
  json += "},\"label\":\"" + String(ei_classifier_inferencing_categories[best]) + "\"";
  json += ",\"confidence\":" + String(result.classification[best].value, 3);
  json += ",\"dsp_ms\":" + String(result.timing.dsp);
  json += ",\"nn_ms\":" + String(result.timing.classification) + "}";
  
  server.send(200, "application/json", json);
}

unsigned long lastSample = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Wire.begin(5, 6);
  
  // Set BNO055 to NDOF mode (Operation Mode Reg 0x3D = 0x0C)
  Wire.beginTransmission(0x29);
  Wire.write(0x3D);
  Wire.write(0x0C);
  Wire.endTransmission();
  delay(50);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Ohzi_esp", "fivesupport", 1);
  
  server.on("/", handleRoot);
  server.on("/classify", handleClassify);
  server.begin();
  Serial.println("BNO055 Live Inference AP ready: http://192.168.4.1");
}

void loop() {
  server.handleClient();
  
  if (millis() - lastSample >= 20) { // 50 Hz
    lastSample = millis();
    
    int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    
    Wire.beginTransmission(0x29);
    Wire.write(0x08);
    Wire.endTransmission();
    Wire.requestFrom(0x29, 6);
    if (Wire.available() >= 6) {
      ax = Wire.read() | (Wire.read() << 8);
      ay = Wire.read() | (Wire.read() << 8);
      az = Wire.read() | (Wire.read() << 8);
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
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6] = (float)ax;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 5] = (float)ay;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 4] = (float)az;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3] = (float)gx;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2] = (float)gy;
    features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = (float)gz;
  }
}
