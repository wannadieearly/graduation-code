#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <math.h>

// --- Wi-Fi 凭据 ---
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
WiFiServer server(80);

// --- 电机引脚 ---
const int dirPinX = 10, stepPinX = 9;
const int dirPinY = 12, stepPinY = 11;
const int dirPinZ = 6,  stepPinZ = 5;
const int penLiftSteps = 1500;
const int microstepDelay = 50;
const float stepsPerMM = 1000.0;

// --- 盲文点位 ---
struct Dot { float x; float y; };
Dot brailleDots[200]; 
int totalDotsCount = 0, currentDotIndex = 0;
float currentX_mm = 0.0, currentY_mm = 0.0;
enum State { IDLE, DRAWING }; State currentState = IDLE;

void setup() {
  Serial.begin(9600);
  pinMode(dirPinX, OUTPUT); pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT); pinMode(stepPinY, OUTPUT);
  pinMode(dirPinZ, OUTPUT); pinMode(stepPinZ, OUTPUT);

  // 连接 Wi-Fi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(1000); }
  server.begin();
  Serial.print("IP: "); Serial.println(WiFi.localIP()); // 打印IP
}

void loop() {
  // 1. 监听网络
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    // 2. 处理指令
    if (request.indexOf("GET /print") != -1) {
      setupBraillePhrase(); // 【核心】生成正确的盲文点位
      currentState = DRAWING;
    } else if (request.indexOf("GET /stop") != -1) {
      currentState = IDLE;
    }
    
    // 3. 发送响应
    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");
    client.println();
    client.stop();
  }

  // 4. 执行打印
  if (currentState == DRAWING && currentDotIndex < totalDotsCount) {
    moveTo(brailleDots[currentDotIndex].x, brailleDots[currentDotIndex].y);
    punchDot();
    currentDotIndex++;
  } 
  
  // --- 修改点：将返回起点的逻辑放在这个独立的 if 语句中 ---
  // 这样逻辑更清晰，也避免了之前可能的括号错误
  if (currentState == DRAWING && currentDotIndex >= totalDotsCount) {
    Serial.println("Print done.");
    
    // 打印完成后，自动返回起点
    Serial.println("Returning to home position (0,0)...");
    moveTo(0, 0);
    
    currentState = IDLE;
    currentDotIndex = 0;
  }
} // 👈 这里是 loop() 函数的结束大括号，确保它存在！


// ===================================================================
// ============== 以下为关键的、未简化的盲文生成逻辑 ==================
// ===================================================================

// 【恢复】为“床前明月光”这个特定短语生成点位
void setupBraillePhrase() {
  totalDotsCount = 0;
  float current_x = 0.0;

  // 这是“床前明月光”的正确现行盲文编码序列
  addBrailleCell(0b011111, current_x); addSpace(current_x, 10); 
  addBrailleCell(0b111110, current_x); addSpace(current_x, 10); 
  addBrailleCell(0b011101, current_x); addSpace(current_x, 10);
  addBrailleCell(0b110011, current_x); addSpace(current_x, 10);
  addBrailleCell(0b001101, current_x); addSpace(current_x, 10);
  addBrailleCell(0b101011, current_x); addSpace(current_x, 10);
  addBrailleCell(0b111100, current_x); addSpace(current_x, 10);
  addBrailleCell(0b011011, current_x); addSpace(current_x, 10);
  addBrailleCell(0b111110, current_x);
}

// 添加一个盲文方 (6个点)
void addBrailleCell(int pattern, float &offsetX) {
  float localX[] = {0, 0, 0, 10, 10, 10}; 
  float localY[] = {0, 10, 20, 0, 10, 20}; 
  for (int i = 0; i < 6; i++) {
    if ((pattern >> i) & 1) { 
      brailleDots[totalDotsCount++] = {offsetX + localX[i], localY[i]};
    }
  }
  offsetX += 10; 
}

// 添加空格 (无点)
void addSpace(float &offsetX, float spaceWidth) {
    offsetX += spaceWidth; 
}


// ===================================================================
// ============== 电机控制函数 (保持不变) ============================
// ===================================================================

void moveTo(float tx, float ty) {
  long sx = (long)((tx - currentX_mm) * stepsPerMM);
  long sy = (long)((ty - currentY_mm) * stepsPerMM);
  digitalWrite(dirPinX, sx >= 0 ? HIGH : LOW);
  digitalWrite(dirPinY, sy >= 0 ? LOW : HIGH);
  sx = abs(sx); sy = abs(sy);
  for (long i = 0; i < max(sx, sy); i++) {
    if(i<sx) digitalWrite(stepPinX, HIGH); if(i<sy) digitalWrite(stepPinY, HIGH);
    delayMicroseconds(microstepDelay);
    if(i<sx) digitalWrite(stepPinX, LOW); if(i<sy) digitalWrite(stepPinY, LOW);
    delayMicroseconds(microstepDelay);
  }
  currentX_mm = tx; currentY_mm = ty;
}

void punchDot() {
  digitalWrite(dirPinZ, LOW); 
  for(int i=0; i<penLiftSteps; i++){ digitalWrite(stepPinZ, HIGH); delayMicroseconds(microstepDelay); digitalWrite(stepPinZ, LOW); delayMicroseconds(microstepDelay); }
  delay(100);
  digitalWrite(dirPinZ, HIGH); 
  for(int i=0; i<penLiftSteps; i++){ digitalWrite(stepPinZ, HIGH); delayMicroseconds(microstepDelay); digitalWrite(stepPinZ, LOW); delayMicroseconds(microstepDelay); }
  delay(100);
}