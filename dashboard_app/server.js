const http = require('http');
const fs = require('fs');
const path = require('path');
const { Aedes } = require('aedes');
const aedes = new Aedes();
const net = require('net');
const ws = require('websocket-stream');

// --- 1. HTTP 웹 서버 (대시보드 서빙) ---
const PORT = process.env.PORT || 8080;
const PUBLIC_DIR = path.join(__dirname, 'public');

const httpServer = http.createServer((req, res) => {
  let filePath = path.join(PUBLIC_DIR, req.url === '/' ? 'index.html' : req.url);
  let extname = path.extname(filePath);
  let contentType = 'text/html';

  if (extname === '.js') contentType = 'text/javascript';
  if (extname === '.css') contentType = 'text/css';
  if (extname === '.json') contentType = 'application/json';

  fs.readFile(filePath, (err, content) => {
    if (err) {
      res.writeHead(404);
      res.end('404 Not Found');
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content, 'utf-8');
    }
  });
});

httpServer.listen(PORT, () => {
  console.log(`====================================================`);
  console.log(`🌐 HTTP 웹 대시보드 서버 가동 중 (포트: ${PORT})`);
  console.log(`👉 로컬 접속: http://localhost:${PORT}/team_dashboard.html`);
  console.log(`====================================================`);
});

// --- 2. MQTT Broker (TCP: 보드 연결용) ---
const MQTT_PORT = 1883;
const mqttServer = net.createServer(aedes.handle);
mqttServer.listen(MQTT_PORT, () => {
  console.log(`📡 MQTT Broker (TCP) 가동 중 (포트: ${MQTT_PORT})`);
  console.log(`👉 ESP32 등 보드들이 접속할 포트입니다.`);
});

// --- 3. MQTT Broker (WebSocket: 웹 브라우저 연결용) ---
const WS_PORT = 8000;
const wsServer = http.createServer();
ws.createServer({ server: wsServer }, aedes.handle);
wsServer.listen(WS_PORT, () => {
  console.log(`🕸️ MQTT Broker (WebSocket) 가동 중 (포트: ${WS_PORT})`);
  console.log(`👉 웹 대시보드가 실시간 데이터를 받을 포트입니다.`);
  console.log(`====================================================`);
});

// 클라이언트 접속/해제 로깅
aedes.on('client', (client) => {
  console.log(`[MQTT] Client Connected: ${client ? client.id : client}`);
});
aedes.on('clientDisconnect', (client) => {
  console.log(`[MQTT] Client Disconnected: ${client ? client.id : client}`);
});
