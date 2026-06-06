/*
 * MPX5010DP 传感器测试
 * 读取 A0 模拟值，转换为电压并打印。
 * 打开串口监视器（波特率 115200），观察数值变化。
 */

const int sensorPin = A0;  // 压力传感器接 A0

void setup() {
  Serial.begin(115200);     // 启动串口通信
  pinMode(sensorPin, INPUT);
}

void loop() {
  int raw = analogRead(sensorPin);                     // 读取原始值 (0-1023)
  float voltage = raw * (5.0 / 1023.0);                // 转换为电压 (Pro Micro 5V 参考)
  float pressure_kPa = (voltage / 5.0 - 0.04) / 0.09;  // 根据手册线性公式估算 kPa
  // 公式推导：Vout = Vs * (0.09 * P + 0.04) ，P 单位为 kPa
  // 反推：P = (Vout/Vs - 0.04) / 0.09

  // 打印数值（原始值、电压、估算压力）
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print("\tVoltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V");
  Serial.print("\tPressure: ");
  Serial.print(pressure_kPa, 2);
  Serial.println(" kPa");

  delay(200);  // 每 200ms 刷新一次，避免数据太快看不清
}