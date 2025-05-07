#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <analogWrite.h>

const char* ssid = "HoaMuoiGio1";
const char* password = "minhtriet20092014";
String cam_url = "http://192.168.1.15/stream";

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

void taskServo1(void *pvParameters) {
  for (;;) {
    servo1.write(pos1);
    delay(15);
  }
}

void taskServo2(void *pvParameters) {
  for (;;) {
    servo2.write(pos2);
    delay(15);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Khởi động ESP32...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.print("Đang kết nối WiFi");
  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(1000);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Đã kết nối WiFi!");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Không thể kết nối WiFi!");
    return;
  }

  servo1.attach(14);
  servo2.attach(25);

  xTaskCreatePinnedToCore(taskServo1, "TaskServo1", 1000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskServo2, "TaskServo2", 1000, NULL, 1, NULL, 1);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);

  pinMode(MAGNET_PIN, OUTPUT);
  digitalWrite(MAGNET_PIN, LOW);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    const char index_html[] PROGMEM = R"rawliteral(
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
<body onload="updateLED(document.getElementById('ledSlider').value)">
  <img src="https://www.utc.edu.vn/assets/utc/images/logo.png" alt="Logo Trường ĐH GTVT" width="700" style="margin-bottom: 1px;"><br>
  <h2>ĐIỀU KHIỂN ROBOT TỪ XA VỚI ESP32</h2>
  <img src="http://192.168.1.15/stream" width="500"><br>

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
      <h3>ĐÈN LED</h3>
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
    let camLedOn = false;

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

    function toggleCamLED() {
      camLedOn = !camLedOn;
      fetch("/control?state=" + (camLedOn ? "on" : "off"));
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
      analogWrite(IN1, 0); analogWrite(IN2, 0);
      analogWrite(IN3, 0); analogWrite(IN4, 0);
      if (cmd == "forward") {
        analogWrite(IN1, motorSpeed); analogWrite(IN2, 0);
        analogWrite(IN3, motorSpeed); analogWrite(IN4, 0);
      } else if (cmd == "backward") {
        analogWrite(IN1, 0); analogWrite(IN2, motorSpeed);
        analogWrite(IN3, 0); analogWrite(IN4, motorSpeed);
      } else if (cmd == "left") {
        analogWrite(IN1, 0); analogWrite(IN2, motorSpeed);
        analogWrite(IN3, motorSpeed); analogWrite(IN4, 0);
      } else if (cmd == "right") {
        analogWrite(IN1, motorSpeed); analogWrite(IN2, 0);
        analogWrite(IN3, 0); analogWrite(IN4, motorSpeed);
      }
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("val")) {
      motorSpeed = constrain(request->getParam("val")->value().toInt(), 0, 255);
      request->send(200, "text/plain", "Speed set");
    } else {
      request->send(400, "text/plain", "Missing parameter");
    }
  });

  server.on("/magnet", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      digitalWrite(MAGNET_PIN, state == "on" ? HIGH : LOW);
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      digitalWrite(4, state == "on" ? HIGH : LOW);
      request->send(200, "text/plain", "OK");
    }
  });

  server.begin();
}

void loop() {}
