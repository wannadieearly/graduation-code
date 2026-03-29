#include <math.h>

// --- 串口识别相关 ---
const byte WO_CHAR_BYTES[] = {0xE6, 0x88, 0x91}; 
const int WO_CHAR_LENGTH = sizeof(WO_CHAR_BYTES);
byte receivedBytes[10];
int receivedCount = 0;

// --- 步进电机引脚定义 (使用你最新的引脚) ---
const int dirPinX = 12;   
const int stepPinX = 11; 
const int dirPinY = 10; 
const int stepPinY = 9; 
const int dirPinZ = 6; 
const int stepPinZ = 5; 

// --- 步进电机参数 ---
const float stepsPerMM = 1000.0; 
const int microstepDelay = 100; 
const int penLiftSteps = 5000; 

// --- 盲文点位定义 ---
const float dotSpacing = 5.0;
// “我”字的三个坐标点 (X, Y)
float brailleDotsX[] = {0.0, 5.0, 10.0}; 
float brailleDotsY[] = {0.0, 5.0, 0.0};
const int totalDots = 3;

// --- 运行状态管理 ---
enum SystemState { IDLE, DRAWING, PAUSED };
SystemState currentState = IDLE;
int currentDotIndex = 0;

float currentX_mm = 0.0;
float currentY_mm = 0.0;

void setup() {
  Serial.begin(9600);
  
  pinMode(dirPinX, OUTPUT);
  pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT);
  pinMode(stepPinY, OUTPUT);
  pinMode(dirPinZ, OUTPUT);
  pinMode(stepPinZ, OUTPUT);

  // 初始化：抬笔回到原点
  currentX_mm = 0.0;
  currentY_mm = 0.0;

  Serial.println(">>> 镜像修正版系统就绪。");
  Serial.println(">>> 输入汉字“我”触发，发送 's' 开始/继续，'p' 暂停。");
}

void loop() {
  // 1. 处理串口输入
  if (Serial.available()) {
    byte incoming = Serial.peek(); 

    if (incoming == 's' || incoming == 'S' || incoming == 'p' || incoming == 'P') {
      handleControlCommand(Serial.read());
    } 
    else {
      handleCharacterInput(Serial.read());
    }
  }

  // 2. 打印任务处理
  if (currentState == DRAWING) {
    if (currentDotIndex < totalDots) {
      Serial.print("--- 正在打印第 "); Serial.print(currentDotIndex + 1); Serial.println(" 个点...");
      
      moveMotorXY(brailleDotsX[currentDotIndex], brailleDotsY[currentDotIndex]);
      performBrailleDot();
      
      currentDotIndex++;
    } else {
      Serial.println("√ “我”字打印完成！");
      resetToOrigin();
      currentState = IDLE;
      currentDotIndex = 0;
    }
  }
}

void handleCharacterInput(byte b) {
  if (receivedCount < sizeof(receivedBytes)) {
    receivedBytes[receivedCount++] = b;
  }

  if (b == '\n' || b == '\r') {
    while (receivedCount > 0 && (receivedBytes[receivedCount-1] == '\n' || receivedBytes[receivedCount-1] == '\r')) {
      receivedCount--;
    }

    if (receivedCount == WO_CHAR_LENGTH) {
      bool match = true;
      for (int i=0; i<WO_CHAR_LENGTH; i++) {
        if (receivedBytes[i] != WO_CHAR_BYTES[i]) { match = false; break; }
      }

      if (match) {
        Serial.println("\n[ 识别成功：我 ]");
        Serial.println("● ○");
        Serial.println("○ ●");
        Serial.println("● ○");
        Serial.println(">>> 任务就绪。发送 's' 开始。");
        currentDotIndex = 0; 
      }
    }
    receivedCount = 0; 
  }
}

void handleControlCommand(char c) {
  if (c == 's' || c == 'S') {
    if (currentState == IDLE || currentState == PAUSED) {
      currentState = DRAWING;
      Serial.println(">>> 执行中...");
    }
  } 
  else if (c == 'p' || c == 'P') {
    if (currentState == DRAWING) {
      currentState = PAUSED;
      Serial.println("||| 已暂停。");
    }
  }
}

// --- 核心运动控制 ---

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

  // --- 修正镜像的关键点 ---
  // 原来是 (stepsX >= 0) ? HIGH : LOW
  // 现在改为 LOW 为正向，解决 X 轴镜像问题
  digitalWrite(dirPinX, (stepsX >= 0) ? LOW : HIGH); 
  
  // Y 轴保持原样（如果 Y 也镜像了，就把下面的 LOW/HIGH 对调）
  digitalWrite(dirPinY, (stepsY >= 0) ? HIGH : LOW); 

  stepsX = abs(stepsX);
  stepsY = abs(stepsY);
  long maxS = max(stepsX, stepsY);

  for (long i = 0; i < maxS; i++) {
    if (i < stepsX) digitalWrite(stepPinX, HIGH);
    if (i < stepsY) digitalWrite(stepPinY, HIGH);
    delayMicroseconds(microstepDelay);
    if (i < stepsX) digitalWrite(stepPinX, LOW);
    if (i < stepsY) digitalWrite(stepPinY, LOW);
    delayMicroseconds(microstepDelay);
  }
  currentX_mm = targetX_mm;
  currentY_mm = targetY_mm;
}

void performBrailleDot() {
  moveMotorZ(false); // 落笔
  delay(100);
  moveMotorZ(true);  // 抬笔
  delay(100);
}

void resetToOrigin() {
  moveMotorXY(0, 0);
}