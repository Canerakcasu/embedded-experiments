🆕 Project Update: MAX7219 LED Matrix Display Integration
This update adds a 4x(8x8) MAX7219 LED matrix display to the project for real-time visual feedback of the ARGB LED strip status, including global brightness and the colors of the first five LEDs.

🔧 What's Updated?
✅ Hardware
Added four 8x8 MAX7219-based LED matrix modules.

✅ Libraries
Included the following libraries in the ESP32 code:

MD_Parola

MD_MAX72xx

SPI

✅ ESP32 Code
Enhanced to:

Drive the MAX7219 matrix.

Retrieve and display the HEX color codes of the first 5 ARGB LEDs.

Retrieve and display the global brightness level.

Cycle through the display information at regular intervals.

✅ Visual Feedback
The matrix display now provides live updates of:

Global brightness (e.g., B: 75%)

Colors of the first five LEDs (e.g., L0:#FF0000, L1:#00FF00, ...)

💡 How It Works
The ESP32 maintains a baseLedColors[] array for storing the base color (from Node-RED via MQTT) of each ARGB LED.

Rotary encoder controls global brightness (currentBrightness), and the ARGB strip updates instantly.

The MAX7219 matrix cycles through the following information (each shown for ~3 seconds):

Global Brightness – e.g., B: 75%

Color of LED 0 – e.g., L0:#FF0000

Color of LED 1 – e.g., L1:#00FF00

...

Color of LED 4 – e.g., L4:#0000FF

Any changes (via Node-RED or encoder):

Are instantly reflected on the matrix display.

Toggle ON/OFF function still works via encoder button.

Shows B: 0% when OFF.

📷 Visual Example (Optional)
Add a GIF or image of the matrix displaying updates if available.

📦 Dependencies
Ensure the following libraries are installed in your Arduino IDE:

MD_Parola

MD_MAX72XX

SPI

📡 Integration Notes
Communication via MQTT from Node-RED allows seamless LED control.

Matrix display is synchronized with the LED strip status for clear feedback.