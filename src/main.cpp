#include "esp_camera.h"
#include "img_converters.h"      // for fmt2rgb888
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#include "V_detection_inferencing.h"  // Edge Impulse model

// ===== CAMERA PINS (AI Thinker ESP32-CAM) =====
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

//  WIFI 
const char* ssid     = "UPC2549274";
const char* password = "Bp4enb7rtnrE";

AsyncWebServer server(80);
camera_fb_t *latest_fb = nullptr;

// JPEG → RGB888
uint8_t *convert_jpeg_to_rgb(camera_fb_t *fb) {
    if (!fb || fb->format != PIXFORMAT_JPEG) return nullptr;

    int w = fb->width;
    int h = fb->height;
    size_t out_len = w * h * 3;

    uint8_t *rgb = (uint8_t*)ps_malloc(out_len);
    if (!rgb) return nullptr;

    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb)) {
        free(rgb);
        return nullptr;
    }
    return rgb;
}

// RESIZE TO 96×96 (nearest neighbor) for EI MODEL
void resize_nn(uint8_t *src, int src_w, int src_h, uint8_t *dst) {
    for (int y = 0; y < EI_CLASSIFIER_INPUT_HEIGHT; y++) {
        for (int x = 0; x < EI_CLASSIFIER_INPUT_WIDTH; x++) {

            int sx = x * src_w / EI_CLASSIFIER_INPUT_WIDTH;
            int sy = y * src_h / EI_CLASSIFIER_INPUT_HEIGHT;
            int src_i = (sy * src_w + sx) * 3;
            int dst_i = (y * EI_CLASSIFIER_INPUT_WIDTH + x) * 3;

            dst[dst_i + 0] = src[src_i + 0];
            dst[dst_i + 1] = src[src_i + 1];
            dst[dst_i + 2] = src[src_i + 2];
        }
    }
}

// RUN EI MODEL & COUNT PILLS 
int detect_pills(uint8_t *rgb96) {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * 3;

    signal.get_data = [&](size_t offset, size_t length, float *out_ptr) {
        for (size_t i = 0; i < length; i++) {
            out_ptr[i] = (float)rgb96[offset + i] / 255.0f;   // FIXED
        }
        return 0;
    };

    ei_impulse_result_t result = {0};
    Serial.println("Running inference...");
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, true); // ENABLE DEBUG
    if (r != EI_IMPULSE_OK) {
        Serial.printf("Classifier error: %d\n", r);
        return -1;
    }

    Serial.printf("Detected %d bounding boxes\n", result.bounding_boxes_count);

    int count = 0;
    for (size_t i = 0; i < result.bounding_boxes_count; i++) {
        auto bb = result.bounding_boxes[i];
        Serial.printf("BB: %s x=%d y=%d w=%d h=%d score=%.2f\n",
            bb.label, bb.x, bb.y, bb.width, bb.height, bb.value);

        if (bb.value > 0.6f)
            count++;
    }

    return count;
}


//  UI 
String html_page() {
    return String(
        "<html><body>"
        "<h2>ESP32-CAM Pill Counter</h2>"
        "<button onclick='capture()'>Capture</button><br><br>"
        "<img id='img' width='320'><br><br>"
        "<h3 id='count'></h3>"
        "<script>"
        "async function capture(){"
        "document.getElementById('count').innerText='Processing...';"
        "let r = await fetch('/capture');"
        "let d = await r.json();"
        "document.getElementById('count').innerText='Pills: '+d.count;"
        "document.getElementById('img').src='/image?rand='+Date.now();"
        "}"
        "</script>"
        "</body></html>"
    );
}

// CAMERA INIT
void startCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync= VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_QQVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_camera_init(&config);
}


void setup() {
    Serial.begin(115200);
    delay(1000);

    startCamera();

    WiFi.begin(ssid, password);
    Serial.print("Connecting WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("Open browser at http://");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "text/html", html_page());
    });

    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *req){
        if (latest_fb) esp_camera_fb_return(latest_fb);
        latest_fb = esp_camera_fb_get();
        int count = -1;
        Serial.println("In capture handler");

        uint8_t *full_rgb = convert_jpeg_to_rgb(latest_fb);
        if (full_rgb) {
            static uint8_t rgb96[EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * 3];
            resize_nn(full_rgb, latest_fb->width, latest_fb->height, rgb96);
            free(full_rgb);
            Serial.println("IN if loop");

            count = detect_pills(rgb96);

        }

        JSONVar resp;
        resp["count"] = count;
        req->send(200, "application/json", JSON.stringify(resp));
    });

    server.on("/image", HTTP_GET, [](AsyncWebServerRequest *req){
        if (!latest_fb) {
            req->send(500, "text/plain", "No image");
            return;
        }
        req->send_P(200, "image/jpeg", latest_fb->buf, latest_fb->len);
    });

    server.begin();
}

void loop() {}
