/*
  呼吸控制器 - Pro Micro
  功能：
    - 读取 MPX5010DP 压力传感器 (A0)
    - 自动零点校准 (开机无气流)
    - 两种力度曲线 (线性 / 自定义)，由拨动开关 (引脚4) 切换
    - TM1637 四位数码管显示当前 CC 值 (引脚2,3)
    - 通过 USB-MIDI 发送 CC#2 (呼吸控制)
    - 主循环全程非阻塞；开关消抖由硬件 RC (100nF) 完成，软件只做边沿检测
*/

#include <MIDIUSB.h>
#include <TM1637Display.h>

// ==================== 引脚定义 ====================
#define PRESSURE_PIN    A0       // 压力传感器模拟输入
#define SWITCH_PIN      4        // 曲线切换开关 (INPUT_PULLUP)
#define CLK_PIN         2        // TM1637 时钟
#define DIO_PIN         3        // TM1637 数据

// ==================== 传感器参数 ====================
const float FULL_PRESSURE_KPA = 10.0;   // 传感器标称量程 10 kPa

// ★ 满量程标定值 —— 这项必须用实际的传感器实测，不要沿用上一块件的数据 ★
//
// 含义：净 ADC 计数 (已减零点) 达到多少时，认为压力到了 10 kPa / CC 到 127。
// 测法：烧录 testSensorMPX5010DP.ino，开串口监视器 (115200)
//   1. 传感器不通气          -> 记下 Raw，即零点 zero
//   2. 用平时最大的力度吹    -> 记下 Raw，即峰值 peak
//   3. FULL_RAW = peak - zero
//
// 参考：按数据手册 Vout = Vs*(0.09*P + 0.04)，5V 供电下 0→10 kPa
//       对应净读数约 921。若实测明显偏小，说明你最大吹气压力不到
//       10 kPa —— 那就直接用实测值，等于把「你吹得动的最大值」当作
//       满量程，这样 0~127 的整个范围都用得上，分辨率最好。
//
// 注意：getNetPressureKpa() 返回的 kPa 只有在 FULL_RAW 标定正确时
//       才有物理意义，否则它只是个线性刻度。
const int FULL_RAW = 500;               // TODO: 待新传感器实测后替换

// ==================== MIDI 设置 ====================
const byte MIDI_CHANNEL = 0;     // MIDI 通道 1 (0-based)
const byte CC_NUMBER = 2;       // 呼吸控制 CC
const unsigned long SEND_INTERVAL = 15;   // 发送最小间隔 (ms)

// ==================== 滤波与死区 ====================
const int AVG_SAMPLES = 8;      // 压力采样平均次数
const float DEAD_ZONE_KPA = 0.02;  // 死区压力 (kPa)，低于此值 CC=0
const int CALIB_SAMPLES = 100;     // 零点校准采样次数

// ==================== 显示 ====================
const unsigned long DISP_REFRESH = 50;   // 数码管刷新间隔 (ms)

// ==================== 曲线模式 ====================
enum CurveMode {
  LINEAR,
  CUSTOM
};

// ==================== 全局变量 ====================
TM1637Display display(CLK_PIN, DIO_PIN);   // 数码管对象

float zeroPressureRaw = 0;      // 零点 ADC 平均值 (原始读数)
CurveMode currentMode = LINEAR; // 当前曲线模式
byte lastCCValue = 255;        // 上次发送的 CC 值 (初始化为无效值)
unsigned long lastSendTime = 0;
unsigned long lastDisplayTime = 0;
bool lastSwitchState = HIGH;   // 上一次开关状态 (未按下为HIGH)

// ==================== 函数声明 ====================
void calibrateZero();
float readPressureRaw();
float getNetPressureKpa();
byte pressureToCC(float kPa, CurveMode mode);
void sendMidiCC(byte value);
void updateDisplay(byte value);
void checkSwitch();

// ==================== 初始化 ====================
void setup() {
  // 调试串口 (可选，如需查看压力值可取消注释)
  Serial.begin(115200);

  // 初始化引脚
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(PRESSURE_PIN, INPUT);

  // 初始化 TM1637
  display.setBrightness(7);  // 最大亮度
  display.clear();

  // 开机零点校准 (请确保此时传感器无气流)
  calibrateZero();

  // 读取当前开关状态，设置初始曲线模式
  lastSwitchState = digitalRead(SWITCH_PIN);
  currentMode = (lastSwitchState == HIGH) ? LINEAR : CUSTOM;
  // 启动时强制发送一次 CC=0
  sendMidiCC(0);
  lastCCValue = 0;
  updateDisplay(0);

  // Serial.println("Ready");
}

void loop() {
  // 1. 检查拨动开关是否变化 (边缘检测)
  checkSwitch();

  // 2. 获取当前净压力
  float netKpa = getNetPressureKpa();
  // 死区处理
  if (netKpa < DEAD_ZONE_KPA) netKpa = 0.0;

  // 3. 计算 CC 值
  byte cc = pressureToCC(netKpa, currentMode);

  // 4. 条件发送 MIDI
  if (cc != lastCCValue && (millis() - lastSendTime >= SEND_INTERVAL)) {
    sendMidiCC(cc);
    lastCCValue = cc;
    lastSendTime = millis();
  }

  // 5. 更新数码管 (非阻塞)
  if (millis() - lastDisplayTime >= DISP_REFRESH) {
    updateDisplay(cc);
    lastDisplayTime = millis();
  }

  // 6. 处理 USB-MIDI 主机接收 (必须周期性调用，避免数据堆积)
  MidiUSB.flush();
}

// ==================== 零点校准 ====================
void calibrateZero() {
  unsigned long sum = 0;
  for (int i = 0; i < CALIB_SAMPLES; i++) {
    sum += analogRead(PRESSURE_PIN);
    delay(2); // 短暂暂停，允许 ADC 稳定
  }
  zeroPressureRaw = (float)sum / CALIB_SAMPLES;
  // 调试输出零点 (保留注释方便测试)
  // Serial.print("Zero raw: "); Serial.println(zeroPressureRaw);
}

// ==================== 压力读取 ====================
float readPressureRaw() {
  // 多次采样平均滤波
  unsigned long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) {
    sum += analogRead(PRESSURE_PIN);
    delayMicroseconds(50);
  }
  return (float)sum / AVG_SAMPLES;
}

// 计算净压力 (kPa)
float getNetPressureKpa() {
  float raw = readPressureRaw();
  float netRaw = raw - zeroPressureRaw;
  // 限制在 0 ~ FULL_RAW 之间，防止负值或超出
  if (netRaw < 0) netRaw = 0;
  if (netRaw > FULL_RAW) netRaw = FULL_RAW;
  // 线性映射到 kPa：0 -> 0, FULL_RAW -> FULL_PRESSURE_KPA
  float kpa = (netRaw / FULL_RAW) * FULL_PRESSURE_KPA;
  return kpa;
}

// ==================== 曲线映射 ====================
byte pressureToCC(float kPa, CurveMode mode) {
  if (kPa <= 0.0) return 0;
  // 归一化压力 (0..1)
  float norm = kPa / FULL_PRESSURE_KPA;
  if (norm > 1.0) norm = 1.0;

  byte cc;
  switch (mode) {
    case LINEAR:
      // 线性映射: 0..127
      cc = (byte)(norm * 127.0 + 0.5);
      break;
    case CUSTOM:
      // 自定义曲线: 平方根 (初段灵敏，后段平缓)
      // 也防止陡增造成难以精细控制
      cc = (byte)(sqrt(norm) * 127.0 + 0.5);
      break;
  }
  return cc;
}

// ==================== MIDI 发送 ====================
void sendMidiCC(byte value) {
  // 构造 Control Change 消息
  midiEventPacket_t event = {
    (byte)0x0B,           // Cable 0 + 3 bytes of message
    (byte)(0xB0 | MIDI_CHANNEL), // Control Change on channel
    CC_NUMBER,            // CC 编号
    value                 // CC 值
  };
  MidiUSB.sendMIDI(event);
  // 立即 flush 以确保发出
  MidiUSB.flush();
  // 调试输出 (可选)
  // Serial.print("CC: "); Serial.println(value);
}

// ==================== 数码管更新 ====================
void updateDisplay(byte value) {
  // 显示 3 位数，右对齐，不填充前导零
  display.showNumberDec(value, false, 3, 0);
}

// ==================== 开关检测 ====================
// 硬件侧已经在开关两端并了 100nF 到 GND (PCB 上的 C1)，配合引脚内部
// 上拉形成了 RC 消抖，所以这里只做边沿检测，不加任何延时或重复确认
// —— 之前那句 delay(30) 是多余的，而且会阻塞主循环。
void checkSwitch() {
  bool currentState = digitalRead(SWITCH_PIN);
  if (currentState == lastSwitchState) return;

  lastSwitchState = currentState;
  // 更新曲线模式：高电平(未按下) -> 线性；低电平(按下) -> 自定义
  currentMode = (currentState == HIGH) ? LINEAR : CUSTOM;

  // 切换模式时，强制发送当前压力对应的新 CC 值，避免宿主残留旧值
  float netKpa = getNetPressureKpa();
  if (netKpa < DEAD_ZONE_KPA) netKpa = 0.0;
  byte cc = pressureToCC(netKpa, currentMode);
  sendMidiCC(cc);
  lastCCValue = cc;
  updateDisplay(cc);
  // 重置发送计时器，防止后续正常发送被间隔限制
  lastSendTime = millis();
}