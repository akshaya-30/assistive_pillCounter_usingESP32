#include "esp_camera.h"
#include "img_converters.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#include "V_detection_inferencing.h"

// camera pin definitions for AI Thinker ESP32-CAM module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define EI_CLASSIFIER_INPUT_CHANNELS 3

// wifi credentials
const char* ssid = "UPC2549274";
const char* password = "Bp4enb7rtnrE";

// server
AsyncWebServer server(80);

// buffer for inference input
static uint8_t *ei_rgb_buffer = nullptr;
static size_t ei_rgb_buffer_len = 0;

// ei get_data callback for edge impulse
int ei_get_data(size_t offset, size_t length, float *out_ptr) {
    if (!ei_rgb_buffer) return -1;
    if (offset + length > ei_rgb_buffer_len) return -1;

    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)ei_rgb_buffer[offset + i] / 255.0f;
    }
    return 0;
}

// jpeg to rgb888 conversion
uint8_t* jpeg_to_rgb(camera_fb_t *fb) {
    size_t size = fb->width * fb->height * 3;
    uint8_t *rgb = (uint8_t*)ps_malloc(size);
    if (!rgb) return nullptr;

    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb)) {
        free(rgb);
        return nullptr;
    }
    return rgb;
}

// resize to fit shortest side, maintaining aspect ratio, and center crop
void resize_fit_shortest(uint8_t *src, int sw, int sh, uint8_t *dst) {
    float scale = max(
        (float)EI_CLASSIFIER_INPUT_WIDTH / sw,
        (float)EI_CLASSIFIER_INPUT_HEIGHT / sh
    );

    int nw = sw * scale;
    int nh = sh * scale;

    int x_offset = (nw - EI_CLASSIFIER_INPUT_WIDTH) / 2;
    int y_offset = (nh - EI_CLASSIFIER_INPUT_HEIGHT) / 2;

    for (int y = 0; y < EI_CLASSIFIER_INPUT_HEIGHT; y++) {
        for (int x = 0; x < EI_CLASSIFIER_INPUT_WIDTH; x++) {

            int sx = (x + x_offset) / scale;
            int sy = (y + y_offset) / scale;

            sx = constrain(sx, 0, sw - 1);
            sy = constrain(sy, 0, sh - 1);

            int si = (sy * sw + sx) * 3;
            int di = (y * EI_CLASSIFIER_INPUT_WIDTH + x) * 3;

            dst[di + 0] = src[si + 2]; // R
            dst[di + 1] = src[si + 1]; // G
            dst[di + 2] = src[si + 0]; // B
        }
    }
}


// run inference
int run_inference(uint8_t *input) {

    ei_rgb_buffer = input;

    signal_t signal;
    signal.total_length =
        EI_CLASSIFIER_INPUT_WIDTH *
        EI_CLASSIFIER_INPUT_HEIGHT;

    signal.get_data = ei_get_data;

    ei_impulse_result_t result = {0};

    Serial.println("Running inference...");
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
        Serial.printf("EI error: %d\n", err);
        return -1;
    }

    Serial.printf("BB count: %d\n", result.bounding_boxes_count);

    int count = 0;
    for (size_t i = 0; i < result.bounding_boxes_count; i++) {
        auto &bb = result.bounding_boxes[i];

        Serial.printf(
            "PILL %.2f at x=%d y=%d w=%d h=%d\n",
            bb.value, bb.x, bb.y, bb.width, bb.height
        );

        if (bb.value > 0.3f) {
            count++;
        }
    }

    return count;
}


// ui
String html_page() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Pill Counter</title>
<style>
body {
    font-family: Arial;
    background: #f2f5f9;
}
.card {
    max-width: 400px;
    margin: 40px auto;
    background: white;
    padding: 25px;
    border-radius: 12px;
    box-shadow: 0 10px 25px rgba(0,0,0,0.1);
    text-align: center;
}
button {
    background: #4CAF50;
    color: white;
    border: none;
    padding: 14px;
    width: 100%;
    font-size: 18px;
    border-radius: 8px;
}
</style>
</head>
<body>
<div class="card">
<h2> Assistive Pill Counter (ESP32-CAM)</h2>
<button onclick="capture()">Capture</button>
<p id="status"></p>
<img id="img" width="100%">
</div>

<script>
async function capture() {
    document.getElementById("status").innerText = "Processing...";
    const r = await fetch("/capture");
    const d = await r.json();
    document.getElementById("status").innerText = "Pills detected: " + d.count;
    document.getElementById("img").src = "/image?rand=" + Date.now();
}
</script>
</body>
</html>
)rawliteral";
}

// camera setup
void startCamera() {
    camera_config_t c;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = Y2_GPIO_NUM;
    c.pin_d1 = Y3_GPIO_NUM;
    c.pin_d2 = Y4_GPIO_NUM;
    c.pin_d3 = Y5_GPIO_NUM;
    c.pin_d4 = Y6_GPIO_NUM;
    c.pin_d5 = Y7_GPIO_NUM;
    c.pin_d6 = Y8_GPIO_NUM;
    c.pin_d7 = Y9_GPIO_NUM;
    c.pin_xclk = XCLK_GPIO_NUM;
    c.pin_pclk = PCLK_GPIO_NUM;
    c.pin_vsync = VSYNC_GPIO_NUM;
    c.pin_href = HREF_GPIO_NUM;
    c.pin_sccb_sda = SIOD_GPIO_NUM;
    c.pin_sccb_scl = SIOC_GPIO_NUM;
    c.pin_pwdn = PWDN_GPIO_NUM;
    c.pin_reset = RESET_GPIO_NUM;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size = FRAMESIZE_QVGA;
    c.jpeg_quality = 12;
    c.fb_count = 1;

    esp_camera_init(&c);
}

// setup
void setup() {
    Serial.begin(115200);
    disableCore0WDT();
    disableCore1WDT();

    startCamera();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/html", html_page());
    });

    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *r){
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            r->send(500, "application/json", "{\"count\":-1}");
            return;
        }

        uint8_t *rgb = jpeg_to_rgb(fb);
        esp_camera_fb_return(fb);

        static uint8_t resized[96 * 96 * 3];
        resize_fit_shortest(rgb, fb->width, fb->height, resized);
        free(rgb);

        int count = run_inference(resized);

        JSONVar resp;
        resp["count"] = count;
        r->send(200, "application/json", JSON.stringify(resp));
    });

    server.on("/image", HTTP_GET, [](AsyncWebServerRequest *r){
        camera_fb_t *fb = esp_camera_fb_get();
        r->send_P(200, "image/jpeg", fb->buf, fb->len);
        esp_camera_fb_return(fb);
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(204);
    });

    server.begin();
}

void loop() {}
