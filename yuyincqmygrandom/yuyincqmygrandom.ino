#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <math.h>

// --- Wi-Fi 凭据 ---
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
WiFiServer server(80);

// --- 汉字 UTF-8 定义 ---
const byte CHAR_CHUANG[] = {0xE5, 0xBA, 0x8A}; 
const byte CHAR_QIAN[]   = {0xE5, 0x89, 0x8D}; 
const byte CHAR_MING[]   = {0xE6, 0x98, 0x8E}; 
const byte CHAR_YUE[]    = {0xE6, 0x9C, 0x88}; 
const byte CHAR_GUANG[]  = {0xE5, 0x85, 0x89}; 

// --- 引脚定义 ---
const int dirPinX = 10, stepPinX = 9;
const int dirPinY = 12, stepPinY = 11;
const int dirPinZ = 6,  stepPinZ = 5;

// --- 物理参数 ---
const float stepsPerMM = 1000.0;
const int microstepDelay = 50;
const int penLiftSteps = 1500;
const float dotPitch = 10.0;     
const float cellSpacing = 18.0;  
const float wordSpacing = 25.0;  

// --- 状态与队列 ---
struct Dot { float x; float y; };
Dot brailleDots[400]; 
int totalDotsCount = 0;
int currentDotIndex = 0;
float currentX_mm = 0.0, currentY_mm = 0.0;
float currentX_Offset = 0.0;

enum State { IDLE, DRAWING };
State currentState = IDLE;

// ===================================================================

void setup() {
  Serial.begin(9600);
  pinMode(dirPinX, OUTPUT); pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT); pinMode(stepPinY, OUTPUT);
  pinMode(dirPinZ, OUTPUT); pinMode(stepPinZ, OUTPUT);

  Serial.print("Connecting to: "); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  server.begin();
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());
}

// 辅助函数：将 URL 编码字符（如 %E6）转回原始字节
String urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] == '%' && i + 2 < str.length()) {
      temp[2] = str[i + 1];
      temp[3] = str[i + 2];
      decoded += (char)strtol(temp, NULL, 16);
      i += 2;
    } else if (str[i] == '+') {
      decoded += ' ';
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.readString(); // 清空缓冲区

    if (request.indexOf("GET /print") != -1) {
      int start = request.indexOf("text=") + 5;
      int end = request.indexOf(" ", start);
      if (end == -1) end = request.length();
      
      String rawText = request.substring(start, end);
      String decodedText = urlDecode(rawText);
      
      Serial.println("Received Text: " + decodedText);
      parseAndQueueText(decodedText); 
      
      if (totalDotsCount > 0) {
        currentState = DRAWING;
        Serial.print("Dots in queue: "); Serial.println(totalDotsCount);
      }
    } else if (request.indexOf("GET /stop") != -1) {
      resetSystem();
    }
    
    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK");
    client.stop();
  }

  // 打印任务执行
  if (currentState == DRAWING && currentDotIndex < totalDotsCount) {
    moveTo(brailleDots[currentDotIndex].x, brailleDots[currentDotIndex].y);
    punchDot();
    currentDotIndex++;
    
    if (currentDotIndex >= totalDotsCount) {
      Serial.println("Print finished. Resetting...");
      delay(1000);
      resetSystem();
    }
  }
}

// ===================================================================

void parseAndQueueText(String text) {
  totalDotsCount = 0;
  currentDotIndex = 0;
  currentX_Offset = 0;

  for (int i = 0; i < text.length(); ) {
    if ((byte)text[i] >= 0x80) {
      byte b[3] = {(byte)text[i], (byte)text[i+1], (byte)text[i+2]};
      
      if      (memcmp(b, CHAR_CHUANG, 3) == 0) addCharToQueue(1);
      else if (memcmp(b, CHAR_QIAN, 3)   == 0) addCharToQueue(2);
      else if (memcmp(b, CHAR_MING, 3)   == 0) addCharToQueue(3);
      else if (memcmp(b, CHAR_YUE, 3)    == 0) addCharToQueue(4);
      else if (memcmp(b, CHAR_GUANG, 3)  == 0) addCharToQueue(5);
      
      i += 3;
    } else { i++; }
  }
}

void addCharToQueue(int type) {
  switch(type) {
    case 1: addCell(0b011111); currentX_Offset += cellSpacing; addCell(0b111110); break;
    case 2: addCell(0b011101); currentX_Offset += cellSpacing; addCell(0b110011); break;
    case 3: addCell(0b001101); currentX_Offset += cellSpacing; addCell(0b101011); break;
    case 4: addCell(0b111100); break;
    case 5: addCell(0b011011); currentX_Offset += cellSpacing; addCell(0b111110); break;
  }
  currentX_Offset += wordSpacing;
}

void addCell(int pattern) {
  float localX[] = {0, 0, 0, dotPitch, dotPitch, dotPitch};
  float localY[] = {0, dotPitch, dotPitch * 2.0, 0, dotPitch, dotPitch * 2.0};
  for (int i = 0; i < 6; i++) {
    if ((pattern >> i) & 1) {
      if (totalDotsCount < 400) brailleDots[totalDotsCount++] = {currentX_Offset + localX[i], localY[i]};
    }
  }
}

void moveTo(float tx, float ty) {
  long sx = (long)((tx - currentX_mm) * stepsPerMM);
  long sy = (long)((ty - currentY_mm) * stepsPerMM);
  digitalWrite(dirPinX, sx >= 0 ? HIGH : LOW);
  digitalWrite(dirPinY, sy >= 0 ? LOW : HIGH);
  sx = abs(sx); sy = abs(sy);
  long steps = max(sx, sy);
  for (long i = 0; i < steps; i++) {
    if(i < sx) digitalWrite(stepPinX, HIGH);
    if(i < sy) digitalWrite(stepPinY, HIGH);
    delayMicroseconds(microstepDelay);
    if(i < sx) digitalWrite(stepPinX, LOW);
    if(i < sy) digitalWrite(stepPinY, LOW);
    delayMicroseconds(microstepDelay);
  }
  currentX_mm = tx; currentY_mm = ty;
}

void punchDot() {
  digitalWrite(dirPinZ, LOW); 
  for(int i=0; i<penLiftSteps; i++){ digitalWrite(stepPinZ, HIGH); delayMicroseconds(microstepDelay); digitalWrite(stepPinZ, LOW); delayMicroseconds(microstepDelay); }
  delay(150);
  digitalWrite(dirPinZ, HIGH); 
  for(int i=0; i<penLiftSteps; i++){ digitalWrite(stepPinZ, HIGH); delayMicroseconds(microstepDelay); digitalWrite(stepPinZ, LOW); delayMicroseconds(microstepDelay); }
  delay(150);
}

void resetSystem() {
  currentState = IDLE;
  totalDotsCount = 0;
  currentDotIndex = 0;
  currentX_Offset = 0;
  moveTo(0, 0); 
  Serial.println("System Ready/Reset.");
}