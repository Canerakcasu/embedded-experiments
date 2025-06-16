const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Hardware Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    body { margin: 0; background-color: #2c3e50; color: #ecf0f1; }
    h1 { font-size: 1.8rem; color: white; }
    .content { padding: 20px; }
    .card-grid { max-width: 1600px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); }
    .card { background-color: #34495e; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.5); border-radius: 10px; padding: 20px; text-align: left;}
    .card-title { font-size: 1.2rem; font-weight: bold; margin-bottom: 15px; border-bottom: 1px solid #7f8c8d; padding-bottom: 10px;}
    .status-item, .setting-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; font-size: 1rem; }
    .status-label { color: #bdc3c7; }
    .status-value { font-weight: bold; color: #1abc9c; }
    .button-group { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }
    button { background-color: #3498db; color: white; border: none; padding: 12px 15px; border-radius: 5px; font-size: 1rem; cursor: pointer; transition: background-color 0.3s; }
    button:hover { background-color: #2980b9; }
    .btn-danger { background-color: #e74c3c; } .btn-danger:hover { background-color: #c0392b; }
    .btn-success { background-color: #2ecc71; } .btn-success:hover { background-color: #27ae60; }
    .lang-buttons { margin-bottom: 20px; }
    .lang-buttons button { background-color: #95a5a6; padding: 8px 12px; font-size: 0.9rem; }
    .lang-buttons button:hover { background-color: #7f8c8d; }
    input[type="number"] { width: 80px; background-color: #2c3e50; color: white; border: 1px solid #7f8c8d; padding: 5px; border-radius: 3px; text-align: center;}
  </style>
</head>
<body>
  <div class="content">
    <h1 data-lang-key="main_title">Donanim Kontrol Paneli (ESP2)</h1>
    <div class="lang-buttons">
      <button onclick="setLanguage('tr')">Türkçe</button>
      <button onclick="setLanguage('en')">English</button>
    </div>
    <div class="card-grid">
      
      <div class="card">
        <div class="card-title" data-lang-key="status_title">Sistem Durumu</div>
        <div class="status-item"><span class="status-label" data-lang-key="gamemode_label">Oyun Modu:</span><span id="gameMode" class="status-value">Connecting...</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="button_label">Buton Durumu:</span><span id="buttonState" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="rfid_label">Son RFID UID:</span><span id="rfidStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="led_label">LED Parlakligi:</span><span id="ledBrightness" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="mic_label">Mikrofon Durumu:</span><span id="micStatus" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="volt_power_label">Voltmetre Gucu:</span><span id="voltmeterPower" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="volt_live_label">Anlik Voltaj:</span><span id="liveVoltage" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="volt_level_label">Secilen Seviye:</span><span id="selectedLevel" class="status-value">-</span></div>
      </div>
      <div class="card">
        <div class="card-title" data-lang-key="motor_status_title">Motor Durumu</div>
        <div class="status-item"><span class="status-label" data-lang-key="position_label">Pozisyon:</span><span id="motorPosition" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="speed_label">Hiz:</span><span id="motorSpeed" class="status-value">-</span></div>
        <div class="status-item"><span class="status-label" data-lang-key="running_label">Calisiyor mu?:</span><span id="motorRunning" class="status-value">-</span></div>
        <div class="card-title" style="margin-top: 20px;" data-lang-key="manual_motor_title">Manuel Motor</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'moveMotor', steps: 200})">+360&deg;</button>
          <button onclick="sendCommand({action:'moveMotor', steps: -200})">-360&deg;</button>
        </div>
      </div>
      <div class="card">
        <div class="card-title" data-lang-key="main_controls_title">Ana Kontroller</div>
        <div class="button-group">
          <button onclick="sendCommand({action:'startGame'})" data-lang-key="start_button">Tum Diziyi Baslat / Yeniden Baslat</button>
          <button class="btn-danger" onclick="sendCommand({action:'resetSystem'})" data-lang-key="reset_button">ESP32 CPU'yu Resetle</button>
        </div>
      </div>
      
      <div class="card">
        <div class="card-title" data-lang-key="settings_title">Ayarlar</div>
         <div class="setting-item"><label data-lang-key="s_volt_speed">Voltmetre Hizi (ms)</label><input type="number" id="voltmeterSpeed" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_steps">Adim / 360&deg;</label><input type="number" id="stepsPer360" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_motor_speed">Motor Max Hiz</label><input type="number" id="stepperSpeed" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_motor_accel">Motor Ivmelenme</label><input type="number" id="stepperAccel" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_search_speed">Arama Hizi</label><input type="number" id="stepperSearchSpeed" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_mic_dec">Mik. Azalma Adimi</label><input type="number" id="micDecreaseStep" class="setting-input"></div>
         <div class="setting-item"><label data-lang-key="s_mic_inc">Mik. Artma Adimi</label><input type="number" id="micIncreaseStep" class="setting-input"></div>
        <div class="button-group">
            <button class="btn-success" onclick="applySettings()" data-lang-key="apply_button">Ayarlari Uygula</button>
        </div>
      </div>

    </div>
  </div>

<script>
  const translations = {
    en: { main_title: "Hardware Control Panel (ESP2)", status_title: "System Status", gamemode_label: "Game Mode:", button_label: "Button State:", rfid_label: "Last RFID UID:", led_label: "LED Brightness:", mic_label: "Mic Status:", volt_power_label: "Voltmeter Power:", volt_live_label: "Live Voltage:", volt_level_label: "Selected Level:", motor_status_title: "Motor Status", position_label: "Position:", speed_label: "Speed:", running_label: "Is Running?:", main_controls_title: "Main Controls", start_button: "Start / Restart Full Sequence", reset_button: "Reset ESP32 CPU", manual_motor_title: "Manual Motor", settings_title: "Settings", apply_button: "Apply Settings", s_volt_speed:"Voltmeter Speed (ms)", s_steps:"Steps / 360°", s_motor_speed:"Motor Max Speed", s_motor_accel:"Motor Acceleration", s_search_speed:"Search Speed", s_mic_dec:"Mic Decrease Step", s_mic_inc:"Mic Increase Step" },
    tr: { main_title: "Donanim Kontrol Paneli (ESP2)", status_title: "Sistem Durumu", gamemode_label: "Oyun Modu:", button_label: "Buton Durumu:", rfid_label: "Son RFID UID:", led_label: "LED Parlakligi:", mic_label: "Mikrofon Durumu:", volt_power_label: "Voltmetre Gucu:", volt_live_label: "Anlik Voltaj:", volt_level_label: "Secilen Seviye:", motor_status_title: "Motor Durumu", position_label: "Pozisyon:", speed_label: "Hiz:", running_label: "Calisiyor mu?:", main_controls_title: "Ana Kontroller", start_button: "Tum Diziyi Baslat / Yeniden Baslat", reset_button: "ESP32 CPU'yu Resetle", manual_motor_title: "Manuel Motor", settings_title: "Ayarlar", apply_button: "Ayarlari Uygula", s_volt_speed:"Voltmetre Hizi (ms)", s_steps:"Adim / 360°", s_motor_speed:"Motor Max Hiz", s_motor_accel:"Motor Ivmelenme", s_search_speed:"Arama Hizi", s_mic_dec:"Mik. Azalma Adimi", s_mic_inc:"Mik. Artma Adimi" }
  };

  function setLanguage(lang) { localStorage.setItem('esp2_language', lang); document.querySelectorAll('[data-lang-key]').forEach(elem => { const key = elem.getAttribute('data-lang-key'); if (translations[lang] && translations[lang][key]) { elem.innerHTML = translations[lang][key]; } }); }
  
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onload);
  function onload(event) { initWebSocket(); const savedLang = localStorage.getItem('esp2_language') || 'tr'; setLanguage(savedLang); }
  function initWebSocket() { websocket = new WebSocket(gateway); websocket.onopen = onOpen; websocket.onclose = onClose; websocket.onmessage = onMessage; }
  function onOpen(event) { console.log('Connection opened'); websocket.send('{"action":"get_status"}');}
  function onClose(event) { console.log('Connection closed'); setTimeout(initWebSocket, 2000); }
  
  function onMessage(event) {
    var data = JSON.parse(event.data);
    document.getElementById('gameMode').innerHTML = data.gameMode;
    if(data.hardware) {
        document.getElementById('buttonState').innerHTML = data.hardware.buttonState;
        document.getElementById('rfidStatus').innerHTML = data.hardware.rfidStatus;
        document.getElementById('ledBrightness').innerHTML = data.hardware.ledBrightness;
        document.getElementById('micStatus').innerHTML = data.hardware.micStatus;
        document.getElementById('voltmeterPower').innerHTML = data.hardware.voltmeterPower;
        document.getElementById('liveVoltage').innerHTML = data.hardware.liveVoltage.toFixed(2);
        document.getElementById('selectedLevel').innerHTML = data.hardware.selectedLevel;
        document.getElementById('motorPosition').innerHTML = data.hardware.motorPosition;
        document.getElementById('motorSpeed').innerHTML = data.hardware.motorSpeed;
        document.getElementById('motorRunning').innerHTML = data.hardware.motorRunning ? "Yes" : "No";
    }
    if(data.settings){
        document.getElementById('voltmeterSpeed').value = data.settings.voltmeterSpeed;
        document.getElementById('stepsPer360').value = data.settings.stepsPer360;
        document.getElementById('stepperSpeed').value = data.settings.stepperSpeed;
        document.getElementById('stepperAccel').value = data.settings.stepperAccel;
        document.getElementById('stepperSearchSpeed').value = data.settings.stepperSearchSpeed;
        document.getElementById('micDecreaseStep').value = data.settings.micDecreaseStep;
        document.getElementById('micIncreaseStep').value = data.settings.micIncreaseStep;
    }
  }

  function sendCommand(command) { websocket.send(JSON.stringify(command)); }

  function applySettings() {
      let settings = {
          voltmeterSpeed: parseInt(document.getElementById('voltmeterSpeed').value),
          stepsPer360: parseInt(document.getElementById('stepsPer360').value),
          stepperSpeed: parseInt(document.getElementById('stepperSpeed').value),
          stepperAccel: parseInt(document.getElementById('stepperAccel').value),
          stepperSearchSpeed: parseInt(document.getElementById('stepperSearchSpeed').value),
          micDecreaseStep: parseInt(document.getElementById('micDecreaseStep').value),
          micIncreaseStep: parseInt(document.getElementById('micIncreaseStep').value)
      };
      sendCommand({action: "apply_settings", payload: settings});
  }
</script>
</body>
</html>
)rawliteral";