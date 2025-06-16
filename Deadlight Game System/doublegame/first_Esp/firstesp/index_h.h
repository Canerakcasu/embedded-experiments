const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Game Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    body { margin: 0; background-color: #2c3e50; color: #ecf0f1; }
    h1 { font-size: 1.8rem; color: white; }
    .content { padding: 20px; }
    .card-grid { max-width: 1200px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .card { background-color: #34495e; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.5); border-radius: 10px; padding: 20px; text-align: left; }
    .card-title { font-size: 1.2rem; font-weight: bold; margin-bottom: 15px; border-bottom: 1px solid #7f8c8d; padding-bottom: 10px;}
    .status-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; font-size: 1rem; }
    .status-label { color: #bdc3c7; }
    .status-value { font-weight: bold; color: #1abc9c; font-size: 1.1rem; }
    .button-group { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    button { background-color: #3498db; color: white; border: none; padding: 12px 15px; border-radius: 5px; font-size: 1rem; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #2980b9; }
    .btn-danger { background-color: #e74c3c; }
    .btn-danger:hover { background-color: #c0392b; }
    input[type="range"] { width: 100%; }
    .setting-item { margin-top: 15px; }
    .radio-group label { margin-right: 15px; }
  </style>
</head>
<body>
  <div class="content">
    <h1>Deadlight Game Control Panel (ESP1)</h1>
    <div class="card-grid">

      <div class="card">
        <div class="card-title">Game Status</div>
        <div class="status-item"><span class="status-label">Game State:</span><span id="gameState" class="status-value">Connecting...</span></div>
        <div class="status-item"><span class="status-label">Level Detail:</span><span id="levelDetail" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Matrix Display:</span><span id="matrix" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">Time Remaining:</span><span id="timer" class="status-value">-</span></div>
      </div>

      <div class="card">
        <div class="card-title">System Status</div>
        <div class="status-item"><span class="status-label">WiFi Status:</span><span id="wifiStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label">DFPlayer Status:</span><span id="dfPlayerStatus" class="status-value">-</span></div>
      </div>

      <div class="card">
        <div class="card-title">Game Controls</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'start_game'})">Start Game</button>
          <button class="btn-danger" onclick="sendCommand({action:'reset_game'})">Reset Game</button>
        </div>
      </div>
      
      <div class="card">
        <div class="card-title">Settings</div>
        <div class="setting-item">
          <label for="volumeSlider">Volume: <span id="volumeValue"></span></label>
          <input type="range" id="volumeSlider" min="0" max="30" onchange="sendSetting('set_volume', this.value)">
        </div>
        <div class="setting-item radio-group">
          <label>Sound Language:</label>
          <input type="radio" id="lang_tr" name="language" value="TR" onchange="sendSetting('set_sound_language', this.value)">
          <label for="lang_tr">TR</label>
          <input type="radio" id="lang_en" name="language" value="EN" onchange="sendSetting('set_sound_language', this.value)">
          <label for="lang_en">EN</label>
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
    document.getElementById('gameState').innerHTML = data.gameState;
    document.getElementById('levelDetail').innerHTML = data.levelDetail;
    document.getElementById('matrix').innerHTML = data.matrix;
    
    let seconds = data.timer;
    let minutes = Math.floor(seconds / 60);
    seconds = seconds % 60;
    document.getElementById('timer').innerHTML = `${minutes}:${seconds < 10 ? '0' : ''}${seconds}`;

    document.getElementById('wifiStatus').innerHTML = data.wifiStatus ? 'Connected' : 'Disconnected';
    document.getElementById('dfPlayerStatus').innerHTML = data.dfPlayerStatus ? 'OK' : 'Error';
    
    document.getElementById('volumeSlider').value = data.volume;
    document.getElementById('volumeValue').innerHTML = data.volume;
    
    if (data.soundLanguage === 'TR') {
      document.getElementById('lang_tr').checked = true;
    } else {
      document.getElementById('lang_en').checked = true;
    }
  }

  function sendCommand(command) {
    console.log("Sending command: ", command);
    websocket.send(JSON.stringify(command));
  }
  
  function sendSetting(action, value) {
    let payload_value = (action === 'set_volume') ? parseInt(value) : value;
    sendCommand({ action: action, value: payload_value });
  }
</script>
</body>
</html>
)rawliteral";