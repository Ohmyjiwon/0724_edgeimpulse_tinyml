const canvas = document.getElementById('drawCanvas');
const ctx = canvas.getContext('2d');
const pencilCursor = document.getElementById('pencilCursor');
const statusBadge = document.getElementById('status');
const valPitch = document.getElementById('valPitch');
const valRoll = document.getElementById('valRoll');
const accuracyLabel = document.getElementById('accuracy');

let isConnected = false;
let currentShape = 'circle';
let tracePath = [];
let targetPath = [];

// Canvas dimensions
const W = canvas.width;
const H = canvas.height;

// Pencil Position & Smoothing
let penX = W / 2;
let penY = H / 2;
let targetPenX = W / 2;
let targetPenY = H / 2;

// Calibration Offsets
let offsetPitch = 0;
let offsetRoll = 0;

// Shape Definitions
function generateCircle() {
  targetPath = [];
  const cx = W / 2;
  const cy = H / 2;
  const r = 150;
  for (let i = 0; i <= 360; i += 5) {
    targetPath.push({
      x: cx + r * Math.cos(i * Math.PI / 180),
      y: cy + r * Math.sin(i * Math.PI / 180)
    });
  }
}

function generateEllipse() {
  targetPath = [];
  const cx = W / 2;
  const cy = H / 2;
  const rx = 250;
  const ry = 100;
  for (let i = 0; i <= 360; i += 5) {
    targetPath.push({
      x: cx + rx * Math.cos(i * Math.PI / 180),
      y: cy + ry * Math.sin(i * Math.PI / 180)
    });
  }
}

function generateSquare() {
  targetPath = [];
  const cx = W / 2;
  const cy = H / 2;
  const size = 300;
  const half = size / 2;
  // Top
  for(let x = cx - half; x <= cx + half; x+=10) targetPath.push({x, y: cy - half});
  // Right
  for(let y = cy - half; y <= cy + half; y+=10) targetPath.push({x: cx + half, y});
  // Bottom
  for(let x = cx + half; x >= cx - half; x-=10) targetPath.push({x, y: cy + half});
  // Left
  for(let y = cy + half; y >= cy - half; y-=10) targetPath.push({x: cx - half, y});
}

function renderScene() {
  ctx.clearRect(0, 0, W, H);

  // Draw Target Shape (Dotted)
  if (targetPath.length > 0) {
    ctx.beginPath();
    ctx.setLineDash([10, 15]);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
    ctx.lineWidth = 4;
    ctx.moveTo(targetPath[0].x, targetPath[0].y);
    for (let i = 1; i < targetPath.length; i++) {
      ctx.lineTo(targetPath[i].x, targetPath[i].y);
    }
    ctx.closePath();
    ctx.stroke();
  }

  // Draw Trace Path
  if (tracePath.length > 0) {
    ctx.beginPath();
    ctx.setLineDash([]);
    ctx.strokeStyle = '#3b82f6';
    ctx.lineWidth = 6;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.moveTo(tracePath[0].x, tracePath[0].y);
    for (let i = 1; i < tracePath.length; i++) {
      ctx.lineTo(tracePath[i].x, tracePath[i].y);
    }
    ctx.stroke();
  }
  
  // Calculate accuracy occasionally
  if (tracePath.length > 10 && targetPath.length > 0) {
    let totalDist = 0;
    for (let p of tracePath) {
      let minDist = 9999;
      for (let t of targetPath) {
        let d = Math.hypot(p.x - t.x, p.y - t.y);
        if (d < minDist) minDist = d;
      }
      totalDist += minDist;
    }
    let avgDist = totalDist / tracePath.length;
    let acc = Math.max(0, 100 - (avgDist / 2)); // rough metric
    accuracyLabel.innerText = acc.toFixed(1) + '%';
  } else {
    accuracyLabel.innerText = '0%';
  }
}

function updatePencilCursor(pitch, roll) {
  // BNO055 returns values * 16 for degrees
  let rawPitchDeg = pitch / 16.0;
  let rawRollDeg = roll / 16.0;
  
  // Apply calibration offsets
  let pitchDeg = rawPitchDeg - offsetPitch;
  let rollDeg = rawRollDeg - offsetRoll;
  
  valPitch.innerText = pitchDeg.toFixed(1);
  valRoll.innerText = rollDeg.toFixed(1);

  // Map angles to screen coordinates
  // Increased sensitivity: mapping -30 to +30 degrees instead of 45
  let r = Math.max(-30, Math.min(30, rollDeg));
  let p = Math.max(-30, Math.min(30, pitchDeg));

  targetPenX = ((r + 30) / 60) * W; 
  targetPenY = ((p + 30) / 60) * H;
  
  // Reverse Y because screen Y goes down, pitch might go up
  targetPenY = H - targetPenY;

  // EMA Smoothing (Brush Stabilizer Effect)
  penX = penX * 0.7 + targetPenX * 0.3;
  penY = penY * 0.7 + targetPenY * 0.3;

  // Move Pencil UI
  pencilCursor.style.left = penX + 'px';
  pencilCursor.style.top = penY + 'px';

  // Add to trace
  if (isConnected) {
    tracePath.push({x: penX, y: penY});
    if (tracePath.length > 500) tracePath.shift(); 
  }
  
  requestAnimationFrame(renderScene);
}

// Button Listeners
document.getElementById('btnCircle').onclick = () => { currentShape = 'circle'; generateCircle(); renderScene(); };
document.getElementById('btnEllipse').onclick = () => { currentShape = 'ellipse'; generateEllipse(); renderScene(); };
document.getElementById('btnSquare').onclick = () => { currentShape = 'square'; generateSquare(); renderScene(); };
document.getElementById('btnClear').onclick = () => { tracePath = []; renderScene(); };
document.getElementById('btnCenter').onclick = () => { 
  offsetPitch = parseFloat(valPitch.innerText) + offsetPitch;
  offsetRoll = parseFloat(valRoll.innerText) + offsetRoll;
  tracePath = []; 
  renderScene();
};

// BLE Connection Logic
const SERVICE_UUID = '0000ffe0-0000-1000-8000-00805f9b34fb';
const CHARACTERISTIC_UUID = '0000ffe1-0000-1000-8000-00805f9b34fb';
let bleDevice, bleCharacteristic;

document.getElementById('btnConnect').onclick = async () => {
  if (navigator.bluetooth) {
    try {
      bleDevice = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'Ohzi_ESP32S3_Motion' }],
        optionalServices: [SERVICE_UUID]
      });
      const server = await bleDevice.gatt.connect();
      const service = await server.getPrimaryService(SERVICE_UUID);
      bleCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);
      await bleCharacteristic.startNotifications();
      
      bleCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
        const value = new TextDecoder().decode(event.target.value);
        try {
          const data = JSON.parse(value);
          if (data.pitch !== undefined && data.roll !== undefined) {
             updatePencilCursor(data.pitch, data.roll);
          }
        } catch (e) {}
      });

      bleDevice.addEventListener('gattserverdisconnected', () => {
        statusBadge.className = 'status badge-idle';
        statusBadge.innerText = 'Disconnected';
        isConnected = false;
      });

      statusBadge.className = 'status connected';
      statusBadge.innerText = 'BLE Connected';
      isConnected = true;
      tracePath = []; // reset trace on connect
    } catch (err) {
      console.error(err);
      startAPPollingFallback();
    }
  } else {
    startAPPollingFallback();
  }
};

let apInterval;
function startAPPollingFallback() {
  statusBadge.innerText = 'Connecting Wi-Fi AP...';
  if (apInterval) clearInterval(apInterval);
  apInterval = setInterval(async () => {
    try {
      const res = await fetch('http://192.168.4.1/classify', { signal: AbortSignal.timeout(400) });
      const data = await res.json();
      if (data.pitch !== undefined && data.roll !== undefined) {
        updatePencilCursor(data.pitch, data.roll);
      }
      if (!isConnected) {
        statusBadge.className = 'status connected';
        statusBadge.innerText = 'AP Connected';
        isConnected = true;
        tracePath = [];
      }
    } catch (e) {
      if (isConnected) {
        statusBadge.className = 'status badge-idle';
        statusBadge.innerText = 'Disconnected';
        isConnected = false;
      }
    }
  }, 200);
}

// Initial setup
generateCircle();
renderScene();
