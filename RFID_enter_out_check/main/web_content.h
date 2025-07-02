// web_content.h
#pragma once

const char PAGE_CSS[] PROGMEM = R"rawliteral(
body { 
  font-family: 'Roboto', sans-serif; 
  background-color: #121212; 
  color: #e0e0e0;
  text-align: center; 
  margin: 0;
  padding: 30px;
}
.container { 
  background: #1e1e1e; 
  padding: 20px 40px; 
  border-radius: 12px; 
  box-shadow: 0 8px 16px rgba(0,0,0,0.4); 
  display: inline-block; 
  min-width: 700px; 
  border: 1px solid #333;
}
h1, h2, h3 {
  color: #ffffff;
  font-weight: 300;
  text-align: center;
}
h1 { font-size: 2.2em; }
h2 { border-bottom: 2px solid #03dac6; padding-bottom: 10px; font-weight: 400; margin-top: 30px;}
h3 { margin-top: 30px; color: #bb86fc; }
.data-grid { display: grid; grid-template-columns: 150px 1fr; gap: 12px; text-align: left; margin-top: 25px; }
.data-grid span { padding: 10px; border-radius: 5px; }
.data-grid span:nth-child(odd) { background-color: #333; font-weight: bold; color: #03dac6; }
.data-grid span:nth-child(even) { background-color: #2c2c2c; }
.footer-nav { font-size:0.9em; color:#bbb; margin-top:30px; }
.footer-nav a { color: #bb86fc; text-decoration: none; padding: 0 10px; }
table { width: 100%; border-collapse: collapse; margin-top: 20px; }
th, td { padding: 12px; border: 1px solid #444; text-align: left; vertical-align: middle;}
th { background-color: #03dac6; color: #121212; font-weight: 700; }
form { margin-top: 20px; padding: 20px; border: 1px solid #333; border-radius: 8px; background-color: #2c2c2c; }
input[type=text], input[type=password] { width: calc(50% - 24px); padding: 10px; margin: 5px; border-radius: 4px; border: 1px solid #555; background: #333; color: #e0e0e0; }
input[type=submit], button { padding: 10px 20px; border: none; border-radius: 4px; background: #03dac6; color: #121212; font-weight: bold; cursor: pointer; margin-top: 10px; }
button.btn-reboot { background-color: #cf6679; }
.btn-delete { color: #cf6679; text-decoration: none; font-weight: bold; }
a.btn-download { color: #81d4fa; text-decoration: none; }
.home-link { margin-bottom: 20px; display: inline-block; color: #bb86fc; font-size: 1.1em; }
.status { padding: 5px 10px; border-radius: 15px; font-size: 0.85em; text-align: center; color: white; font-weight: bold; }
.status-ok { background-color: #28a745; }
.status-bad { background-color: #cf6679; }
.status-in { background-color: #28a745; }
.status-out { background-color: #6c757d; }
.uid-reader { background-color: #252525; border: 1px dashed #555; padding: 20px; margin-top: 15px; border-radius: 8px; }
.uid-reader p { margin: 0; font-size: 1.1em; color: #aaa; }
.uid-reader #lastUID { font-family: 'Courier New', monospace; font-size: 1.5em; color: #03dac6; font-weight: bold; margin-top: 10px; min-height: 28px;}
)rawliteral";

const char PAGE_Main[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>RFID Control System</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body>
  <div class="container">
    <h1>RFID Access Control</h1>
    <h2 id="currentTime">--:--:--</h2>
    <h2>Last Event</h2>
    <div class="data-grid">
      <span>Time:</span><span id="eventTime">-</span>
      <span>Card UID:</span><span id="eventUID">-</span>
      <span>Name:</span><span id="eventName">-</span>
      <span>Action:</span><span id="eventAction">-</span>
    </div>
    <p class="footer-nav">
      <a href="/admin">Admin Panel</a> | 
      <a href="/logs">Activity Logs</a> |
      <a href="/adduserpage">Add New User</a>
    </p>
  </div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('tr-TR'); }
function fetchData() { fetch('/data').then(response => response.json()).then(data => {
      document.getElementById('eventTime').innerText = data.time;
      document.getElementById('eventUID').innerText = data.uid;
      document.getElementById('eventName').innerText = data.name;
      document.getElementById('eventAction').innerText = data.action;
    });
}
setInterval(updateTime, 1000);
setInterval(fetchData, 2000);
window.onload = () => { updateTime(); fetchData(); };
</script>
</body>
</html>
)rawliteral";

const char PAGE_AddUser[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Add New User</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head>
<body><div class='container'>
  <h1>Add New User</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>
  <div class="uid-reader">
    <p>Please scan RFID card/tag now...</p>
    <div id="lastUID">N/A</div>
  </div>
  <form action='/adduser' method='post'>
    <h3>User Information</h3>
    Card UID: <input type='text' id='uid_field' name='uid' required><br>
    Name: <input type='text' name='name' required><br><br>
    <input type='submit' value='Add User'>
  </form>
</div>
<script>
function getLastUID() {
  fetch('/getlastuid').then(response => response.text()).then(uid => {
    if (uid && uid !== "N/A") {
        document.getElementById('lastUID').innerText = uid;
        document.getElementById('uid_field').value = uid;
    }
  });
}
setInterval(getLastUID, 1000);
window.onload = getLastUID;
</script>
</body></html>
)rawliteral";

const char PAGE_Update[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Firmware Update</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head>
<body><div class='container'>
  <h1>OTA Firmware Update</h1><a href='/admin' class='home-link'>&larr; Back to Admin Panel</a>
  <form method='POST' action='/update' enctype='multipart/form-data'>
    <input type='file' name='update' accept='.bin'>
    <input type='submit' value='Update Firmware'>
  </form>
  <p>Update may take a few minutes. The device will reboot automatically after a successful update.</p>
</div></body></html>
)rawliteral";