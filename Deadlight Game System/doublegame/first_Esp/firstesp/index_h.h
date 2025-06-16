const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Game Control (ESP1)</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    body { margin: 0; background-color: #2c3e50; color: #ecf0f1; }
    h1 { font-size: 1.8rem; color: white; }
    .content { padding: 20px; }
    .card-grid { max-width: 1200px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); }
    .card { background-color: #34495e; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.5); border-radius: 10px; padding: 20px; text-align: left; }
    .card-title { font-size: 1.2rem; font-weight: bold; margin-bottom: 15px; border-bottom: 1px solid #7f8c8d; padding-bottom: 10px;}
    .status-item, .setting-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; font-size: 1rem; }
    .status-label { color: #bdc3c7; }
    .status-value { font-weight: bold; color: #1abc9c; font-size: 1.1rem; }
    .button-group { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    button { background-color: #3498db; color: white; border: none; padding: 12px 15px; border-radius: 5px; font-size: 1rem; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #2980b9; }
    .btn-success { background-color: #2ecc71; } .btn-success:hover { background-color: #27ae60; }
    .btn-danger { background-color: #e74c3c; } .btn-danger:hover { background-color: #c0392b; }
    .lang-buttons { margin-bottom: 20px; }
    .lang-buttons button { background-color: #95a5a6; padding: 8px 12px; font-size: 0.9rem; }
    .lang-buttons button:hover { background-color: #7f8c8d; }
    input[type="number"] { width: 80px; background-color: #2c3e50; color: white; border: 1px solid #7f8c8d; padding: 5px; border-radius: 3px; text-align: center;}
    .radio-group label { margin-right: 15px; cursor: pointer; }
  </style>
</head>
<body>
  <div class="content">
    <h1 data-lang-key="main_title">Oyun Kontrol Paneli (ESP1)</h1>
    <div class="lang-buttons">
      <button onclick="setLanguage('tr')">Türkçe</button>
      <button onclick="setLanguage('en')">English</button>
    </div>
    <div class="card-grid">
      <div class="card">
        <div class="card-title" data-lang-key="game_status_title">Oyun Durumu</div>
        <div class="status-item"><span class="status-label" data-lang-key="state_label">Oyun Aşaması:</span><span id="gameState" class="status-value">Connecting...</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="level_label">Seviye Detayı:</span><span id="levelDetail" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="matrix_label">Matrix Ekranı:</span><span id="matrix" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="time_label">Kalan Süre:</span><span id="timer" class="status-value">-</span></div>
      </div>
      <div class="card">
        <div class="card-title" data-lang-key="system_status_title">Sistem Durumu</div>
        <div class="status-item"><span class="status-label" data-lang-key="wifi_label">WiFi Durumu:</span><span id="wifiStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="sound_module_label">Ses Modülü:</span><span id="dfPlayerStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="last_sound_label">Son Çalınan Ses:</span><span id="lastSoundTrack" class="status-value">-</span></div>
      </div>
      <div class="card">
        <div class="card-title" data-lang-key="controls_title">Oyun Kontrolleri</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'start_game'})" data-lang-key="start_button">Oyunu Başlat</button>
          <button class="btn-danger" onclick="sendCommand({action:'reset_game'})" data-lang-key="reset_button">Oyunu Sıfırla</button>
        </div>
      </div>
      <div class="card">
        <div class="card-title" data-lang-key="settings_title">Ayarlar</div>
        <div class="setting-item">
          <label data-lang-key="volume_label">Ses Seviyesi</label>
          <input type="number" id="volume" min="0" max="30" class="setting-input">
        </div>
        <div class="setting-item">
          <label data-lang-key="anim_interval_label">Animasyon Hızı (ms)</label>
          <input type="number" id="base_animation_interval" min="50" max="500" class="setting-input">
        </div>
        <div class="setting-item">
          <label data-lang-key="react_window_label">Reaksiyon Süresi (ms)</label>
          <input type="number" id="base_reaction_window" min="200" max="1000" class="setting-input">
        </div>
        <div class="setting-item">
          <label data-lang-key="strikes_label">Can Hakkı</label>
          <input type="number" id="max_strikes" min="1" max="10" class="setting-input">
        </div>
        <div class="setting-item radio-group">
          <label data-lang-key="sound_lang_label">Ses Dili:</label>
          <div>
            <input type="radio" id="lang_tr" name="language" value="TR" onchange="sendSetting('set_sound_language', this.value)">
            <label for="lang_tr">TR</label>
            <input type="radio" id="lang_en" name="language" value="EN" onchange="sendSetting('set_sound_language', this.value)">
            <label for="lang_en">EN</label>
          </div>
        </div>
        <div class="button-group">
            <button class="btn-success" onclick="applySettings()" data-lang-key="apply_button">Ayarları Uygula</button>
        </div>
      </div>
    </div>
  </div>
<script>
  const translations = {
    en: { main_title: "Game Control Panel (ESP1)", game_status_title: "Game Status", state_label: "Game State:", level_label: "Level Detail:", matrix_label: "Matrix Display:", time_label: "Time Remaining:", system_status_title: "System Status", wifi_label: "WiFi Status:", sound_module_label: "Sound Module:", last_sound_label: "Last Sound Played:", controls_title: "Game Controls", start_button: "Start Game", reset_button: "Reset Game", settings_title: "Settings", volume_label: "Volume", anim_interval_label: "Animation Speed (ms)", react_window_label: "Reaction Window (ms)", strikes_label: "Number of Lives", sound_lang_label: "Sound Language:", apply_button: "Apply Settings" },
    tr: { main_title: "Oyun Kontrol Paneli (ESP1)", game_status_title: "Oyun Durumu", state_label: "Oyun Aşaması:", level_label: "Seviye Detayı:", matrix_label: "Matrix Ekranı:", time_label: "Kalan Süre:", system_status_title: "Sistem Durumu", wifi_label: "WiFi Durumu:", sound_module_label: "Ses Modülü:", last_sound_label: "Son Çalınan Ses:", controls_title: "Oyun Kontrolleri", start_button: "Oyunu Başlat", reset_button: "Oyunu Sıfırla", settings_title: "Ayarlar", volume_label: "Ses Seviyesi", anim_interval_label: "Animasyon Hızı (ms)", react_window_label: "Reaksiyon Süresi (ms)", strikes_label: "Can Hakkı", sound_lang_label: "Ses Dili:", apply_button: "Ayarları Uygula" }
  };
  function setLanguage(lang) { localStorage.setItem('esp1_language', lang); document.querySelectorAll('[data-lang-key]').forEach(elem => { const key = elem.getAttribute('data-lang-key'); if (translations[lang] && translations[lang][key]) { elem.innerHTML = translations[lang][key]; } }); }
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onload);
  function onload(event) { initWebSocket(); const savedLang = localStorage.getItem('esp1_language') || 'tr'; setLanguage(savedLang); }
  function initWebSocket() { websocket = new WebSocket(gateway); websocket.onopen  = onOpen; websocket.onclose = onClose; websocket.onmessage = onMessage; }
  function onOpen(event) { console.log('Connection opened'); }
  function onClose(event) { console.log('Connection closed'); setTimeout(initWebSocket, 2000); }
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
    if(data.hardware) { document.getElementById('lastSoundTrack').innerHTML = data.hardware.lastSoundTrack; }
    if(data.settings) {
        document.getElementById('volume').value = data.settings.volume;
        document.getElementById('base_animation_interval').value = data.settings.base_animation_interval;
        document.getElementById('base_reaction_window').value = data.settings.base_reaction_window;
        document.getElementById('max_strikes').value = data.settings.max_strikes;
    }
    if (data.soundLanguage === 'TR') { document.getElementById('lang_tr').checked = true; } 
    else { document.getElementById('lang_en').checked = true; }
  }
  function sendCommand(command) { websocket.send(JSON.stringify(command)); }
  function sendSetting(action, value) { sendCommand({ action: action, value: value }); }
  function applySettings() {
      let settings = {
          volume: parseInt(document.getElementById('volume').value),
          base_animation_interval: parseInt(document.getElementById('base_animation_interval').value),
          base_reaction_window: parseInt(document.getElementById('base_reaction_window').value),
          max_strikes: parseInt(document.getElementById('max_strikes').value)
      };
      sendCommand({action: "apply_settings", payload: settings});
  }
</script>
</body>
</html>
)rawliteral";