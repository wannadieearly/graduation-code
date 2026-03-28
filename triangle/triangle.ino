// ==========================================
// 等边三角形绘图系统
// 路径：底边(右) -> 斜边(左上) -> 斜边(左下回到原点)
// ==========================================

const int X_PUL_PIN = 9;   
const int X_DIR_PIN = 10;  
const int Y_PUL_PIN = 11;  
const int Y_DIR_PIN = 12;  

const int STEP_DELAY = 50;    // 增加一点延迟确保双轴同步平稳
int STEPS_PER_SIDE = 40000;    

enum SystemState { STOPPED, RUNNING, PAUSED };
SystemState currentState = STOPPED;
bool isPaused = false;

int currentSide = 0;           // 0:底边, 1:右斜边, 2:左斜边
int currentSteps = 0;          
unsigned long startTime;       
const unsigned long TIMEOUT = 40000; 

const bool X_FORWARD = HIGH;   // 右
const bool X_BACKWARD = LOW;   // 左
const bool Y_FORWARD = HIGH;   // 下
const bool Y_BACKWARD = LOW;   // 上

void setup() {
  pinMode(X_PUL_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(Y_PUL_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("=== 等边三角形绘图系统 ===");
  printHelp();
}

void loop() {
  checkSerialCommands();
  if (currentState == RUNNING && !isPaused) {
    if (millis() - startTime >= TIMEOUT) { stopDrawing(); return; }
    drawTriangleStep();
  }
}

// 核心逻辑：利用插补原理走斜边
void drawTriangleStep() {
  switch (currentSide) {
    case 0: // 第1边：底边（纯向右）
      digitalWrite(X_DIR_PIN, X_FORWARD);
      pulseMotor(X_PUL_PIN);
      currentSteps++;
      break;
      
    case 1: // 第2边：右斜边（向左上运动）
      // X轴走 0.5步，Y轴走 0.866步。这里使用简单的比例累加算法
      digitalWrite(X_DIR_PIN, X_BACKWARD);
      digitalWrite(Y_DIR_PIN, Y_BACKWARD);
      
      // X轴移动（每2步脉冲1次，实现0.5比例）
      if (currentSteps % 2 == 0) pulseMotor(X_PUL_PIN);
      // Y轴移动（模拟0.866比例，约等于每1000步脉冲866次）
      if ((currentSteps * 866L / 1000L) > ((currentSteps - 1) * 866L / 1000L)) {
        pulseMotor(Y_PUL_PIN);
      }
      currentSteps++;
      break;
      
    case 2: // 第3边：左斜边（向左下运动回到原点）
      digitalWrite(X_DIR_PIN, X_BACKWARD);
      digitalWrite(Y_DIR_PIN, Y_FORWARD);
      
      if (currentSteps % 2 == 0) pulseMotor(X_PUL_PIN);
      if ((currentSteps * 866L / 1000L) > ((currentSteps - 1) * 866L / 1000L)) {
        pulseMotor(Y_PUL_PIN);
      }
      currentSteps++;
      break;
  }

  // 检查当前边是否结束
  if (currentSteps >= STEPS_PER_SIDE) {
    sideCompleted();
  }
}

void pulseMotor(int pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(STEP_DELAY);
  digitalWrite(pin, LOW);
  delayMicroseconds(STEP_DELAY);
}

void sideCompleted() {
  currentSteps = 0;
  currentSide++;
  Serial.print("完成第 "); Serial.print(currentSide); Serial.println(" 条边");
  
  if (currentSide >= 3) {
    currentState = STOPPED;
    currentSide = 0;
    Serial.println("√ 三角形绘制完成！");
  }
}

// 串口控制逻辑
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("start")) {
      currentState = RUNNING; isPaused = false; currentSide = 0; currentSteps = 0; startTime = millis();
      Serial.println("开始绘制三角形...");
    } 
    else if (input.equalsIgnoreCase("stop")) { stopDrawing(); }
    else if (input.startsWith("setside")) {
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        STEPS_PER_SIDE = input.substring(spaceIndex + 1).toInt();
        Serial.print("边长设为: "); Serial.println(STEPS_PER_SIDE);
      }
    }
  }
}

void stopDrawing() { currentState = STOPPED; Serial.println("停止"); }

void printHelp() {
  Serial.println("命令: start, stop, setside [步数]");
}