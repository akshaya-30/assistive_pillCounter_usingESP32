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
ESP32-CAM (AI Thinker)

Machine Learning
FOMO Object Detection
Quantized INT8 model

# 💻 Software Requirements
Edge Impulse
Arduino Framework
PlatformIO
ESPAsyncWebServer
For mobile capture, Python,Flask server

Required libraries:
esp32-camera
WiFi.h
Mobile / Edge Inference
Python 3.x 
TensorFlow Lite
OpenCV (optional, for preprocessing)

# ⚙️ Setup Instructions
#Edge impulse setup
1. Create a project in edge impulse
2. Add the image of pills and split into 80/20 
3. Label the images
4. Add the Preporcoessing block to 96x96, RGB and shortestfit
5. Add the FOMO Objection detection model and train the model
6. Test the model using Live classification, and Model testing
7. After post processing, add threshold as 0.6.
8. GO to deployments and build the deployable as
     1. Arduino library ( quantised ) - for esp32 CAM
     2. Linux x86 - for python flask interface

# ESP32-CAM Setup 
Open esp32_cam.ino in Arduino IDE/ platformIO
Select:
Board: AI Thinker ESP32-CAM
Correct COM port

Update:
Wi-Fi credentials
Camera configuration
Upload the code

And test the model using the IP in serial monitor

#  Mobile / Edge Inference Setup
Added as another deployment layer

Navigate to the server folder\
Install dependencies:
pip install tensorflow opencv-python numpy

Run inference:
python server.py
📊 Output
![WhatsApp Image 2026-02-20 at 12 10 27](https://github.com/user-attachments/assets/8b159ec4-cc52-4fea-a0c2-62e39573f853)


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
