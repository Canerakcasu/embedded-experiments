## 🎷 Centralized Audio Hint System

This document details the implementation of a **centralized, bilingual, and non-overlapping audio hint system** for the *Deadlight* escape room project. This system allows the gamemaster to trigger real-time audio hints directly from the **Node-RED dashboard**. Hints are played from the **Raspberry Pi**, ensuring smooth performance and full decoupling from the ESP32 devices.

---

### 🔑 Key Features

* **Centralized Audio Files**
  All hint files are stored on the Raspberry Pi, allowing easy management and updates without reprogramming microcontrollers.

* **Bilingual Support (TR/EN)**
  Language selection is done via a switch on the Node-RED dashboard. Files are organized into language-specific folders.

* **Non-Overlapping Playback**
  Any playing audio is forcefully stopped before a new one starts, avoiding audio clashes.

* **Clean Node-RED Flow**
  Uses a minimal, modular architecture with a reusable Python script for playback.

---

### 💠 System Architecture

```mermaid
graph LR
A[ui_button (hint)] --> B[Function Node<br>"Generate Sound Path"]
B --> C[exec node<br>Run Python Script]
C --> D[play_sound.py<br>on Raspberry Pi]
D --> E[mpg123 Audio Output]
```

#### 1. `ui_button`

Sends the hint filename (e.g., `Puzzle1_1_easy_hint.mp3`) as `msg.payload`.

#### 2. `Function Node`

Builds the full path to the file based on selected language:

```javascript
const fileName = msg.payload;
const lang = flow.get("hint_language") || "tr";
const basePath = "/home/admin/gamemaster_sounds/";
const fullPath = basePath + lang + "/" + fileName;
msg.payload = fullPath;
return msg;
```

#### 3. `exec` Node

Runs the Python script with the file path as argument:

* **Command:** `python3 /home/admin/scripts/play_sound.py`
* **Append msg.payload:** ✅ *Checked*

---

### 🧠 Language Selection in Dashboard

* A `ui_switch` toggles between `tr` and `en`.
* A `change` node saves the selected language in the Node-RED flow context as `flow.hint_language`.

---

### 💻 Raspberry Pi Setup

#### A. Install `mpg123`

```bash
sudo apt-get update && sudo apt-get install -y mpg123
```

#### B. Directory Structure

```
/home/admin/
└── gamemaster_sounds/
    ├── tr/
    │   ├── Puzzle1_1_easy_hint.mp3
    │   └── Puzzle1_1_hard_hint.mp3
    └── en/
        ├── Puzzle1_1_easy_hint.mp3
        └── Puzzle1_1_hard_hint.mp3
```

#### C. Python Script: `/home/admin/scripts/play_sound.py`

```python
#!/usr/bin/python3
import sys
import os

# Exit if no file path is provided
if len(sys.argv) < 2:
    sys.exit(1)

sound_file_path = sys.argv[1]

# Stop any existing playback
kill_command = "killall -9 mpg123 || true"

# Play the new sound file
play_command = f'mpg123 -q "{sound_file_path}"'

# Execute commands
os.system(kill_command)
os.system("sleep 0.1")
os.system(play_command)
```

Make it executable:

```bash
chmod +x /home/admin/scripts/play_sound.py
```

---

### ✅ Summary

| Feature            | Status |
| ------------------ | ------ |
| Bilingual support  | ✅ Yes  |
| Overlap prevention | ✅ Yes  |
| ESP-independent    | ✅ Yes  |
| Dynamic UI control | ✅ Yes  |

This architecture provides a modular, reliable, and language-flexible solution for delivering immersive audio hints in a live escape room environment.
