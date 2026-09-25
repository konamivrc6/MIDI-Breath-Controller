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
| 电容 | C1 / C2 / C3 | C1 = 开关消抖 100nF；C2、C3 = 电源去耦 |
| 气管 | 硅胶软管 | 内径 4mm / 外径 6mm |

PCB 尺寸 60 × 50 mm 双面板，4 个 3.2mm 安装孔。
设计文件 `PCB/LnWS Breath Controller.eprj2`（EasyEDA Pro），两张 Gerber 中 **`2026-09-12` 是现行版本**（相对 06-07 版只改了丝印标注，铜箔完全一致）。

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

C1（100nF）并在开关公共端与 GND 之间做 RC 消抖，因此固件里**不做软件消抖**。

### MPX5010DP 接线

| 脚 | 名称 | 板上接到 |
|---|---|---|
| 1 | Vout | A0 |
| 2 | GND | GND |
| 3 | Vs | 5V |
| 4 | V1 | 5V ⚠️ |
| 5 | V2 | GND ⚠️ |
| 6 | VEX | 悬空 |

> ⚠️ **V1 / V2 偏离数据手册。** 数据手册要求这三个脚悬空（V1、V2、VEX 是工厂激光修调脚）。PCB 把 4 脚接到了 5V、5 脚接到了 GND。按 NXP 官方说法这些脚出厂时已被激光切断，所以大概率无影响；但如果出现零点漂移或量程异常，**这是第一个要查的地方**。面包板验证时建议 4/5/6 全悬空。

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
