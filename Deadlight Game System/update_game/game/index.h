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
            --bg-color: #121212; --card-color: #1E1E1E; --primary-color: #03DAC6;
            --accent-color: #BB86FC; --text-color: #E0E0E0; --danger-color: #CF6679;
            --font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body { background-color: var(--bg-color); color: var(--text-color); font-family: var(--font-family); margin: 0; padding: 20px; text-align: center; }
        .container { max-width: 800px; margin: auto; text-align: left; }
        .card { background-color: var(--card-color); border-radius: 12px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }
        h2 { margin-top: 0; color: var(--primary-color); border-bottom: 1px solid #333; padding-bottom: 10px; }
        .grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .display {
            background-color: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; text-align: center;
            font-size: 1.2em; font-weight: bold; margin-bottom: 15px; color: var(--accent-color); min-height: 28px; 
            word-wrap: break-word; font-family: 'Courier New', Courier, monospace;
        }
        #timer-display { font-size: 2.5em; color: var(--danger-color); letter-spacing: 3px; }
        label { display: block; margin-bottom: 8px; font-weight: bold; color: var(--primary-color); }
        button, select { width: 100%; padding: 12px; border-radius: 5px; border: none; font-size: 1em; cursor: pointer; margin-top: 10px; transition: background-color 0.2s, filter 0.2s; box-sizing: border-box; }
        button { background-color: var(--accent-color); color: #121212; font-weight: bold; }
        button:hover:not(:disabled) { filter: brightness(1.2); }
        button:disabled { background-color: #444; color: #777; cursor: not-allowed; }
        select { background-color: #333; color: var(--text-color); }
        .control-group { margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h2 data-lang-key="timerTitle">Game Timer</h2>
            <div class="display" id="timer-display">10:00</div>
        </div>
        <div class="grid-container">
            <div class="card">
                <h2 data-lang-key="controlPanelTitle">Control Panel</h2>
                <label data-lang-key="gameStatusLabel">Game Status</label>
                <div class="display" id="game-state">Connecting...</div>
                <label data-lang-key="levelDetailLabel">Level Details</label>
                <div class="display" id="level-detail">--</div>
                <button id="startButton" onclick="sendAction('start_game')" data-lang-key="startButton">START GAME</button>
            </div>
            <div class="card">
                <h2 data-lang-key="settingsTitle">Settings</h2>
                <div class="control-group">
                    <label for="sound-language-select" data-lang-key="soundLanguageLabel">Sound Language</label>
                    <select id="sound-language-select" onchange="sendAction('set_sound_language', this.value)">
                        <option value="TR">Turkce</option><option value="EN">English</option>
                    </select>
                </div>
                <div class="control-group">
                    <label for="ui-language-select" data-lang-key="uiLanguageLabel">Interface Language</label>
                    <select id="ui-language-select" onchange="updateUIText(this.value)">
                        <option value="TR">Turkce</option><option value="EN">English</option>
                    </select>
                </div>
                <div class="control-group">
                    <label>Volume: <span id="volume-value">5</span></label>
                    <input type="range" id="volume-slider" min="0" max="30" value="5" style="width: 100%;" oninput="sendAction('set_volume', this.value)">
                </div>
            </div>
        </div>
    </div>
<script>
    const langStrings = {
        'TR': {
            timerTitle: "Oyun Sayacı", controlPanelTitle: "Kontrol Paneli", gameStatusLabel: "Oyun Durumu",
            levelDetailLabel: "Seviye Detayı", startButton: "OYUNU BAŞLAT", settingsTitle: "Ayarlar",
            soundLanguageLabel: "Oyun Sesi Dili", uiLanguageLabel: "Arayüz Dili"
        },
        'EN': {
            timerTitle: "Game Timer", controlPanelTitle: "Control Panel", gameStatusLabel: "Game Status",
            levelDetailLabel: "Level Details", startButton: "START GAME", settingsTitle: "Settings",
            soundLanguageLabel: "Game Sound Language", uiLanguageLabel: "Interface Language"
        }
    };

    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', () => { initWebSocket(); });

    function initWebSocket() {
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
        document.getElementById('game-state').innerText = "Connection Lost. Retrying...";
        setTimeout(initWebSocket, 2000); 
    }

    function formatTime(seconds) {
        if (seconds < 0) seconds = 0;
        const min = Math.floor(seconds / 60).toString().padStart(2, '0');
        const sec = (seconds % 60).toString().padStart(2, '0');
        return `${min}:${sec}`;
    }

    function updateUIText(lang) {
        const dict = langStrings[lang] || langStrings['EN'];
        document.querySelectorAll('[data-lang-key]').forEach(elem => {
            const key = elem.getAttribute('data-lang-key');
            if(dict[key]) elem.innerText = dict[key];
        });
        document.getElementById('ui-language-select').value = lang;
    }

    function onMessage(event) {
        let data = JSON.parse(event.data);
        
        if(data.gameState) document.getElementById('game-state').innerText = data.gameState;
        if(data.levelDetail) document.getElementById('level-detail').innerText = data.levelDetail;
        
        if(data.hasOwnProperty('timer')) {
            document.getElementById('timer-display').innerText = formatTime(data.timer);
        }

        if(data.hasOwnProperty('volume')) {
            document.getElementById('volume-slider').value = data.volume;
            document.getElementById('volume-value').innerText = data.volume;
        }
        
        if(data.hasOwnProperty('soundLanguage')) { 
            document.getElementById('sound-language-select').value = data.soundLanguage; 
        }
        
        document.getElementById('startButton').disabled = (data.gameState !== "Oyunu Baslat");
    }
    
    function sendAction(action, value = null) {
        if (websocket.readyState !== WebSocket.OPEN) return;
        let command = {'action': action};
        if (value !== null) {
            command.value = !isNaN(parseInt(value)) ? parseInt(value) : value;
        }
        websocket.send(JSON.stringify(command));
        
        if(action === 'set_volume') {
            document.getElementById('volume-value').innerText = value;
        }
    }
</script>
</body>
</html>
)rawliteral";