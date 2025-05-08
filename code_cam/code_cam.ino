#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <analogWrite.h>

// WiFi credentials
const char* ssid = "HoaMuoiGio1";
const char* password = "minhtriet20092014";
//const char* ssid = "Thanh Nguyen_2.4G";
//const char* password = "Canh@2013#";
// GPIO cấu hình cho ESP32-CAM (AI-Thinker)
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

#define LED_PIN       4

WebServer server(80);
// 📡 Stream handler
void handle_jpg_stream() {
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (true) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);
    if (!client.connected()) break;
    delay(30);  // Adjust frame rate
  }
}
// LED control
void handleLedControl() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      analogWrite(LED_PIN, 128); // Logic có thể ngược lại tùy LED
      server.send(200, "text/plain", "CAM LED ON");
    } else if (state == "off") {
      analogWrite(LED_PIN, 0);
      server.send(200, "text/plain", "CAM LED OFF");
    } else {
      server.send(400, "text/plain", "Invalid state");
    }
  } else {
    server.send(400, "text/plain", "Missing state parameter");
  }
}

// 🌐 Start server
void startCameraServer() {
  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.begin();
  Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(115200);
  
  // Bật đèn flash GPIO 4 làm output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // tắt mặc định

  
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");

  server.on("/control", handleLedControl);  // Nhận lệnh từ ESP32 chính

  // 📸 Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Set quality and resolution
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // 🚀 Init camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // ✅ Start streaming server
  startCameraServer();
  server.begin();
  Serial.println("Camera ready! Access /stream for live view.");
}

void loop() {
  server.handleClient();
  delay(1);
}