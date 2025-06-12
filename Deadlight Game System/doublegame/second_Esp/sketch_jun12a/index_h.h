const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Game Master Control Panel</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    body { margin: 0; background-color: #2c3e50; color: #ecf0f1; }
    h1 { font-size: 1.8rem; color: white; }
    .content { padding: 20px; }
    .card-grid { max-width: 1200px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .card { background-color: #34495e; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.5); border-radius: 10px; padding: 20px; }
    .card-title { font-size: 1.2rem; font-weight: bold; margin-bottom: 10px; border-bottom: 1px solid #7f8c8d; padding-bottom: 5px;}
    .status-item, .setting-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; font-size: 1rem; }
    .status-value { font-weight: bold; color: #1abc9c; }
    .button-group { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    button { background-color: #3498db; color: white; border: none; padding: 12px 15px; border-radius: 5px; font-size: 1rem; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #2980b9; }
    .btn-danger { background-color: #e74c3c; }
    .btn-danger:hover { background-color: #c0392b; }
    input[type="range"] { width: 100%; }
  </style>
</head>
<body>
  <div class="content">
    <h1>ESP32 Game Master Control Panel</h1>
    <div class="card-grid">
      
      <div class="card">
        <div class="card-title">System Status</div>
        <div class="status-item"><span>Game Mode:</span><span id="gameMode" class="status-value">Connecting...</span></div>
        <div class="status-item"><span>Button State:</span><span id="buttonState" class="status-value">-</span></div>
        <div class="status-item"><span>Last RFID UID:</span><span id="rfidStatus" class="status-value">-</span></div>
      </div>

      <div class="card">
        <div class="card-title">Live Sensor Data</div>
        <div class="status-item"><span>Microphone (Raw):</span><span id="micRaw" class="status-value">-</span></div>
        <div class="status-item"><span>Microphone (Avg):</span><span id="micAvg" class="status-value">-</span></div>
      </div>
      
      <div class="card">
        <div class="card-title">Motor Status</div>
        <div class="status-item"><span>Position:</span><span id="motorPosition" class="status-value">-</span></div>
        <div class="status-item"><span>Speed:</span><span id="motorSpeed" class="status-value">-</span></div>
        <div class="status-item"><span>Is Running?:</span><span id="motorRunning" class="status-value">-</span></div>
      </div>
      
      <div class="card">
        <div class="card-title">Game Controls</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'startGame'})">Start / Restart Game</button>
          <button class="btn-danger" onclick="sendCommand({action:'resetSystem'})">Reset ESP32</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Manual Motor Control</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'moveMotor', steps: 50})">Move +90&deg; (50 Steps)</button>
          <button onclick="sendCommand({action:'moveMotor', steps: -50})">Move -90&deg; (-50 Steps)</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Settings</div>
        <div class="setting-item">
          <label for="voltmeterSpeedSlider">Voltmeter Speed (ms): </label>
          <span id="voltmeterSpeedValue"></span>
        </div>
        <input type="range" id="voltmeterSpeedSlider" min="10" max="200" onchange="sendSetting('setVoltmeterSpeed', this.value)">
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
    document.getElementById('micRaw').innerHTML = data.micRaw;
    document.getElementById('micAvg').innerHTML = data.micAvg;
    document.getElementById('motorPosition').innerHTML = data.motorPosition;
    document.getElementById('motorSpeed').innerHTML = data.motorSpeed;
    document.getElementById('motorRunning').innerHTML = data.motorRunning ? "Yes" : "No";
    
    // Ayarları da güncelle
    var speedSlider = document.getElementById('voltmeterSpeedSlider');
    var speedValueLabel = document.getElementById('voltmeterSpeedValue');
    
    // Sadece kullanıcı kaydırmıyorsa arayüzü güncelle. Bu, takılmayı önler.
    if (document.activeElement !== speedSlider) {
        speedSlider.value = data.voltmeterSpeed;
    }
    speedValueLabel.innerHTML = speedSlider.value; // Değeri her zaman göster
  }

  function sendCommand(command) {
    console.log("Sending command: ", command);
    websocket.send(JSON.stringify(command));
  }
  
  function sendSetting(action, value) {
      let payload = parseInt(value);
      console.log(`Sending setting: ${action} = ${payload}`);
      sendCommand({ action: action, value: payload });
      // Slider'ın yanındaki etiketi anında güncelle
      if (action === 'setVoltmeterSpeed') {
          document.getElementById('voltmeterSpeedValue').innerHTML = value;
      }
  }

</script>
</body>
</html>
)rawliteral";