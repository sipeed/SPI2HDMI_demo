# SPI2HDMI on ESP32-S3 (ESP-IDF)

- `components/esp_lcd_spi2hdmi` — an `esp_lcd` panel driver. It speaks classic 4-wire SPI or QSPI (the 32-bit framing used by SH8601/AXS15231B drivers), RGB565, arbitrary rectangles.
- `retro-go` — a port of the [retro-go](https://github.com/ducalex/retro-go) emulator collection (NES, Game Boy, Doom and more) with HDMI picture and sound, see [below](#retro-go).
- `examples/console_test` — interactive test on the UART console: switch between SPI and QSPI, change the clock, pick 480p or 720p and a zoom factor, draw test patterns, measure TE, play a test tone over HDMI audio.

Built and run with ESP-IDF v5.4.2.

## Wiring used by the example

| ESP32-S3 | SPI mode | QSPI mode | SPI2HDMI |
|---:|---|---|---|
| GPIO12 | SCK | SCK | SCK |
| GPIO10 | CS | CS | CS |
| GPIO11 | MOSI | D0 | IO0 |
| GPIO13 | D/C | D1 | IO1 |
| GPIO14 | – | D2 | IO2 |
| GPIO9 | – | D3 + mode strap | IO3 |
| GPIO8 | RST | RST | RST |
| GPIO3 | TE | TE | TE |
| 5V / GND | power | power | 5V / GND |

The module chooses SPI or QSPI from the level on IO3 when RST is released. The example drives IO3 and pulses RST itself, so one wiring covers both modes.

## Build and run

```sh
. /path/to/esp-idf/export.sh
cd esp32s3/examples/console_test
idf.py set-target esp32s3
idf.py -p /dev/ttyACM0 flash monitor
```

Type `help` in the monitor. Useful commands: `mode spi`, `mode qspi`, `freq 40`, `480 2`, `720 2`, `bars`, `grid`, `status`.

Start at 40 MHz. 80 MHz works on short, direct wiring but is too fast for long jumper wires or a breadboard; jitter or noise in the picture means the clock is too high for the wiring.

## retro-go

`retro-go/` is a git submodule pointing at a fork of retro-go that adds an `spi2hdmi-s3` target: 320×240 games scaled 2× to 640×480 over QSPI, with sound sent as HDMI audio. It uses the same wiring as the table above.

```sh
git submodule update --init esp32s3/retro-go
cd esp32s3/retro-go
python rg_tool.py --target spi2hdmi-s3 --port /dev/ttyACM0 run
```

See retro-go's own `BUILDING.md` for the build tool, and `tools/uart_pad.py` for playing with a PC keyboard over the serial port; a USB gamepad on the ESP32-S3's USB port works too. ROMs are not included: copy your own into the flash storage partition. retro-go is licensed under GPL-2.0.

## Using the component in your project

Copy `components/esp_lcd_spi2hdmi` into your project's `components/`. Create an SPI panel IO with `lcd_cmd_bits = 32`, `lcd_param_bits = 8`, `dc_gpio_num = -1` and `flags.quad_mode = 1` for QSPI, or `lcd_cmd_bits = 8` with a D/C GPIO for classic SPI, then call `esp_lcd_new_panel_spi2hdmi()`, `esp_lcd_panel_init()` and `esp_lcd_panel_draw_bitmap()`.

Pixels are RGB565, high byte first. `esp_lcd_spi2hdmi_set_mode(panel, 1)` switches to 1280x720@30 (logical 640x360 at 2x); `esp_lcd_spi2hdmi_set_scale()` changes the zoom. Rotation and mirroring are not supported.
