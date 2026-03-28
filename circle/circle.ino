#include <math.h>
#include <Arduino.h>

const int dirPinX = 10;
const int stepPinX = 9;
const int dirPinY = 12;
const int stepPinY = 11;

const float radius = 400.0;
const int numSteps = 360;

// --- 新增状态控制变量 ---
bool isRunning = false;  // 运行状态：false 为暂停/停止，true 为运行
int currentI = 0;        // 记录当前画圆的进度索引

void setup() {
  Serial.begin(9600);

  while (!Serial); 
  
  pinMode(dirPinX, OUTPUT);
  pinMode(stepPinX, OUTPUT);
  pinMode(dirPinY, OUTPUT);
  pinMode(stepPinY, OUTPUT);
  
  digitalWrite(dirPinX, LOW);
  digitalWrite(stepPinX, LOW);
  digitalWrite(dirPinY, LOW);
  digitalWrite(stepPinY, LOW);

  Serial.println("=== 步进电机控制器已就绪 ===");
  Serial.println("发送 's' 开始运行");
  Serial.println("发送 'p' 暂停运行");
}

void moveMotor(int dirPin, int stepPin, bool dir, int steps) {
  digitalWrite(dirPin, dir);
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(100);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(25);
  }
}

// --- 新增串口命令检测函数 ---
void checkSerial() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 's' || cmd == 'S') {
      isRunning = true;
      Serial.println(">>> 状态：开始/继续");
    } 
    else if (cmd == 'p' || cmd == 'P') {
      isRunning = false;
      Serial.println("||| 状态：已暂停");
    }
  }
}

void loop() {
  // 1. 检查是否有新的串口指令
  checkSerial();

  // 2. 如果当前处于运行状态，则执行一步
  if (isRunning) {
    if (currentI <= numSteps) {
      float theta = 2.0 * PI * currentI / numSteps;
      float x = radius * cos(theta);
      float y = radius * sin(theta);

      int stepsX = round(x);
      int stepsY = round(y);

      // 执行电机运动
      if (stepsX != 0) moveMotor(dirPinX, stepPinX, stepsX > 0, abs(stepsX));
      if (stepsY != 0) moveMotor(dirPinY, stepPinY, stepsY > 0, abs(stepsY));

      // 打印进度
      Serial.print("进度: ");
      Serial.print(currentI);
      Serial.print("/");
      Serial.println(numSteps);

      currentI++; // 增加索引
    } else {
      // 完成一圈后自动停止
      isRunning = false;
      currentI = 0;
      Serial.println("=== 绘圆完成 ===");
    }
  }
}