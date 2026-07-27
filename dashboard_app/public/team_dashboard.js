// MQTT Broker settings
const brokerUrl = "broker.hivemq.com";
const brokerPort = 8000;
const clientId = "TeamDashboard_" + Math.random().toString(16).substr(2, 8);

const topicMotion = "teamX/node1/motion";
const topicVision = "teamX/node2/vision";

// UI Elements
const mqttStatus = document.getElementById('mqttStatus');
const motionTime = document.getElementById('motionTime');
const visionTime = document.getElementById('visionTime');

// Motion Elements
const motionLabel = document.getElementById('motionLabel');
const motionConfidenceBar = document.getElementById('motionConfidenceBar');
const motionConfidenceText = document.getElementById('motionConfidenceText');
const accX = document.getElementById('accX');
const accY = document.getElementById('accY');
const accZ = document.getElementById('accZ');
const pitchUI = document.getElementById('pitch');
const rollUI = document.getElementById('roll');
const yawUI = document.getElementById('yaw');

// Vision Elements
const visionLabel = document.getElementById('visionLabel');
const visionConfidenceBar = document.getElementById('visionConfidenceBar');
const visionConfidenceText = document.getElementById('visionConfidenceText');
const visionLog = document.getElementById('visionLog');

// Create MQTT Client
const client = new Paho.MQTT.Client(brokerUrl, brokerPort, "/mqtt", clientId);

// Set Callbacks
client.onConnectionLost = onConnectionLost;
client.onMessageArrived = onMessageArrived;

// Connect to Broker
const connectOptions = {
  timeout: 3,
  onSuccess: onConnect,
  onFailure: onFailure,
  useSSL: true
};

console.log("Connecting to " + brokerUrl + ":" + brokerPort);
client.connect(connectOptions);

function onConnect() {
  console.log("Connected to MQTT broker!");
  mqttStatus.className = "status-badge connected";
  mqttStatus.innerText = "🟢 MQTT Connected";
  
  // Subscribe to Topics
  client.subscribe(topicMotion);
  client.subscribe(topicVision);
  console.log(`Subscribed to:\n- ${topicMotion}\n- ${topicVision}`);
}

function onFailure(error) {
  console.error("MQTT Connection Failed:", error);
  mqttStatus.className = "status-badge error";
  mqttStatus.innerText = "🔴 Connection Failed";
  setTimeout(() => client.connect(connectOptions), 5000); // Reconnect loop
}

function onConnectionLost(responseObject) {
  if (responseObject.errorCode !== 0) {
    console.warn("Connection Lost:", responseObject.errorMessage);
    mqttStatus.className = "status-badge error";
    mqttStatus.innerText = "🔴 Disconnected";
    setTimeout(() => client.connect(connectOptions), 5000); // Reconnect loop
  }
}

function onMessageArrived(message) {
  const topic = message.destinationName;
  const payload = message.payloadString;
  const now = new Date().toLocaleTimeString();

  try {
    const data = JSON.parse(payload);

    if (topic === topicMotion) {
      updateMotionUI(data, now);
    } else if (topic === topicVision) {
      updateVisionUI(data, now);
    }
  } catch (e) {
    console.error("Invalid JSON from topic", topic, ":", payload);
  }
}

function updateMotionUI(data, timeStr) {
  motionTime.innerText = timeStr;
  
  // Update Inference
  if (data.label) {
    let icon = "❓";
    if (data.label === "idle") icon = "🛑";
    if (data.label === "walk") icon = "🚶";
    if (data.label === "run") icon = "🏃";
    motionLabel.innerText = `${icon} ${data.label.toUpperCase()}`;
  }
  
  if (data.confidence !== undefined) {
    const pct = Math.round(data.confidence * 100);
    motionConfidenceBar.style.width = pct + "%";
    motionConfidenceText.innerText = "Confidence: " + pct + "%";
  }

  // Update Sensors
  if (data.ax !== undefined) accX.innerText = data.ax;
  if (data.ay !== undefined) accY.innerText = data.ay;
  if (data.az !== undefined) accZ.innerText = data.az;
  
  if (data.pitch !== undefined) pitchUI.innerText = (data.pitch/16.0).toFixed(1) + "°";
  if (data.roll !== undefined) rollUI.innerText = (data.roll/16.0).toFixed(1) + "°";
  if (data.yaw !== undefined) yawUI.innerText = (data.yaw/16.0).toFixed(1) + "°";
}

function updateVisionUI(data, timeStr) {
  visionTime.innerText = timeStr;
  
  // Update Log Box with raw JSON
  visionLog.innerText = JSON.stringify(data, null, 2);

  // Parse label & confidence (Assume partner sends similar structure)
  if (data.label) {
    visionLabel.innerText = `🔍 ${data.label.toUpperCase()}`;
  } else if (data.class) { // fallback common keys
    visionLabel.innerText = `🔍 ${data.class.toUpperCase()}`;
  }
  
  if (data.confidence !== undefined) {
    const pct = data.confidence <= 1.0 ? Math.round(data.confidence * 100) : Math.round(data.confidence);
    visionConfidenceBar.style.width = pct + "%";
    visionConfidenceText.innerText = "Confidence: " + pct + "%";
  } else if (data.score !== undefined) {
    const pct = data.score <= 1.0 ? Math.round(data.score * 100) : Math.round(data.score);
    visionConfidenceBar.style.width = pct + "%";
    visionConfidenceText.innerText = "Confidence: " + pct + "%";
  }
}
