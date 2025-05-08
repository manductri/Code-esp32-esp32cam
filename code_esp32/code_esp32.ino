#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <analogWrite.h> // Đảm bảo thư viện này tương thích và hoạt động đúng với ESP32 (thường dùng ledcWrite)
#include <HTTPClient.h>

const char* ssid = "HoaMuoiGio1";
const char* password = "minhtriet20092014";
String cam_stream_url = "http://192.168.1.44/stream"; // Giữ nguyên cho stream video
String cam_control_base_url = "http://192.168.1.44"; // IP của ESP32-CAM

Servo servo1, servo2;
int pos1 = 90;
int pos2 = 90;
int motorSpeed = 200;

AsyncWebServer server(80);

#define IN1 26
#define IN2 27
#define IN3 32
#define IN4 33
#define MAGNET_PIN 5

// Cấu trúc để truyền tham số cho task điều khiển CAM LED
struct CamLedControlParams {
  String state;
  char url[100]; // Đủ lớn để chứa URL
};

// Task function để gửi lệnh điều khiển LED đến ESP32-CAM
void taskControlCamLed(void *pvParameters) {
  CamLedControlParams* params = (CamLedControlParams*)pvParameters;
  String url_str = String(params->url) + "/control?state=" + params->state;
  
  // Giải phóng bộ nhớ của params ngay sau khi sao chép dữ liệu cần thiết
  // Hoặc nếu bạn không cần params->url và params->state sau khi gọi http.begin
  // thì có thể không cần String url_str = ... mà dùng trực tiếp
  String stateToCam = params->state;
  String camTargetUrl = String(params->url);
  delete params; // Giải phóng params

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    Serial.print("taskControlCamLed: Sending to ESP32-CAM: ");
    Serial.println(url_str);

    http.begin(url_str); // Khởi tạo HTTP client với URL đầy đủ
    http.setConnectTimeout(2000); // Timeout kết nối: 2 giây
    http.setTimeout(3000);      // Timeout cho toàn bộ yêu cầu: 3 giây
    
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("[HTTP-CAM] GET response code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("[HTTP-CAM] Response: ");
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP-CAM] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("taskControlCamLed: WiFi not connected. Cannot send command.");
  }
  vTaskDelete(NULL); // Quan trọng: Xóa task sau khi hoàn thành
}


void taskServo1(void *pvParameters) {
  for (;;) {
    servo1.write(pos1);
    delay(15); // Cân nhắc dùng vTaskDelay để task yield tốt hơn
  }
}

void taskServo2(void *pvParameters) {
  for (;;) {
    servo2.write(pos2);
    delay(15); // Cân nhắc dùng vTaskDelay để task yield tốt hơn
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Khởi động ESP32 (chính)...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  // WiFi.persistent(true); // persistent(true) là mặc định, không cần thiết lắm

  Serial.print("Đang kết nối WiFi");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 30000)) { // Timeout 30 giây
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Đã kết nối WiFi!");
    Serial.print("IP ESP32 (chính): ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Không thể kết nối WiFi! Vui lòng kiểm tra lại.");
    // Cân nhắc việc dừng ở đây hoặc thử lại, thay vì return và chạy server không có WiFi
    // return; 
  }

  servo1.attach(14);
  servo2.attach(25);

  // Stack size cho servo tasks có thể cần tăng nếu có vấn đề, nhưng 1000 words (4KB) thường là đủ.
  xTaskCreatePinnedToCore(taskServo1, "TaskServo1", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskServo2, "TaskServo2", 2048, NULL, 1, NULL, 1);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  // Sử dụng ledcSetup và ledcAttach cho analogWrite trên ESP32 sẽ tốt hơn
  // nhưng thư viện analogWrite.h có thể đã làm điều này cho bạn.
  // Đảm bảo giá trị 0 là tắt hẳn.
  analogWrite(IN1, 0); 
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);

  pinMode(MAGNET_PIN, OUTPUT);
  digitalWrite(MAGNET_PIN, LOW);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Sử dụng cam_stream_url trong HTML
    String index_html = R"rawliteral(
    <!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Web Control</title>
  <style>
    body { font-family: Arial; margin: 20px; display: flex; flex-direction: column; align-items: center; }
    h2 { color: #0b5394; margin: 2px 0; }
    h3, h4 { margin: 5px 0; line-height: 1.2; }
    input[type=range] {
      width: 250px; -webkit-appearance: none; height: 8px; border-radius: 4px;
      background: #ccc; outline: none; margin: 5px 0;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none;
      width: 20px; height: 20px; background: #007BFF; border-radius: 50%; cursor: pointer; border: none;
    }
    input[type=range]::-moz-range-thumb {
      width: 20px; height: 20px; background: #007BFF; border-radius: 50%; cursor: pointer; border: none;
    }
    button {
      padding: 10px 20px; margin: 3px; font-size: 16px;
      border-radius: 8px; border: none; color: white; background-color: #007BFF;
    }
    button:hover { cursor: pointer; }
    .main-container {
      display: flex; justify-content: center; margin-top: 15px;
      gap: 30px; flex-wrap: wrap;
    }
    .column { display: flex; flex-direction: column; align-items: center; }
    .motor-grid {
      display: grid; grid-template-columns: 80px 80px 80px;
      grid-template-rows: auto auto auto; gap: 10px; justify-content: center; margin-top: 5px;
    }
    .motor-grid button { width: 80px; }
    .servo-container {
      display: flex; flex-direction: row; align-items: center; gap: 10px;
    }
  </style>
</head>
)rawliteral";
    index_html += R"rawliteral(
<body onload="pageLoaded()"> <img src="https://www.utc.edu.vn/assets/utc/images/logo.png" alt="Logo Trường ĐH GTVT" width="700" style="margin-bottom: 1px;"><br>
  <h2>ĐIỀU KHIỂN ROBOT TỪ XA VỚI ESP32</h2>

    <img src="http://192.168.1.44/stream" width="500" height="400">

  <div class="main-container">
    <div class="column">
      <h3>ĐIỀU KHIỂN SERVO</h3>
      <div class="servo-container">
        <h4>Pan:</h4>
        <input type="range" min="0" max="180" value="90" id="slider1" oninput="updateServo(1, this.value)">
      </div>
      <div class="servo-container">
        <h4>Tilt:</h4>
        <input type="range" min="0" max="180" value="90" id="slider2" oninput="updateServo(2, this.value)">
      </div>
      <h3>ĐIỀU KHIỂN NAM CHÂM</h3>
      <button id="magnetBtn" onclick="toggleMagnet()" style="background-color:red;">TẮT</button>
      <h3>ĐÈN LED (ESP32-CAM)</h3>
      <button id="camLedBtn" onclick="toggleCamLED()" style="background-color:red;">TẮT</button>
    </div>

    <div class="column">
      <h3>ĐIỀU KHIỂN ĐỘNG CƠ</h3>
      <h4>Tốc độ: <span id="spdVal">200</span></h4>
        <input type="range" min="0" max="255" value="200" id="speedSlider" oninput="setSpeed(this.value)">
        <div class="motor-grid">
          <div></div>
        <button onmousedown="sendCmd('forward')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('forward')" ontouchend="sendCmd('stop')">⬆️</button>
        <div></div>
        <button onmousedown="sendCmd('left')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('left')" ontouchend="sendCmd('stop')">⬅️</button>
        <button onmousedown="sendCmd('stop')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('stop')" ontouchend="sendCmd('stop')">⏹️</button>
        <button onmousedown="sendCmd('right')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('right')" ontouchend="sendCmd('stop')">➡️</button>
        <div></div>
        <button onmousedown="sendCmd('backward')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('backward')" ontouchend="sendCmd('stop')">⬇️</button>
        <div></div>
      </div>
    </div>
  </div>

  <script>
    let magnetOn = false;
    let camLedOn = false; // Trạng thái LED của ESP32-CAM

    function pageLoaded() {
      // Hàm này được gọi khi body onload. 
      // Bạn có thể bỏ trống nếu không dùng đến 'updateLED' và 'ledSlider' nữa.
      // Hoặc gọi hàm cập nhật tốc độ ban đầu nếu cần
      // setSpeed(document.getElementById('speedSlider').value); 
      console.log("Page loaded and scripts are active.");
    }

    function updateServo(id, val) {
      fetch("/setServo?id=" + id + "&pos=" + val);
    }

    function sendCmd(cmd) {
      fetch("/motor?cmd=" + cmd);
    }

    function toggleMagnet() {
      magnetOn = !magnetOn;
      fetch("/magnet?state=" + (magnetOn ? "on" : "off"));
      const btn = document.getElementById("magnetBtn");
      btn.innerText = magnetOn ? "BẬT" : "TẮT";
      btn.style.backgroundColor = magnetOn ? "green" : "red";
    }

    // Hàm này giờ sẽ gửi lệnh đến ESP32 (chính), 
    // ESP32 (chính) sẽ tạo task để gửi tiếp đến ESP32-CAM
    function toggleCamLED() {
      camLedOn = !camLedOn;
      fetch("/control?state=" + (camLedOn ? "on" : "off"))
        .then(response => response.text())
        .then(data => console.log("CAM LED control:", data))
        .catch(error => console.error('Error toggling CAM LED:', error));
        
      const btn = document.getElementById("camLedBtn");
      btn.innerText = camLedOn ? "BẬT" : "TẮT";
      btn.style.backgroundColor = camLedOn ? "green" : "red";
    }

    function setSpeed(val) {
      document.getElementById("spdVal").innerText = val;
      fetch("/setSpeed?val=" + val);
    }
  </script>
</body>
</html>
    )rawliteral";
    request->send(200, "text/html", index_html);
  });

  server.on("/setServo", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("id") && request->hasParam("pos")) {
      int id = request->getParam("id")->value().toInt();
      int pos = constrain(request->getParam("pos")->value().toInt(), 0, 180);
      if (id == 1) pos1 = pos; else if (id == 2) pos2 = pos;
      request->send(200, "text/plain", "OK");
    } else request->send(400, "text/plain", "Missing parameters");
  });

  server.on("/motor", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("cmd")) {
      String cmd = request->getParam("cmd")->value();
      // Dừng tất cả các motor trước khi thực hiện lệnh mới là một cách hay
      analogWrite(IN1, 0); analogWrite(IN2, 0);
      analogWrite(IN3, 0); analogWrite(IN4, 0);
      
      if (cmd == "forward") {
        analogWrite(IN1, motorSpeed); analogWrite(IN2, 0);
        analogWrite(IN3, motorSpeed); analogWrite(IN4, 0);
      } else if (cmd == "backward") {
        analogWrite(IN1, 0); analogWrite(IN2, motorSpeed);
        analogWrite(IN3, 0); analogWrite(IN4, motorSpeed);
      } else if (cmd == "left") {
        analogWrite(IN1, 0); analogWrite(IN2, motorSpeed); // Bánh trái lùi hoặc dừng, bánh phải tiến
        analogWrite(IN3, motorSpeed); analogWrite(IN4, 0);
      } else if (cmd == "right") {
        analogWrite(IN1, motorSpeed); analogWrite(IN2, 0); // Bánh trái tiến, bánh phải lùi hoặc dừng
        analogWrite(IN3, 0); analogWrite(IN4, motorSpeed);
      }
      // Lệnh "stop" đã được xử lý bởi việc reset motor ở trên, 
      // hoặc có thể để trống vì onmouseup sẽ gửi "stop"
      request->send(200, "text/plain", "Motor command: " + cmd);
    } else {
      request->send(400, "text/plain", "Missing motor command");
    }
  });

  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("val")) {
      motorSpeed = constrain(request->getParam("val")->value().toInt(), 0, 255);
      request->send(200, "text/plain", "Speed set to " + String(motorSpeed));
    } else {
      request->send(400, "text/plain", "Missing speed value");
    }
  });

  server.on("/magnet", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      digitalWrite(MAGNET_PIN, state == "on" ? HIGH : LOW);
      request->send(200, "text/plain", "Magnet " + state);
    } else {
       request->send(400, "text/plain", "Missing magnet state");
    }
  });

  // Xóa handler /control cũ bị xung đột và handler điều khiển LED cục bộ.
  // Handler mới cho /control để tạo task gửi lệnh đến ESP32-CAM:
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      String stateValue = request->getParam("state")->value();
      
      // Tạo và cấp phát động cho params
      CamLedControlParams* params = new CamLedControlParams();
      if (params == nullptr) {
          Serial.println("Failed to allocate memory for CamLedControlParams");
          request->send(500, "text/plain", "Server error: memory allocation failed");
          return;
      }
      params->state = stateValue;
      strncpy(params->url, cam_control_base_url.c_str(), sizeof(params->url) - 1);
      params->url[sizeof(params->url) - 1] = '\0'; // Đảm bảo null-terminated

      // Tạo task để xử lý việc gửi HTTP request, không làm block server
      // Stack 4096 bytes (1024 words * 4) là an toàn cho HTTPClient
      // Ưu tiên 1 (thấp hơn các task server mặc định)
      // Chạy trên core bất kỳ (tskNO_AFFINITY) hoặc core APP_CPU_NUM (1)
      BaseType_t taskCreated = xTaskCreatePinnedToCore(
        taskControlCamLed,         // Hàm task
        "CamLedControl",           // Tên task
        4096,                      // Stack size (4KB)
        params,                    // Tham số truyền vào (params chứa URL và trạng thái)
        1,                         // Mức ưu tiên
        NULL,                      // Không cần handle
        1                          // Core 1
      );

      request->send(200, "text/plain", "Sent CAM LED command: " + stateValue);
    } else {
      request->send(400, "text/plain", "Missing state parameter");
    }
  });

  server.begin();
  Serial.println("HTTP server đã khởi động trên ESP32 (chính)");
}

void loop() {
  // Không cần code gì ở đây vì ESPAsyncWebServer hoạt động dựa trên task riêng.
  // delay(10); // Có thể thêm delay nhỏ nếu gặp vấn đề WDT, nhưng thường không cần.
}