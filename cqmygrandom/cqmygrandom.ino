#include <math.h>

// --- 汉字 UTF-8 编码定义 ---
const byte CHAR_CHUANG[] = {0xE5, 0xBA, 0x8A}; 
const byte CHAR_QIAN[]   = {0xE5, 0x89, 0x8D}; 
const byte CHAR_MING[]   = {0xE6, 0x98, 0x8E}; 
const byte CHAR_YUE[]    = {0xE6, 0x9C, 0x88}; 
const byte CHAR_GUANG[]  = {0xE5, 0x85, 0x89}; 

// --- 引脚定义 ---
const int dirPinX = 10; const int stepPinX = 9;
const int dirPinY = 12; const int stepPinY = 11;
const int dirPinZ = 6;  const int stepPinZ = 5;

// --- 物理参数 ---
const float stepsPerMM = 1000.0;
const int microstepDelay = 50;
const int penLiftSteps = 1500;
const float dotPitch = 10.0;     
const float cellSpacing = 18.0;  
const float wordSpacing = 25.0;  

struct Dot { float x; float y; };
Dot brailleDots[400];           
int totalDotsCount = 0;         
float currentX_WritingOffset = 0; 

enum SystemState { IDLE, DRAWING, PAUSED };
SystemState currentState = IDLE;
int currentDotIndex = 0;
float currentX_mm = 0.0;
float currentY_mm = 0.0;

byte inputBuffer[3]; 
int bufferIdx = 0;

// --- 函数部分 ---

void addCellToQueue(int pattern, float nextGap) {
  float localX[] = {0, 0, 0, dotPitch, dotPitch, dotPitch};
  float localY[] = {0, dotPitch, dotPitch * 2.0, 0, dotPitch, dotPitch * 2.0};
  for (int i = 0; i < 6; i++) {
    if ((pattern >> i) & 1) {
      if (totalDotsCount < 400) {
        brailleDots[totalDotsCount++] = {currentX_WritingOffset + localX[i], localY[i]};
      }
    }
  }
  currentX_WritingOffset += nextGap; 
}

void processCharacter(int type) {
  switch(type) {
    case 1: 
      addCellToQueue(0b011111, cellSpacing); addCellToQueue(0b111110, wordSpacing); 
      Serial.println(">> [床] 已编入 |"); break;
    case 2: 
      addCellToQueue(0b011101, cellSpacing); addCellToQueue(0b110011, wordSpacing); 
      Serial.println(">> [前] 已编入 |"); break;
    case 3: 
      addCellToQueue(0b001101, cellSpacing); addCellToQueue(0b101011, wordSpacing); 
      Serial.println(">> [明] 已编入 |"); break;
    case 4: 
      addCellToQueue(0b111100, wordSpacing); 
      Serial.println(">> [月] 已编入 |"); break;
    case 5: 
      addCellToQueue(0b011011, cellSpacing); addCellToQueue(0b111110, wordSpacing); 
      Serial.println(">> [光] 已编入 |"); break;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(dirPinX, OUTPUT); pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT); pinMode(stepPinY, OUTPUT);
  pinMode(dirPinZ, OUTPUT); pinMode(stepPinZ, OUTPUT);
  Serial.println(">>> 系统就绪。请输入汉字，发送 's' 打印，发送 'r' 重置。");
}

void loop() {
  while (Serial.available() > 0) {
    byte b = Serial.read();

    // 1. 过滤掉换行符、回车符等控制字符 (ASCII < 32)
    // 但排除掉我们要用的 's', 'r', 'p' 等指令
    if (b < 32 && b != '\n' && b != '\r') continue; 

    // 2. 识别控制指令
    if (b == 's' || b == 'S' || b == 'r' || b == 'R' || b == 'p' || b == 'P') {
      handleControlCommand(b);
      bufferIdx = 0; // 输入指令时清空汉字缓冲区，防止错位
    } 
    // 3. 处理汉字字节 (UTF-8 汉字通常首字节 > 127)
    else if (b >= 0x80 || bufferIdx > 0) {
      inputBuffer[bufferIdx++] = b;
      if (bufferIdx == 3) {
        if (memcmp(inputBuffer, CHAR_CHUANG, 3) == 0)      processCharacter(1);
        else if (memcmp(inputBuffer, CHAR_QIAN, 3) == 0)   processCharacter(2);
        else if (memcmp(inputBuffer, CHAR_MING, 3) == 0)   processCharacter(3);
        else if (memcmp(inputBuffer, CHAR_YUE, 3) == 0)    processCharacter(4);
        else if (memcmp(inputBuffer, CHAR_GUANG, 3) == 0)  processCharacter(5);
        bufferIdx = 0; 
      }
    }
  }

  // 打印任务执行
  if (currentState == DRAWING && currentDotIndex < totalDotsCount) {
    moveMotorXY(brailleDots[currentDotIndex].x, brailleDots[currentDotIndex].y);
    performBrailleDot();
    currentDotIndex++;
    if (currentDotIndex >= totalDotsCount) {
      Serial.println("\n√ 任务打印完成。输入 'r' 复位或继续添加文字。");
      currentState = IDLE;
    }
  }
}

void handleControlCommand(char c) {
  if (c == 's' || c == 'S') {
    if (totalDotsCount > 0) {
      currentState = DRAWING;
      Serial.print("\n>>> 开始打印，总点数: "); Serial.println(totalDotsCount);
    }
  } else if (c == 'r' || c == 'R') {
    totalDotsCount = 0;
    currentDotIndex = 0;
    currentX_WritingOffset = 0;
    bufferIdx = 0;
    resetToOrigin();
    currentState = IDLE;
    Serial.println("\n<<< 已重置。队列已清空，坐标已回零。");
  } else if (c == 'p' || c == 'P') {
    currentState = PAUSED;
    Serial.println("\n||| 已暂停。");
  }
}

// --- 电机底层 (保持不变) ---
void moveMotorZ(bool up) {
  digitalWrite(dirPinZ, up ? HIGH : LOW);
  for (int i = 0; i < penLiftSteps; i++) {
    digitalWrite(stepPinZ, HIGH); delayMicroseconds(microstepDelay);
    digitalWrite(stepPinZ, LOW);  delayMicroseconds(microstepDelay);
  }
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
  moveMotorZ(false); delay(150); moveMotorZ(true); delay(150);
}
void resetToOrigin() {
  moveMotorXY(0, 0); currentX_mm = 0; currentY_mm = 0;
}