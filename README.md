# LnWS Breath Controller

吹气式 MIDI 呼吸控制器。MPX5010DP 气压传感器 + Pro Micro，把吹气压力映射成 MIDI CC#2 发给宿主。

- 开机自动零点校准
- 两种力度曲线（线性 / 平方根），由拨动开关切换
- TM1637 四位数码管实时显示当前 CC 值
- 主循环全程非阻塞

## 硬件

| 部件 | 型号 | 备注 |
|---|---|---|
| 主控 | Pro Micro (ATmega32U4, 5V / 16 MHz) | 2×12 排针插座 |
| 压力传感器 | MPX5010DP | 6 脚单排 SIP，量程 10 kPa |
| 显示 | TM1637 四位数码管 | 4 脚，2.54mm 间距 |
| 开关 | 3 脚拨动开关 | 2.0mm 间距 |
| 消抖电容 | 3 × 1µF 0805 MLCC | 并联，接开关公共端与 GND 之间 |
| 上拉 | 4.7k 0805 | 开关输入，保证触点湿电流 |
| 去耦电容 | 直插 1 枚 | 传感器旁电源去耦 |
| 气管 | 硅胶软管 | 内径 4mm / 外径 6mm |

PCB 尺寸 60 × 50 mm 双面板，4 个 3.2mm 安装孔。
设计文件 `PCB/LnWS Breath Controller.eprj2`（EasyEDA Pro），现行 Gerber 为 **`2026-10-01`**。

该版相对 06-07 首版的主要变化：布线由纯走线改为大面积铺铜；开关消抖电容由直插 100nF 换成 3 颗并联的 1µF 0805 MLCC；新增 4.7k 湿电流上拉。

### 引脚对照表

下表由 Gerber 铜箔层走线逆向得出，与固件里的定义一致。

| 信号 | Pro Micro 引脚 | 说明 |
|---|---|---|
| 传感器 Vout | **A0**（18 / PF7） | 模拟输入 |
| 传感器 GND | GND | |
| 传感器 Vs | VCC（5V） | |
| 数码管 CLK | **2** | |
| 数码管 DIO | **3** | |
| 拨动开关 | **4** | 接开关公共端；另一侧接地，靠 `INPUT_PULLUP` |
| 数码管 VCC | VCC（5V） | |
| 数码管 GND | GND | |

### 开关消抖的设计依据

开关公共端接 **4.7k 上拉**，并 **3 颗 1µF 0805 MLCC 并联到 GND**，构成 RC 消抖。固件里**不做软件消抖**，全部靠这段硬件完成。

两个参数都是被约束推出来的，**不要随手改其中一个**：

**上拉为什么是 4.7k —— 湿电流（wetting current）。**
机械开关的触点会形成氧化膜，电流太小就击不穿，接触电阻变得不稳定。镍/银镀层要求 **≥1mA**。触点闭合时电流 = 5V / R，所以 **R ≤ 5k**。取 4.7k → 1.06mA，是在满足湿电流的前提下能取的最大阻值。
（注意：Pro Micro 内部上拉是 20~50k，也就是 0.1~0.25mA —— 经典「内部上拉 + 100nF」的做法本来就跑在湿电流以下。）

**电容为什么是 3 颗 —— DC bias。**
MLCC 在直流偏压下容值会衰减，且衰减按**每颗**计算，并联不会改善比例。X7R 0805 在 5V 偏压下的典型衰减：25V 耐压约 25%，16V 约 35~45%，X5R 更差。所以不能按标称值算，要按**有效值**算：

| 规格 | 颗数 | 标称 | 有效值 | τ = R_total × C_eff |
|---|---|---|---|---|
| 1µF 0805 25V X7R | 2 | 2.0 µF | 1.5 µF | 5.7 ~ 6.4 ms |
| 1µF 0805 16V X7R | 3 | 3.0 µF | 1.65 ~ 1.95 µF | 6.9 ~ 7.7 ms |
| 1µF 0805 16V X5R | 3 | 3.0 µF | 1.35 ~ 1.65 µF | 5.7 ~ 6.4 ms |

本板用 3 颗，对上述任一规格都留有余量（小型滑动开关典型抖动 1~5ms）。反过来，只放 1 颗在衰减超过 30% 时 τ 就会掉到 3ms 以下，压不住抖动。

**并联上拉的影响 —— 反而让 τ 变确定了。**
固件保留 `INPUT_PULLUP`，内部上拉（20~50 kΩ）与外部 4.7k 并联，总阻为 **3.81 ~ 4.30 kΩ**：

| R_pu | R_total | 湿电流 |
|---|---|---|
| 20 kΩ | 3.81 kΩ | 1.31 mA |
| 30 kΩ | 4.06 kΩ | 1.23 mA |
| 50 kΩ | 4.30 kΩ | 1.16 mA |

因为 4.7k 远小于内部上拉，**它主导了并联结果** —— 内部那个 2.5 倍的离散被压缩到 1.13 倍，τ 实际上是确定的。副带好处是湿电流从 1.06mA 抬到 1.16~1.31mA。
保留内部上拉还有兜底作用：万一外部 4.7k 虚焊，τ 会变长到 30~75ms，模式切换变迟钝但功能不丢。

> ⚠️ 改动这里的 R 或 C 之前，务必重算**湿电流**和 **DC bias 后的有效容值** —— 这两者是互相拉扯的（湿电流要求 R 小，消抖要求 R×C 大），只调一个必然破坏另一个。

### MPX5010DP 接线

| 脚 | 名称 | 板上接到 |
|---|---|---|
| 1 | Vout | A0 |
| 2 | GND | GND |
| 3 | Vs | 5V |
| 4 | V1 | 悬空 |
| 5 | V2 | 悬空 |
| 6 | VEX | 悬空 |

> ✅ **4 / 5 / 6 脚全部悬空，符合数据手册。** V1、V2、VEX 是工厂激光修调脚，NXP 明确要求悬空不接。
> 早期版本曾把 4 脚接到 5V、5 脚接到 GND（偏离数据手册），现行版本已修正。

> ⚠️ **不要直接焊接传感器。** 引脚粗、散热快，烙铁停留稍久就会烫坏——本项目已经因此报废过一块。建议在板上装 **6 脚圆孔母座**，传感器插上去，既不烫坏也方便更换。

> ⚠️ **1 脚是缺口标记的那一端。** 插反会把 Vout 直接接到 5V 上。

## 开发环境

### 1. 安装依赖库

把 `libraries/` 下的 **`MIDIUSB`** 和 **`TM1637`** 拷贝到：

```
%USERPROFILE%\Documents\Arduino\libraries\
```

### 2. 安装开发板定义

这个板子定义不在官方 Arduino AVR 核里，需要手动追加。

把 `boards_add in the end.txt` 的内容**追加到文件末尾**：

```
%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\<版本>\boards.txt
```

追加后重启 Arduino IDE，开发板菜单里会出现 **"LnWS Breath Controller"**。

> ⚠️ **Arduino AVR 核一升级，这个文件就会被重置**，需要重新追加。

定义本身只是「SparkFun Pro Micro 换了个 USB 设备名」，其中有两处是踩过坑才改对的：

- **`build.variant=leonardo`** —— 官方核里没有 `promicro` variant。Leonardo 与 Pro Micro 的引脚映射逐位相同（已核对两张 `PROGMEM` 表的 `digital_pin_to_port_PGM` / `digital_pin_to_bit_mask_PGM`），所以直接复用。
- **`upload.tool.serial` / `upload.tool.default=avrdude`** —— 这两行原本没有，IDE 2.x 上传时会报 `Property 'upload.tool.serial' is undefined`。

> ⚠️ **不要点 "Burn Bootloader / 烧录引导程序"。** 定义里引用的 `Caterina-promicro16.hex` 在官方核中不存在，会报错。Pro Micro 出厂自带引导程序，正常上传（Upload）不受影响。

上传后设备会显示为 **`SparkFun LnWS Breath Controller`** —— VID 是 `0x1b4f`，而 Arduino 核里对这个 VID 硬编码了厂商名 "SparkFun"（见 `cores/arduino/USBCore.cpp`）。

## 标定

固件里的 `FULL_RAW` 是「净 ADC 计数 → 满量程」的换算基准，**必须用实际传感器实测确定**，不能沿用别的传感器的数据。代码里留了 TODO。

1. 烧录 `testSensorMPX5010DP/testSensorMPX5010DP.ino`
2. 打开串口监视器（115200）
3. **传感器不通气** → 记下 Raw 值，即零点 `zero`
4. **用平时最大的力度吹** → 记下 Raw 值，即峰值 `peak`
5. 把 `FULL_RAW = peak - zero` 填回 `LnWSBreathController.ino`

参考：按数据手册 `Vout = Vs*(0.09*P + 0.04)`，5V 供电下 0→10 kPa 对应净读数约 **921**。若实测明显偏小，说明最大吹气压力不到 10 kPa——那就直接用实测值，等于把「你吹得动的最大值」当作满量程，0~127 整个范围都用得上，分辨率最好。

## 目录

```
Code/LnWSBreathController/   固件（Arduino sketch）
PCB/                         EasyEDA Pro 工程 + Gerber
Casing/                      外壳（Blender 源文件 + STL）
libraries/                   依赖库（MIDIUSB、TM1637）
testSensorMPX5010DP/         传感器单独测试用 sketch
3D_PCB1_2026-06-07.step      PCB 3D 模型（供外壳建模参照）
```
