/*
 * TM1637 四位数码管测试
 *
 * 用途：单独验证数码管本身和它的接线，不涉及压力传感器和开关。
 * 四个测试项循环执行，全程在串口打印当前在测什么（波特率 115200）。
 *
 * 接线（与主板一致，见 README 的引脚对照表）
 *   CLK -> D2
 *   DIO -> D3
 *   VCC -> 5V
 *   GND -> GND
 *
 * 故障对照
 *   完全黑屏            -> 先查 VCC / GND；再确认 CLK 和 DIO 都接上了
 *   只亮一部分位/段     -> 数码管模块本身损坏
 *   乱码、闪烁、跳字    -> CLK 与 DIO 接反（最常见的一种）
 *   亮度忽明忽暗        -> 供电不足，给模块 VCC 就近加 100nF 去耦
 *   串口一片空白        -> 波特率不是 115200，或者 COM 口选错
 */

#include <TM1637Display.h>

// ==================== 接线 ====================
#define CLK_PIN   2
#define DIO_PIN   3

// ==================== 节奏 ====================
const unsigned long HOLD_MS = 1500;   // 每项测试停留时间
const unsigned long STEP_MS = 300;    // 单项内每步的间隔

// ==================== 全局 ====================
TM1637Display display(CLK_PIN, DIO_PIN);

// 四位全段点亮用的段码（0xff = 该位所有段都亮）
const uint8_t ALL_ON[4] = { 0xff, 0xff, 0xff, 0xff };

// ==================== 初始化 ====================
void setup() {
  Serial.begin(115200);
  // Pro Micro 的串口是 USB CDC。等终端连上来，但最多等 3 秒，
  // 否则没打开串口监视器时会永远卡在这里。
  while (!Serial && millis() < 3000) { }

  Serial.println(F("=== TM1637 数码管测试 ==="));
  Serial.print(F("CLK = D")); Serial.print(CLK_PIN);
  Serial.print(F("    DIO = D")); Serial.println(DIO_PIN);
  Serial.println();

  display.setBrightness(7);
  display.clear();
  delay(200);
}

// ==================== 主循环 ====================
void loop() {
  testAllSegments();
  testBrightness();
  testCountUp();
  testFirmwareStyle();

  Serial.println(F("--- 一轮结束，从头再来 ---"));
  Serial.println();
}

// ==================== 1. 全段点亮 ====================
// 确认模块没坏、每一位都能亮，顺便确认位序
void testAllSegments() {
  Serial.println(F("[1/4] 全段点亮"));
  display.setBrightness(7);

  display.setSegments(ALL_ON, 4, 0);
  Serial.println(F("      四位全亮 8.8.8.8"));
  Serial.println(F("      如果有某一段不亮，记下是第几位、哪一段"));
  delay(HOLD_MS);

  // 逐位单独点亮：从左到右依次是第 1、2、3、4 位
  for (uint8_t pos = 0; pos < 4; pos++) {
    uint8_t one[4] = { 0, 0, 0, 0 };
    one[pos] = 0xff;
    display.setSegments(one, 4, 0);
    Serial.print(F("      只亮第 ")); Serial.print(pos + 1); Serial.println(F(" 位（从左数）"));
    delay(STEP_MS * 2);
  }

  display.clear();
  delay(200);
}

// ==================== 2. 亮度扫描 ====================
// 0 最暗、7 最亮。TM1637 的亮度 0 不是关屏，是 1/16 占空比，仍然可见
void testBrightness() {
  Serial.println(F("[2/4] 亮度 0 → 7（0 最暗，7 最亮）"));
  display.showNumberDec(8888, false, 4, 0);

  for (uint8_t b = 0; b <= 7; b++) {
    display.setBrightness(b);
    Serial.print(F("      亮度 ")); Serial.println(b);
    delay(STEP_MS * 2);
  }

  display.setBrightness(7);
  display.clear();
  delay(200);
}

// ==================== 3. 计数 0 → 127 ====================
// 127 是 MIDI CC 的最大值，正好覆盖实际会用到的整个范围
void testCountUp() {
  Serial.println(F("[3/4] 计数 0 → 127（MIDI CC 的值域）"));
  display.setBrightness(7);

  for (int v = 0; v <= 127; v++) {
    display.showNumberDec(v, false, 4, 0);
    delay(15);
  }

  delay(HOLD_MS);
  display.clear();
  delay(200);
}

// ==================== 4. 两种调用方式对比 ====================
// LnWSBreathController.ino 里用的是 length=3、pos=0，
// 也就是只写前三位，最右边那一位从头到尾没被写过。
// 这里把两种写法并排演示一下，差别一眼可见。
void testFirmwareStyle() {
  const int demo = 42;

  Serial.println(F("[4/4] 对比两种调用方式，看最右边那一位"));

  // --- 主固件当前用法 ---
  Serial.println(F("      (a) showNumberDec(42, false, 3, 0)  <- 固件现在的写法"));
  Serial.println(F("          只写前三位，第 4 位保持空白"));
  display.clear();
  display.setBrightness(7);
  display.showNumberDec(demo, false, 3, 0);
  delay(HOLD_MS * 2);

  // --- 四位右对齐 ---
  Serial.println(F("      (b) showNumberDec(42, false, 4, 0)  <- 占满四位、右对齐"));
  display.clear();
  display.showNumberDec(demo, false, 4, 0);
  delay(HOLD_MS * 2);

  // --- 边界值也过一眼 ---
  display.clear();
  Serial.println(F("      (b) 在边界值 0 / 127 上的表现"));
  display.showNumberDec(0, false, 4, 0);
  delay(STEP_MS * 3);
  display.showNumberDec(127, false, 4, 0);
  delay(HOLD_MS);

  display.clear();
  delay(200);
}
