// Bu dosyanın adı: index.h

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Game Control Panel</title>
    <style>
        :root {
            --bg-color: #121212; --card-color: #1E1E1E; --primary-color:rgb(255, 0, 76);
            --accent-color:rgb(0, 81, 255); --text-color: #E0E0E0; --danger-color: #CF6679;
            --font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body { background-color: var(--bg-color); color: var(--text-color); font-family: var(--font-family); margin: 0; padding: 20px; }
        .container { max-width: 1200px; margin: auto; display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 20px; }
        .card { background-color: var(--card-color); border-radius: 12px; padding: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }
        h2 { margin-top: 0; color: var(--primary-color); border-bottom: 1px solid #333; padding-bottom: 10px; }
        .status-item { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #333; }
        .status-item:last-child { border-bottom: none; }
        .status-dot { width: 14px; height: 14px; border-radius: 50%; transition: all 0.3s; }
        .online { background-color:rgb(0, 38, 255); box-shadow: 0 0 8pxrgb(99, 235, 149); }
        .offline { background-color: #777; }
        .display {
            background-color: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; text-align: center;
            font-size: 1.1em; font-weight: bold; margin-bottom: 15px; color: var(--accent-color); min-height: 25px; 
            word-wrap: break-word; font-family: 'Courier New', Courier, monospace;
        }
        .button-group { display: flex; gap: 10px; }
        label { display: block; margin-bottom: 8px; font-weight: bold; color: var(--primary-color); }
        select, button, input[type=range] {
            width: 100%; padding: 12px; border-radius: 5px; border: none; font-size: 1em; cursor: pointer;
            margin-top: 10px; transition: background-color 0.2s, filter 0.2s; box-sizing: border-box;
        }
        button { background-color: var(--accent-color); color: #121212; font-weight: bold; }
        button:hover:not(:disabled) { filter: brightness(1.2); }
        button:disabled { background-color: #444; color: #777; cursor: not-allowed; }
        select { background-color: #333; color: var(--text-color); }
        .btn-danger { background-color: var(--danger-color); color: white; }
        .control-group { margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h2>Control Panel</h2>
            <label>Game Status</label>
            <div class="display" id="game-state">Connecting...</div>
            <label>Level Details</label>
            <div class="display" id="level-detail">--</div>
            <div class="button-group">
                <button id="startButton" onclick="sendAction('start_game')">START GAME</button>
                <button class="btn-danger" onclick="sendAction('reset_game')">RESET SYSTEM</button>
            </div>
        </div>
        <div class="card">
            <h2>Live Monitor</h2>
            <label>Matrix Display</label>
            <div class="display" id="matrix-display">--</div>
        </div>
        <div class="card">
            <h2>Settings</h2>
            <div class="control-group">
                <label for="language-select">Game Language</label>
                <select id="language-select" onchange="sendAction('set_language', this.value)">
                    <option value="TR">Turkce</option><option value="EN">English</option>
                </select>
            </div>
            <div class="control-group">
                <label>Volume: <span id="volume-value">5</span></label>
                <input type="range" id="volume-slider" min="0" max="30" value="5" style="width: 100%;" oninput="sendAction('set_volume', this.value)">
            </div>
        </div>
        <div class="card">
            <h2>Device Status</h2>
            <div class="status-item"><span>DFPlayer Sound Module</span><span class="status-dot offline" id="dfplayer-status"></span></div>
            <div class="status-item"><span>WiFi Connection</span><span class="status-dot offline" id="wifi-status"></span></div>
        </div>
    </div>
<script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', () => { initWebSocket(); });

    function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen  = (event) => { 
            console.log('Connection opened'); 
            document.getElementById('game-state').innerText = "Connected";
        };
        websocket.onclose = (event) => { 
            console.log('Connection closed'); 
            updateStatusDot('wifi-status', false);
            document.getElementById('game-state').innerText = "Connection Lost. Retrying...";
            setTimeout(initWebSocket, 2000); 
        };
        websocket.onmessage = (event) => {
            let data = JSON.parse(event.data);
            console.log(data);

            if(data.gameState) document.getElementById('game-state').innerText = data.gameState;
            if(data.levelDetail) document.getElementById('level-detail').innerText = data.levelDetail;
            if(data.matrix) document.getElementById('matrix-display').innerText = data.matrix || "--";
            
            updateStatusDot('dfplayer-status', data.dfPlayerStatus);
            updateStatusDot('wifi-status', data.wifiStatus);

            if(data.hasOwnProperty('volume')) {
                document.getElementById('volume-slider').value = data.volume;
                document.getElementById('volume-value').innerText = data.volume;
            }
            if(data.hasOwnProperty('language')) { 
                document.getElementById('language-select').value = data.language; 
            }
            
            document.getElementById('startButton').disabled = (data.gameState !== "Oyunu Baslat");
        };
    }
    
    function updateStatusDot(id, status) {
        const dot = document.getElementById(id);
        if(dot) dot.className = 'status-dot ' + (status ? 'online' : 'offline');
    }

    function sendAction(action, value = null) {
        if (websocket.readyState !== WebSocket.OPEN) {
            console.log("WebSocket is not connected.");
            return;
        }
        let command = {'action': action};
        if (value !== null) {
            command.value = !isNaN(parseInt(value)) ? parseInt(value) : value;
        }
        console.log("Sending: ", command);
        websocket.send(JSON.stringify(command));

        if(action === 'set_volume') {
            document.getElementById('volume-value').innerText = value;
        }
    }
</script>
</body>
</html>
)rawliteral";