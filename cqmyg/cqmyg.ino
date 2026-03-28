#include <math.h>

// --- 串口识别相关 (床前明月光 UTF-8) ---
const byte TARGET_PHRASE[] = {0xE5, 0xBA, 0x8A, 0xE5, 0x89, 0x8D, 0xE6, 0x98, 0x8E, 0xE6, 0x9C, 0x88, 0xE5, 0x85, 0x89};
const int PHRASE_LENGTH = sizeof(TARGET_PHRASE);
byte receivedBytes[25];
int receivedCount = 0;

// --- 步进电机引脚 ---
const int dirPinX = 10; const int stepPinX = 9;
const int dirPinY = 12; const int stepPinY = 11;
const int dirPinZ = 6;  const int stepPinZ = 5;

// --- 物理参数 ---
const float stepsPerMM = 1000.0;
const int microstepDelay = 50;
const int penLiftSteps = 1500;

// --- 盲文规格定义 ---
const float dotPitch = 10;    
const float cellSpacing = 20; 
const float wordSpacing = 20.0; 

struct Dot { float x; float y; };
Dot brailleDots[200]; 
int totalDotsCount = 0;

// --- 运行状态管理 ---
enum SystemState { IDLE, DRAWING, PAUSED };
SystemState currentState = IDLE;
int currentDotIndex = 0;
float currentX_mm = 0.0;
float currentY_mm = 0.0;

// 辅助函数：向点位数组添加一个盲文方
void addCell(int pattern, float &offsetX, float nextGap) {
  float localX[] = {0, 0, 0, dotPitch, dotPitch, dotPitch};
  float localY[] = {0, dotPitch, dotPitch*2, 0, dotPitch, dotPitch*2};
  
  for (int i = 0; i < 6; i++) {
    if ((pattern >> i) & 1) {
      brailleDots[totalDotsCount++] = {offsetX + localX[i], localY[i]};
    }
  }
  offsetX += nextGap; 
}

// 基于“现行盲文”标准设置点位
void setupPhraseDots() {
  totalDotsCount = 0;
  float x = 0;
  // 床 (ch + uang), 前 (q + ian), 明 (m + ing), 月 (üe), 光 (g + uang)
  addCell(0b011111, x, cellSpacing); addCell(0b111110, x, wordSpacing); 
  addCell(0b011101, x, cellSpacing); addCell(0b110011, x, wordSpacing); 
  addCell(0b001101, x, cellSpacing); addCell(0b101011, x, wordSpacing); 
  addCell(0b111100, x, wordSpacing); 
  addCell(0b011011, x, cellSpacing); addCell(0b111110, x, wordSpacing); 
}

void setup() {
  Serial.begin(9600);
  pinMode(dirPinX, OUTPUT); pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT); pinMode(stepPinY, OUTPUT);
  pinMode(dirPinZ, OUTPUT); pinMode(stepPinZ, OUTPUT);

  setupPhraseDots(); 

  Serial.println(">>> 严格盲文打印系统就绪。");
  Serial.println(">>> 输入“床前明月光”查看盲文，发送 's' 开始打印。");
}

void loop() {
  if (Serial.available()) {
    byte incoming = Serial.peek();
    if (incoming == 's' || incoming == 'S' || incoming == 'p' || incoming == 'P') {
      handleControlCommand(Serial.read());
    } else {
      handleCharacterInput(Serial.read());
    }
  }

  if (currentState == DRAWING) {
    if (currentDotIndex < totalDotsCount) {
      moveMotorXY(brailleDots[currentDotIndex].x, brailleDots[currentDotIndex].y);
      performBrailleDot();
      currentDotIndex++;
    } else {
      Serial.println("\n√ 物理打印完成。已复位。");
      resetToOrigin();
      currentState = IDLE;
      currentDotIndex = 0;
    }
  }
}

// --- 核心修改部分：串口识别与盲文返回 ---
void handleCharacterInput(byte b) {
  if (receivedCount < sizeof(receivedBytes)) {
    receivedBytes[receivedCount++] = b;
  }
  
  // 检查最后接收到的字节序列是否匹配目标短语
  if (receivedCount >= PHRASE_LENGTH) {
    bool match = true;
    for (int i = 0; i < PHRASE_LENGTH; i++) {
      // 检查滑动窗口中的内容
      if (receivedBytes[receivedCount - PHRASE_LENGTH + i] != TARGET_PHRASE[i]) {
        match = false;
        break;
      }
    }

    if (match) {
      Serial.println("\n------------------------------------");
      Serial.println("[ 识别成功：床前明月光 ]");
      // 对应现行盲文符号：
      // 床(⠟⠮) 前(⠵⠳) 明(⠍⠗) 月(⠼) 光(⠛⠮)
      Serial.print("对应盲文编码: ");
      Serial.println("⠟⠮ ⠵⠳ ⠍⠗ ⠼ ⠛⠮"); 
      Serial.println("------------------------------------");
      Serial.println(">>> 输入 's' 启动电机进行物理打印...");
      
      currentDotIndex = 0;
      receivedCount = 0; // 匹配后重置缓冲区
    }
  }

  // 防止缓冲区溢出
  if (receivedCount >= 24) receivedCount = 0; 
}

void handleControlCommand(char c) {
  if (c == 's' || c == 'S') { 
    if (totalDotsCount > 0) {
      currentState = DRAWING; 
      Serial.println(">>> 正在执行物理冲压打印..."); 
    }
  }
  else if (c == 'p' || c == 'P') { currentState = PAUSED; Serial.println("||| 暂停。"); }
}

// --- 电机控制部分 (保持原样) ---
void moveMotorZ(bool up) {
  digitalWrite(dirPinZ, up ? HIGH : LOW);
  for (int i = 0; i < penLiftSteps; i++) {
    digitalWrite(stepPinZ, HIGH);
    delayMicroseconds(microstepDelay);
    digitalWrite(stepPinZ, LOW);
    delayMicroseconds(microstepDelay);
  }
  delay(100);
}

void moveMotorXY(float targetX_mm, float targetY_mm) {
  long stepsX = (long)((targetX_mm - currentX_mm) * stepsPerMM);
  long stepsY = (long)((targetY_mm - currentY_mm) * stepsPerMM);
  digitalWrite(dirPinX, (stepsX >= 0) ? HIGH : LOW); 
  digitalWrite(dirPinY, (stepsY >= 0) ? LOW : HIGH); 
  stepsX = abs(stepsX); stepsY = abs(stepsY);
  long maxS = max(stepsX, stepsY);
  for (long i = 0; i < maxS; i++) {
    if (i < stepsX) digitalWrite(stepPinX, HIGH);
    if (i < stepsY) digitalWrite(stepPinY, HIGH);
    delayMicroseconds(microstepDelay);
    if (i < stepsX) digitalWrite(stepPinX, LOW);
    if (i < stepsY) digitalWrite(stepPinY, LOW);
    delayMicroseconds(microstepDelay);
  }
  currentX_mm = targetX_mm; currentY_mm = targetY_mm;
}

void performBrailleDot() {
  moveMotorZ(false); // 冲压
  delay(200);
  moveMotorZ(true);  // 抬起
  delay(200);
}

void resetToOrigin() { moveMotorXY(0, 0); }