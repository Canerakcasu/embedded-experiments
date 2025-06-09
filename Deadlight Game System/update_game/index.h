// Bu dosyanın adı: index.h

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Oyun Kontrol Paneli</title>
    <style>
        :root {
            --bg-color: #121212; --card-color: #1E1E1E; --primary-color: #03DAC6;
            --accent-color: #BB86FC; --text-color: #E0E0E0; --danger-color: #CF6679;
            --font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body { background-color: var(--bg-color); color: var(--text-color); font-family: var(--font-family); margin: 0; padding: 20px; }
        .container { max-width: 1200px; margin: auto; display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 20px; }
        .card { background-color: var(--card-color); border-radius: 12px; padding: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }
        h2 { margin-top: 0; color: var(--primary-color); border-bottom: 1px solid #333; padding-bottom: 10px; }
        .status-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 0; border-bottom: 1px solid #333; }
        .status-item:last-child { border-bottom: none; }
        .status-dot { width: 14px; height: 14px; border-radius: 50%; }
        .online { background-color: #32ff7e; box-shadow: 0 0 8px #32ff7e; }
        .offline { background-color: #777; }
        .game-status-display {
            background-color: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; text-align: center;
            font-size: 1.1em; font-weight: bold; margin-bottom: 15px; color: var(--accent-color); min-height: 25px; word-wrap: break-word; font-family: 'Courier New', Courier, monospace;
        }
        .button-group { display: flex; gap: 10px; }
        label { display: block; margin-bottom: 8px; font-weight: bold; }
        select, button {
            width: 100%; padding: 12px; border-radius: 5px; border: none; background-color: var(--primary-color);
            color: #121212; font-weight: bold; font-size: 1em; cursor: pointer; margin-top: 10px; transition: background-color 0.2s;
        }
        button:hover { filter: brightness(1.2); }
        .btn-danger { background-color: var(--danger-color); color: white; }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h2>Kontrol Paneli</h2>
            <div class="game-status-display" id="game-state">Bağlantı Kuruluyor...</div>
            <label>Oyun Aşaması</label>
            <div class="game-status-display" id="level-detail">--</div>
            <div class="button-group">
                <button onclick="sendCommand('start')">OYUNU BAŞLAT</button>
                <button class="btn-danger" onclick="sendCommand('reset')">SİSTEMİ RESETLE</button>
            </div>
        </div>
        <div class="card">
            <h2>Canlı Takip</h2>
            <label>Matris Ekranı</label>
            <div class="game-status-display" id="matrix-display">--</div>
        </div>
        <div class="card">
            <h2>Ayarlar</h2>
            <div class="control-group">
                <label for="language-select">Oyun Dili</label>
                <select id="language-select" onchange="sendSetting('setLanguage', this.value)">
                    <option value="TR">Türkçe</option><option value="EN">İngilizce</option>
                </select>
            </div>
            <div class="control-group">
                <label>Ses Seviyesi: <span id="volume-value">5</span></label>
                <input type="range" id="volume-slider" min="0" max="30" value="5" oninput="sendSetting('setVolume', this.value)">
            </div>
        </div>
        <div class="card">
            <h2>Cihaz Durumları</h2>
            <div class="status-item"><span>DFPlayer Ses Modülü</span><span class="status-dot offline" id="dfplayer-status"></span></div>
            <div class="status-item"><span>WiFi Bağlantısı</span><span class="status-dot offline" id="wifi-status"></span></div>
        </div>
    </div>
<script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', () => { initWebSocket(); });
    function initWebSocket() {
        websocket = new WebSocket(gateway);
        websocket.onopen  = (event) => { console.log('Bağlantı açıldı'); };
        websocket.onclose = (event) => { setTimeout(initWebSocket, 2000); };
        websocket.onmessage = (event) => {
            let data = JSON.parse(event.data);
            if(data.gameState) document.getElementById('game-state').innerText = data.gameState;
            if(data.levelDetail) document.getElementById('level-detail').innerText = data.levelDetail;
            if(data.matrix) document.getElementById('matrix-display').innerText = data.matrix;
            updateStatusDot('dfplayer-status', data.dfplayer);
            updateStatusDot('wifi-status', data.wifi);
            if(data.hasOwnProperty('volume')) {
                document.getElementById('volume-slider').value = data.volume;
                document.getElementById('volume-value').innerText = data.volume;
            }
            if(data.hasOwnProperty('language')) { document.getElementById('language-select').value = data.language; }
        };
    }
    function updateStatusDot(id, status) {
        const dot = document.getElementById(id);
        dot.className = 'status-dot ' + (status ? 'online' : 'offline');
    }
    function sendCommand(command) { websocket.send(JSON.stringify({'action': command})); }
    function sendSetting(setting, value) {
        let val = isNaN(parseInt(value)) ? value : parseInt(value);
        websocket.send(JSON.stringify({'action': setting, 'value': val}));
        if(setting === 'setVolume') document.getElementById('volume-value').innerText = value;
    }
</script>
</body>
</html>
)rawliteral";