const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Deadlight Game Kontrol Paneli</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; }
    .container { max-width: 600px; margin: auto; padding: 20px; }
    h2 { color: #bb86fc; }
    .card { background: #1e1e1e; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); }
    .status-display { font-size: 1.5em; font-weight: bold; color: #03dac6; margin: 10px 0; }
    .matrix-display { font-family: 'Courier New', Courier, monospace; background: #000; color: #f00; padding: 15px; border-radius: 4px; font-size: 2em; letter-spacing: 5px; margin-bottom: 20px; }
    button { background-color: #bb86fc; color: #121212; border: none; padding: 15px 30px; font-size: 1em; border-radius: 5px; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #3700b3; color: #fff; }
    .control-group { margin: 15px 0; }
    label { margin-right: 10px; }
    select, input[type=range] { padding: 8px; border-radius: 4px; background: #333; color: #e0e0e0; border: 1px solid #444; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Deadlight Kontrol Paneli</h2>
    
    <div class="card">
      <h4>Oyun Durumu</h4>
      <div id="gameState" class="status-display">Bağlanıyor...</div>
      <h4>Matris Ekran</h4>
      <div id="matrix" class="matrix-display">-</div>
    </div>

    <div class="card">
      <button id="startButton" onclick="startGame()">Oyunu Başlat</button>
      <div class="control-group">
        <label for="volume">Ses Seviyesi:</label>
        <input type="range" id="volume" min="0" max="30" onchange="setVolume(this.value)">
      </div>
      <div class="control-group">
        <label for="language">Oyun Dili:</label>
        <select id="language" onchange="setLanguage(this.value)">
          <option value="TR">Turkish</option>
          <option value="EN">English</option>
        </select>
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
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
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
    console.log(event.data);
    var data = JSON.parse(event.data);
    document.getElementById('gameState').innerText = data.gameState;
    document.getElementById('matrix').innerText = data.matrix || "-";
    document.getElementById('volume').value = data.volume;
    document.getElementById('language').value = data.language;
    
    // "Oyunu BaSlat" butonunu sadece bekleme durumunda aktif et
    if (data.gameState === "START") {
      document.getElementById('startButton').disabled = false;
    } else {
      document.getElementById('startButton').disabled = true;
    }
  }

  function sendCommand(json) {
    console.log("Sending: " + json);
    websocket.send(json);
  }

  function startGame() {
    sendCommand('{"command":"startGame"}');
  }

  function setVolume(value) {
    sendCommand(`{"command":"setVolume", "value":${value}}`);
  }

  function setLanguage(value) {
    sendCommand(`{"command":"setLanguage", "value":"${value}"}`);
  }
</script>
</body>
</html>
)rawliteral";