# SPI2HDMI demos

Example code for the [Sipeed SPI2HDMI](https://wiki.sipeed.com/spi2hdmi) module, which lets a microcontroller drive an HDMI monitor the same way it would drive an ILI9341 SPI display.

中文说明见 [README_zh.md](README_zh.md)。

| Directory | Target | What it shows |
|---|---|---|
| [`ch32/demo`](ch32) | CH32V003 / CH32V004 | Bad Apple, Conway's Life and fractals on a 2 KB-RAM MCU |
| [`ch32/arcade`](ch32/arcade) | CH32V004 | Eight-entry menu with games, fractals and video at 720p, TE-synchronised |
| [`esp32s3/`](esp32s3) | ESP32-S3 (ESP-IDF) | `esp_lcd` panel driver for SPI and QSPI, plus an interactive console test |
| [`esp32s3/retro-go`](esp32s3#retro-go) | ESP32-S3 (ESP-IDF) | retro-go emulators (NES, Game Boy, Doom...) with HDMI picture and sound |
| [`common/spi2hdmi.h`](common/spi2hdmi.h) | any MCU | Portable single-header driver: fill in three callbacks |

## Module pins

```text
5V   GND   SCK   CS    RST
IO0  IO1   IO2   IO3   TE
```

| Pin | SPI mode | QSPI mode |
|---|---|---|
| 5V / GND | power input, 5 V | same |
| SCK | clock | clock |
| CS | chip select, active low | same |
| IO0 | MOSI | D0 |
| IO1 | D/C | D1 |
| IO2 | not used | D2 |
| IO3 | leave open (selects SPI) | D3; hold high while RST is released to select QSPI |
| RST | reset, active low | same |
| TE | frame-sync output | same |

All signal pins use 3.3 V logic.

Without any host connected the module shows eight colour bars after power-up, which is a quick way to check the module, cable and monitor.

## Protocol in one minute

SPI mode 0, MSB first. Commands follow the ILI9341 convention, so existing ILI9341 drivers work once the resolution is changed.

```text
0x3A 0x55                 pixel format RGB565
0x2A x0H x0L x1H x1L      column window
0x2B y0H y0L y1H y1L      row window
0x2C <pixels...>          pixel data, high byte first
```

Extensions: `0x88` pixel replication (1x to 16x), `0x8B` output mode (640x480@60 or 1280x720@30), `0x89` tint for grey formats, `0x8A` 16-colour palette. See [`common/spi2hdmi.h`](common/spi2hdmi.h) for the full list.

## Getting the code

```sh
git clone --recursive https://github.com/sipeed/SPI2HDMI_demo.git
```

`--recursive` also fetches the two submodules: [ch32fun](https://github.com/cnlohr/ch32fun) for the CH32 examples and the retro-go port for the ESP32-S3. Skip it and run `git submodule update --init <path>` later if you only need one of them.

## Signal integrity

SPI runs up to 80 MHz and QSPI up to 40 MHz. Those clocks are fast for jumper wires and breadboards: if the picture jitters or shows noise, shorten the wires, keep a ground wire next to SCK, or lower the clock.

## Licenses

Each directory keeps its own license: `esp32s3/` is MIT (the retro-go submodule is GPL-2.0), `ch32/arcade` is GPL-3.0-or-later because it includes third-party game code (see its `THIRD_PARTY.md`). ch32fun is a separate project under its own license.
