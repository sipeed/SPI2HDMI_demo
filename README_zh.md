# SPI2HDMI 例程

[Sipeed SPI2HDMI](https://wiki.sipeed.com/spi2hdmi) 模块的例程代码。这个模块让单片机像驱动 ILI9341 SPI 屏一样驱动 HDMI 显示器。

| 目录 | 平台 | 内容 |
|---|---|---|
| [`ch32/demo`](ch32) | CH32V003 / CH32V004 | 在 2KB RAM 的单片机上播放 Bad Apple、生命游戏和分形 |
| [`ch32/arcade`](ch32/arcade) | CH32V004 | 八项菜单：小游戏、分形、视频，720p 输出，带 TE 同步 |
| [`esp32s3/`](esp32s3) | ESP32-S3（ESP-IDF） | 支持 SPI 和 QSPI 的 `esp_lcd` 驱动组件，以及一个串口交互测试程序 |
| [`esp32s3/retro-go`](esp32s3#retro-go) | ESP32-S3（ESP-IDF） | retro-go 模拟器合集（NES、Game Boy、Doom 等），HDMI 出画面和声音 |
| [`common/spi2hdmi.h`](common/spi2hdmi.h) | 任意单片机 | 单头文件驱动，填三个回调函数即可移植 |

## 模块引脚

```text
5V   GND   SCK   CS    RST
IO0  IO1   IO2   IO3   TE
```

| 引脚 | SPI 模式 | QSPI 模式 |
|---|---|---|
| 5V / GND | 5V 供电 | 同左 |
| SCK | 时钟 | 时钟 |
| CS | 片选，低有效 | 同左 |
| IO0 | MOSI | D0 |
| IO1 | D/C | D1 |
| IO2 | 不用 | D2 |
| IO3 | 悬空（选择 SPI） | D3；RST 释放时保持高电平即选择 QSPI |
| RST | 复位，低有效 | 同左 |
| TE | 帧同步输出 | 同左 |

所有信号脚都是 3.3V 电平。

不接任何主机时，模块上电会显示 8 色彩条，可以用来快速检查模块、线材和显示器。

## 获取代码

```sh
git clone --recursive https://github.com/sipeed/SPI2HDMI_demo.git
```

`--recursive` 会一并拉取两个子模块：CH32 例程用的 [ch32fun](https://github.com/cnlohr/ch32fun) 和 ESP32-S3 用的 retro-go 移植。只需要其中一个时，可以先不加这个参数，之后再执行 `git submodule update --init <路径>`。

## 信号质量

SPI 最高 80MHz，QSPI 最高 40MHz。这个频率对杜邦线和面包板来说偏高：如果画面抖动或出现噪点，请缩短连线、让地线紧挨 SCK，或者降低时钟频率。

## 许可

各目录保留各自的许可：`esp32s3/` 为 MIT（其中的 retro-go 子模块为 GPL-2.0）；`ch32/arcade` 因包含第三方游戏代码而采用 GPL-3.0-or-later（见其中的 `THIRD_PARTY.md`）。ch32fun 是独立项目，遵循其自身许可。
