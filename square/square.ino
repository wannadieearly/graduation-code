// 定义引脚 - 使用两个电机控制XY运动
const int X_PUL_PIN = 9;   // X轴脉冲
const int X_DIR_PIN = 10;  // X轴方向
const int Y_PUL_PIN = 11;  // Y轴脉冲
const int Y_DIR_PIN = 12;  // Y轴方向

// 步进电机参数
const int STEP_DELAY = 50;    // 脉冲间隔（微秒）
int STEPS_PER_SIDE = 40000;    // 每条边的步数

// 状态枚举
enum SystemState { STOPPED, RUNNING, PAUSED };
SystemState currentState = STOPPED;
bool isPaused = false;

// 正方形控制
int currentSide = 0;           // 当前边 0-3
int currentSteps = 0;          // 当前边已走步数
unsigned long startTime;       // 开始时间
const unsigned long TIMEOUT = 30000; // 30秒超时

// 方向常量（根据实际硬件调整）
// 如果方向不对，可以修改这些值
const bool X_FORWARD = HIGH;   // X轴正向
const bool X_BACKWARD = LOW;   // X轴反向
const bool Y_FORWARD = LOW;   // Y轴正向
const bool Y_BACKWARD = HIGH;   // Y轴反向

void setup() {
  // 初始化引脚
  pinMode(X_PUL_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(Y_PUL_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  
  // 初始停止状态
  digitalWrite(X_PUL_PIN, LOW);
  digitalWrite(Y_PUL_PIN, LOW);
  
  // 设置初始方向
  digitalWrite(X_DIR_PIN, X_FORWARD);
  digitalWrite(Y_DIR_PIN, Y_FORWARD);
  
  // 串口初始化
  Serial.begin(9600);
  Serial.println("=== 正方形绘图系统 ===");
  printHelp();
  showCalibrationInfo();
}

void loop() {
  checkSerialCommands();
  
  if (currentState == RUNNING && !isPaused) {
    // 超时检查
    if (millis() - startTime >= TIMEOUT) {
      Serial.println("超时保护：停止");
      stopDrawing();
      return;
    }
    
    drawSquareStep();
  }
}

// 绘制正方形的一个步进
void drawSquareStep() {
  switch (currentSide) {
    case 0: // 第1边：向右（X轴正向）
      setXDirection(X_FORWARD);
      setYDirection(Y_FORWARD); // Y保持不动，但设置方向
      pulseMotor(X_PUL_PIN);
      break;
      
    case 1: // 第2边：向下（Y轴正向）
      setXDirection(X_FORWARD); // X保持不动，但设置方向
      setYDirection(Y_FORWARD);
      pulseMotor(Y_PUL_PIN);
      break;
      
    case 2: // 第3边：向左（X轴反向）
      setXDirection(X_BACKWARD);
      setYDirection(Y_FORWARD); // Y保持不动，但设置方向
      pulseMotor(X_PUL_PIN);
      break;
      
    case 3: // 第4边：向上（Y轴反向）← 这里修正了！
      setXDirection(X_BACKWARD); // X保持不动，但设置方向
      setYDirection(Y_BACKWARD); // 改为反向
      pulseMotor(Y_PUL_PIN);
      break;
  }
  
  currentSteps++;
  
  // 检查是否完成当前边
  if (currentSteps >= STEPS_PER_SIDE) {
    sideCompleted();
  }
}

// 设置X轴方向
void setXDirection(bool direction) {
  digitalWrite(X_DIR_PIN, direction);
}

// 设置Y轴方向
void setYDirection(bool direction) {
  digitalWrite(Y_DIR_PIN, direction);
}

// 脉冲电机
void pulseMotor(int pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(STEP_DELAY);
  digitalWrite(pin, LOW);
  delayMicroseconds(STEP_DELAY);
}

// 完成一条边
void sideCompleted() {
  Serial.print("完成第");
  Serial.print(currentSide + 1);
  
  // 显示边的方向
  switch (currentSide) {
    case 0: Serial.println("边（右）"); break;
    case 1: Serial.println("边（下）"); break;
    case 2: Serial.println("边（左）"); break;
    case 3: Serial.println("边（上）"); break;
  }
  
  currentSteps = 0;
  currentSide++;
  
  if (currentSide >= 4) {
    // 完成整个正方形
    squareCompleted();
  } else {
    // 准备下一条边
    Serial.print("开始第");
    Serial.print(currentSide + 1);
    switch (currentSide) {
      case 1: Serial.println("边（下）"); break;
      case 2: Serial.println("边（左）"); break;
      case 3: Serial.println("边（上）"); break;
    }
  }
}

// 正方形完成
void squareCompleted() {
  currentState = STOPPED;
  currentSide = 0;
  currentSteps = 0;
  
  Serial.println("=== 正方形绘制完成 ===");
  Serial.print("边长: ");
  Serial.print(STEPS_PER_SIDE);
  Serial.println(" 步");
  Serial.print("总步数: ");
  Serial.println(STEPS_PER_SIDE * 4);
  Serial.println("系统已停止，等待新指令...");
}

// 开始绘制
void startDrawing() {
  if (currentState == STOPPED) {
    currentState = RUNNING;
    isPaused = false;
    currentSide = 0;
    currentSteps = 0;
    startTime = millis();
    
    // 设置初始方向
    setXDirection(X_FORWARD);
    setYDirection(Y_FORWARD);
    
    Serial.println("=== 开始绘制正方形 ===");
    Serial.print("边长: ");
    Serial.print(STEPS_PER_SIDE);
    Serial.println(" 步");
    Serial.println("路径：右 → 下 → 左 → 上");
    Serial.println("开始第1边（右）...");
  } else {
    Serial.println("已在运行中");
  }
}

// 暂停/继续
void pauseDrawing() {
  if (currentState == RUNNING) {
    isPaused = !isPaused;
    if (isPaused) {
      Serial.println("已暂停");
    } else {
      Serial.println("继续绘制");
      startTime = millis(); // 重置超时计时
    }
  } else {
    Serial.println("未运行");
  }
}

// 停止绘制
void stopDrawing() {
  if (currentState != STOPPED) {
    currentState = STOPPED;
    isPaused = false;
    
    // 显示停止位置
    if (currentSide < 4) {
      Serial.print("在第");
      Serial.print(currentSide + 1);
      Serial.print("边第");
      Serial.print(currentSteps);
      Serial.println("步处停止");
    }
    
    currentSide = 0;
    currentSteps = 0;
    
    Serial.println("绘制已停止");
  }
}

// 设置边长
void setSideLength(int steps) {
  if (currentState == STOPPED) {
    if (steps >= 100 && steps <= 10000) {
      STEPS_PER_SIDE = steps;
      Serial.print("边长设置为: ");
      Serial.print(steps);
      Serial.println(" 步");
    } else {
      Serial.println("无效值，请输入100-10000");
    }
  } else {
    Serial.println("请先停止运行");
  }
}

// 显示状态
void showStatus() {
  Serial.println("=== 系统状态 ===");
  Serial.print("运行状态: ");
  switch (currentState) {
    case STOPPED: Serial.println("停止"); break;
    case RUNNING: Serial.println(isPaused ? "暂停" : "运行"); break;
  }
  
  if (currentState == RUNNING) {
    Serial.print("当前边: ");
    Serial.print(currentSide + 1);
    Serial.print("/4 ");
    switch (currentSide) {
      case 0: Serial.println("(右)"); break;
      case 1: Serial.println("(下)"); break;
      case 2: Serial.println("(左)"); break;
      case 3: Serial.println("(上)"); break;
    }
    Serial.print("当前边进度: ");
    Serial.print(currentSteps);
    Serial.print("/");
    Serial.println(STEPS_PER_SIDE);
  }
  
  Serial.print("边长设置: ");
  Serial.print(STEPS_PER_SIDE);
  Serial.println(" 步");
  Serial.print("脉冲间隔: ");
  Serial.print(STEP_DELAY);
  Serial.println(" 微秒");
  Serial.println("================");
}

// 显示校准信息
void showCalibrationInfo() {
  Serial.println("=== 方向校准信息 ===");
  Serial.print("X轴正向: ");
  Serial.println(X_FORWARD == HIGH ? "HIGH" : "LOW");
  Serial.print("X轴反向: ");
  Serial.println(X_BACKWARD == HIGH ? "HIGH" : "LOW");
  Serial.print("Y轴正向: ");
  Serial.println(Y_FORWARD == HIGH ? "HIGH" : "LOW");
  Serial.print("Y轴反向: ");
  Serial.println(Y_BACKWARD == HIGH ? "HIGH" : "LOW");
  Serial.println("如果方向不正确，请修改方向常量");
  Serial.println("====================");
}

// 检查串口命令
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.equalsIgnoreCase("start")) {
      startDrawing();
    }
    else if (input.equalsIgnoreCase("pause")) {
      pauseDrawing();
    }
    else if (input.equalsIgnoreCase("stop")) {
      stopDrawing();
    }
    else if (input.equalsIgnoreCase("status")) {
      showStatus();
    }
    else if (input.equalsIgnoreCase("calibrate")) {
      showCalibrationInfo();
    }
    else if (input.startsWith("setside")) {
      // 解析数字
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String numStr = input.substring(spaceIndex + 1);
        int steps = numStr.toInt();
        setSideLength(steps);
      } else {
        Serial.println("格式错误: setside 步数");
      }
    }
    else if (input.equalsIgnoreCase("test")) {
      testMotors();
    }
    else if (input.equalsIgnoreCase("help")) {
      printHelp();
    }
    else {
      Serial.println("未知命令，输入 'help' 查看帮助");
    }
  }
}

// 测试电机
void testMotors() {
  if (currentState == STOPPED) {
    Serial.println("=== 电机测试 ===");
    
    // 测试X轴正向
    Serial.println("测试X轴正向...");
    setXDirection(X_FORWARD);
    for (int i = 0; i < 100; i++) {
      pulseMotor(X_PUL_PIN);
    }
    delay(500);
    
    // 测试X轴反向
    Serial.println("测试X轴反向...");
    setXDirection(X_BACKWARD);
    for (int i = 0; i < 100; i++) {
      pulseMotor(X_PUL_PIN);
    }
    delay(500);
    
    // 测试Y轴正向
    Serial.println("测试Y轴正向...");
    setYDirection(Y_FORWARD);
    for (int i = 0; i < 100; i++) {
      pulseMotor(Y_PUL_PIN);
    }
    delay(500);
    
    // 测试Y轴反向
    Serial.println("测试Y轴反向...");
    setYDirection(Y_BACKWARD);
    for (int i = 0; i < 100; i++) {
      pulseMotor(Y_PUL_PIN);
    }
    
    Serial.println("测试完成");
  } else {
    Serial.println("请先停止运行再进行测试");
  }
}

// 打印帮助
void printHelp() {
  Serial.println("=== 可用命令 ===");
  Serial.println("  start     - 开始绘制正方形");
  Serial.println("  pause     - 暂停/继续绘制");
  Serial.println("  stop      - 停止绘制");
  Serial.println("  status    - 查看系统状态");
  Serial.println("  setside N - 设置边长(N=100-10000步)");
  Serial.println("  calibrate - 显示方向校准信息");
  Serial.println("  test      - 测试电机运行");
  Serial.println("  help      - 显示此帮助");
  Serial.println("=================");
}