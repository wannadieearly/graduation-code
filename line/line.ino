// 定义引脚
const int PUL_PIN = 5;   // PUL+ 连接到数字引脚9
const int DIR_PIN = 6;  // DIR+ 连接到数字引脚10

// 步进电机参数
const int STEP_DELAY = 100; // 脉冲间隔（微秒），控制速度

// 全局变量
bool isRunning = false;     // 运行状态标志
bool isPaused = false;      // 暂停状态标志
unsigned long startTime;    // 开始时间
const unsigned long RUN_TIME = 3000; // 运行时间5秒（毫秒）

void setup() {
  // 初始化引脚模式
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  
  // 设置方向为正转（假设高电平为正转）
  digitalWrite(DIR_PIN, LOW);
  
  // 初始化串口通信（用于接收命令）
  Serial.begin(9600);
  Serial.println("步进电机控制系统");
  Serial.println("命令列表:");
  Serial.println("start - 开始5秒连续脉冲");
  Serial.println("pause - 暂停/继续");
  Serial.println("stop - 停止");
  Serial.println("status - 查看状态");
}

void loop() {
  // 检查串口命令
  checkSerialCommands();
  
  // 如果正在运行且未暂停
  if (isRunning && !isPaused) {
    // 检查是否超过5秒
    if (millis() - startTime >= RUN_TIME) {
      stopMotor();
      return;
    }
    
    // 产生脉冲
    generatePulse();
  }
}

// 产生单个脉冲
void generatePulse() {
  digitalWrite(PUL_PIN, HIGH);
  delayMicroseconds(STEP_DELAY);
  digitalWrite(PUL_PIN, LOW);
  delayMicroseconds(STEP_DELAY);
}

// 开始运行5秒
void startMotor() {
  if (!isRunning) {
    isRunning = true;
    isPaused = false;
    startTime = millis();
    Serial.println("开始5秒连续脉冲");
  } else {
    Serial.println("电机已在运行中");
  }
}

// 暂停或继续
void pauseMotor() {
  if (isRunning) {
    isPaused = !isPaused;  // 切换暂停状态
    if (isPaused) {
      Serial.println("电机已暂停");
    } else {
      Serial.println("电机继续运行");
      // 更新开始时间以补偿暂停的时间
      startTime = millis() - (millis() - startTime);
    }
  } else {
    Serial.println("电机未在运行");
  }
}

// 停止电机
void stopMotor() {
  isRunning = false;
  isPaused = false;
  Serial.println("电机已停止");
}

// 查看状态
void showStatus() {
  Serial.println("=== 电机状态 ===");
  Serial.print("运行状态: ");
  Serial.println(isRunning ? "运行中" : "停止");
  Serial.print("暂停状态: ");
  Serial.println(isPaused ? "已暂停" : "未暂停");
  
  if (isRunning) {
    unsigned long elapsed = millis() - startTime;
    unsigned long remaining = RUN_TIME - elapsed;
    Serial.print("已运行时间: ");
    Serial.print(elapsed / 1000.0);
    Serial.println(" 秒");
    Serial.print("剩余时间: ");
    Serial.print(max(0, remaining) / 1000.0);
    Serial.println(" 秒");
  }
  Serial.println("=================");
}

// 检查串口命令
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();        // 去除首尾空格
    command.toLowerCase(); // 转换为小写
    
    if (command == "start") {
      startMotor();
    }
    else if (command == "pause") {
      pauseMotor();
    }
    else if (command == "stop") {
      stopMotor();
    }
    else if (command == "status") {
      showStatus();
    }
    else {
      Serial.println("未知命令，可用命令: start, pause, stop, status");
    }
  }
}