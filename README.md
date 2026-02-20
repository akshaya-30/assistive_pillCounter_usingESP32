# assistive_pillCounter_usingESP32
Assistive_pillCounter_usingESP32

#Model Details

Backbone: MobileNet
Detection Approach: FOMO (grid-based object detection)
Input Shape: 96 × 96 × 3
Framework: TensorFlow Lite (TFLite)

Why FOMO?

No bounding box regression
Grid-based classification
Faster inference
Ideal for small objects and edge devices

 # 🛠 Hardware Requirements

ESP32-CAM (AI Thinker module)

USB-to-TTL Programmer
Stable 5V power supply
Mobile phone or edge device (Android / Linux / Raspberry Pi)

# 💻 Software Requirements
ESP32-CAM
Arduino IDE
ESP32 board package

Required libraries:
esp32-camera
WiFi.h
Mobile / Edge Inference
Python 3.x / Android Studio (depending on implementation)
TensorFlow Lite
OpenCV (optional, for preprocessing)

# ⚙️ Setup Instructions
1️⃣ ESP32-CAM Setup

Open esp32_cam.ino in Arduino IDE
Select:
Board: AI Thinker ESP32-CAM
Correct COM port

Update:
Wi-Fi credentials
Camera configuration
Upload the code

# 2️⃣ Mobile / Edge Inference Setup

Navigate to the mobile-inference folder\
Install dependencies:
pip install tensorflow opencv-python numpy

Run inference:
python inference.py
📊 Output

Detected objects are shown as:
Grid activations
Object counts
Approximate object locations
Optimized for speed and low memory usage



# 🔮 Future Improvements

Add MQTT / HTTP communication
Visualize detections on a dashboard
Quantized model optimization
On-device inference directly on ESP32 (experimental)
