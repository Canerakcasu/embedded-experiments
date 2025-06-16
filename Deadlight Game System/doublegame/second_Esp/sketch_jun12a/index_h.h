const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Game Master - Full Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    body { margin: 0; background-color: #2c3e50; color: #ecf0f1; }
    h1 { font-size: 1.8rem; color: white; }
    .content { padding: 20px; }
    .card-grid { max-width: 1200px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); }
    .card { background-color: #34495e; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.5); border-radius: 10px; padding: 20px; text-align: left;}
    .card-title { font-size: 1.2rem; font-weight: bold; margin-bottom: 15px; border-bottom: 1px solid #7f8c8d; padding-bottom: 10px;}
    .status-item, .setting-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; font-size: 1rem; }
    .status-label { color: #bdc3c7; }
    .status-value { font-weight: bold; color: #1abc9c; }
    .button-group { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    button { background-color: #3498db; color: white; border: none; padding: 12px 15px; border-radius: 5px; font-size: 1rem; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #2980b9; }
    .btn-danger { background-color: #e74c3c; }
    .btn-danger:hover { background-color: #c0392b; }
    input[type="range"], input[type="number"] { width: 120px; background-color: #2c3e50; color: white; border: 1px solid #7f8c8d; padding: 5px; border-radius: 3px;}
  </style>
</head>
<body>
  <div class="content">
    <h1>ESP32 Hardware Control Panel (ESP2)</h1>
    <div class="card-grid">
      
      <div class="card">
        <div class="card-title">System Status</div>
        <div class="status-item"><span class="status-label">Game Mode:</span><span id="gameMode" class="status-value">Connecting...</span></div>
        <div class="status-item"><span class="status-label">Button State:</span><span id="buttonState" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Last RFID UID:</span><span id="rfidStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">LED Brightness:</span><span id="ledBrightness" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Mic Status:</span><span id="micStatus" class="status-value">-</span></div>
      </div>

      <div class="card">
        <div class="card-title">Motor Status</div>
        <div class="status-item"><span class="status-label">Position:</span><span id="motorPosition" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Speed:</span><span id="motorSpeed" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Is Running?:</span><span id="motorRunning" class="status-value">-</span></div>
      </div>
      
      <div class="card">
        <div class="card-title">Main Controls</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'startGame'})">Start / Restart Full Sequence</button>
          <button class="btn-danger" onclick="sendCommand({action:'resetSystem'})">Reset ESP32 CPU</button>
        </div>
        <div class="card-title" style="margin-top: 20px;">Manual Motor</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'moveMotor', steps: 200})">Move +360&deg;</button>
          <button onclick="sendCommand({action:'moveMotor', steps: -200})">Move -360&deg;</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Settings</div>
        <div class="setting-item">
          <label for="voltmeterSpeedSlider">Voltmeter Speed (ms):</label>
          <input type="range" id="voltmeterSpeedSlider" min="10" max="200" value="50" onchange="sendSetting('setVoltmeterSpeed', this.value)">
          <span id="voltmeterSpeedValue">-</span>
        </div>
      </div>

    </div>
  </div>

<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onload);

  function onload(event) {
    initWebSocket();
  }

  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen  = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
  }

  function onOpen(event) {
    console.log('Connection opened');
  }

  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }

  function onMessage(event) {
    var data = JSON.parse(event.data);
    document.getElementById('gameMode').innerHTML = data.gameMode;
    document.getElementById('buttonState').innerHTML = data.buttonState;
    document.getElementById('rfidStatus').innerHTML = data.rfidStatus;
    document.getElementById('ledBrightness').innerHTML = data.ledBrightness;
    document.getElementById('micStatus').innerHTML = data.micStatus;
    document.getElementById('motorPosition').innerHTML = data.motorPosition;
    document.getElementById('motorSpeed').innerHTML = data.motorSpeed;
    document.getElementById('motorRunning').innerHTML = data.motorRunning ? "Yes" : "No";
    
    var speedSlider = document.getElementById('voltmeterSpeedSlider');
    var speedValueLabel = document.getElementById('voltmeterSpeedValue');
    if (document.activeElement !== speedSlider) {
        speedSlider.value = data.voltmeterSpeed;
    }
    speedValueLabel.innerHTML = speedSlider.value;
  }

  function sendCommand(command) {
    console.log("Sending command: ", command);
    websocket.send(JSON.stringify(command));
  }
  
  function sendSetting(action, value) {
      let payload = parseInt(value);
      console.log(`Sending setting: ${action} = ${payload}`);
      sendCommand({ action: action, value: payload });

      if (action === 'setVoltmeterSpeed') {
          document.getElementById('voltmeterSpeedValue').innerHTML = value;
      }
  }
</script>
</body>
</html>
)rawliteral";