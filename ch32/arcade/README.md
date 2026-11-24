# CH32V004 SPI2HDMI arcade

Independent CH32V004 **32 KiB Flash / 6 KiB RAM** project. It does not replace
or share `main` with `../demo`. Boots to an eight-entry HDMI menu. No LVGL or
benchmark is linked. PD6 retains the active-high breathing LED.

| Key | Entry | Content / controls |
|---|---|---|
| 1 | Tiny Invaders | A/D move, J starts/fires |
| 2 | Snake | WASD direction |
| 3 | Pong | W/S paddle |
| 4 | Jump Bird | Tap J to flap |
| 5 | Dino Run | J jump, S crouch |
| 6 | Fractals | J cycles nine named procedural patterns |
| 7 | Conway | J cycles Gosper gun, R-pentomino, spaceships, random |
| 8 | Bad Apple | 160x90 monochrome video; J restarts |
| 0 / Q / K | Menu | Return from any entry |

W/S chooses a menu item, J/Space/Enter opens it. On all four MiniGameConsole
game-over screens, **W/Up or J restarts the current game; Q/K returns to the
menu**. Release the previous action before pressing restart. Serial letters
are case insensitive. Audio is disabled; high scores are RAM-only.

Pong, Jump Bird and Dino Run use **20 logic updates/second**, about two-thirds
of the previous speed. Pong's paddle moves three pixels per update, retaining
its former approximate travel speed while slowing the ball and AI. HDMI stays
at 30 Hz; no MCU or SPI clock reduction is involved.

The nine patterns are Mandelbrot, Julia 1, Burning Ship, Tricorn, Julia 2,
Julia 3, Sierpinski triangle, Sierpinski carpet and XOR rainbow. Titles are
shown on HDMI, so the optional UART return connection is not needed to know
which pattern is selected.

## Display

Requires SPI2HDMI firmware with `1280x720@30` output (`MODESET=1`). SPI is mode
0, 24 MHz. The menu and four color games use their original **160x80** image,
centered in a **160x90** canvas at 8x (40 output pixels above and below).
Invaders and Conway are **128x64** in that same canvas, centered at (16,13),
so their aspect ratio is preserved. Fractals use **320x180 RGB565** at 4x.
Bad Apple fills the 160x90 canvas at 8x. The 30 Hz HDMI timing is independent of the game's update rate. Fractal
rendering completes progressively; serial input is checked between rows.

## TE synchronization

Connect FPGA TE (pin 39) to PD0/J1-7 and use the FPGA TE-capable bitstream.
Synchronization defaults on. PD0 has an internal pull-down and rising-edge
interrupt; the firmware recognizes recent 30/60 Hz frame pulses and waits for
a fresh rising edge before each batch of game tiles or monochrome frame.
It does not wait separately for every tile or scanline. Input remains serviced
while waiting. Missing TE falls back to immediate writes, a lost signal times
out after at most 40 ms per wait, and returning pulses are recognized again.

The menu shows **TE+** for detected pulses, **TE?** for no valid recent signal,
or **OFF** when disabled. Press **T** to enable, **Y** to disable, in the host
keyboard window or through ASCII UART. No MCU restart is needed. With the
keyboard window closed, the equivalent commands from the repository root are:

```sh
python3 tools/arcade_ch32.py send y   # unsynchronized comparison
python3 tools/arcade_ch32.py send t   # enable TE synchronization again
```

Only debugger TX -> PC1 is needed; the status is visible on HDMI. TE+ confirms
pulse detection, not that every transfer fits blanking. The single framebuffer
and SPI throughput are unchanged. Short updates can benefit; full color
redraws or computed fractals can exceed the roughly 1.333 ms 720p blanking time
and still show partial updates. TE is a frame-start alignment aid, not a second
framebuffer. The ordinary demo remains independent and does not consume TE.

## Wiring

Test board: **CH32V004 QFN20** fitted to the
[wuxx nanoCH32V003 board](https://github.com/wuxx/nanoCH32V003/blob/master/hardware/nanoCH32V003.pdf).
Header positions below were checked against that schematic (high confidence).
They are board header pin numbers, not MCU package pin numbers.

| Board signal | Header | Connect to |
|---|---|---|
| PC5 | J2-3 | module SCK |
| PC6 | J2-4 | module IO0 (MOSI) |
| PC4 | J1-2 | module CS |
| PC3 | J1-3 | module IO1 (DC) |
| PD0, TE input | J1-7 | module TE (3.3 V) |
| PC1, USART1 RX | J1-5 | WCH-Link TX, 3.3 V UART |
| PC0, USART1 TX | J1-6 | Optional WCH-Link RX |
| GND | J1-1 or J2-1 | FPGA and debugger ground |

**Playing needs only debugger TX -> PC1 and common ground. PC0 can remain
unconnected.** UART is 115200 8N1 with no hardware flow control. USART1
uses remap 3 (PC0 TX / PC1 RX), matching
[WCH's remap definitions](https://github.com/openwch/ch32v002_004_005_006_007/blob/main/EVT/EXAM/SRC/Peripheral/inc/ch32v00X_gpio.h).

Future normally-open physical keys connect each signal to GND; firmware
already enables internal pull-ups and 15 ms debounce:

| Key | Signal | Header |
|---|---|---|
| Up | PD2 | J2-6 |
| Down | PD3 | J2-7 |
| Left | PD4 | J2-8 |
| Right | PD5 | J2-9 |
| A | PC2 | J1-4 |
| B / back | PC7 | J2-5 |

PD0/J1-7 is now the TE input. PD1/SWIO, PD7/reset, the PA1/PA2 crystal, and PD6 LED
are retained. Physical buttons work alongside UART control.

## Build and program

Initialize the existing `mcu/ch32fun` repository submodule and install a
RISC-V embedded GCC compiler (tested with xPack GCC 14.2.0-3):

```sh
# From the repository root
git submodule update --init mcu/ch32fun
make -C mcu/ch32/arcade PREFIX=/path/to/bin/riscv-none-elf
```

`TARGET_MCU` defaults to CH32V004; this combined firmware does not fit V003.
The toolchain and programmer are external dependencies. Build minichlink
from the ch32fun submodule following its Linux USB/dependency instructions.
With the WCH-Link in RISC-V mode, SWIO/3.3V/GND connected, program, read back,
then automatically start the MCU:

```sh
# From the repository root; with minichlink on PATH
ARCADE_BIN=mcu/ch32/arcade/arcade.bin
ARCADE_SIZE=$(wc -c < "$ARCADE_BIN")
minichlink -w "$ARCADE_BIN" flash -r /tmp/arcade-readback.bin flash "$ARCADE_SIZE" -b
cmp "$ARCADE_BIN" /tmp/arcade-readback.bin
```

No manual BOOT sequence or RST press is needed with a working SWIO debugger.
Compiler products are ignored. The separate normal demo can be rebuilt and
programmed from `../demo` when desired.

## Serial control from the computer

Install `pyserial` (`python3 -m pip install pyserial`). The graphical keyboard
window additionally uses Python's Tk support (`python3-tk` on Debian/Ubuntu).
From the repository root:

```sh
python3 tools/arcade_ch32.py list
python3 tools/arcade_ch32.py play
python3 tools/arcade_ch32.py send 1       # select Invaders
python3 tools/arcade_ch32.py send j       # start/fire once
python3 tools/arcade_ch32.py hold aj --seconds 2
python3 tools/arcade_ch32.py send q       # menu
```

The keyboard window supports WASD/arrows, J/Space and K, including simultaneous
movement and fire. Click the window to focus it; watch HDMI separately. Losing
focus or closing the window releases keys. A lost connection releases serial
keys after 120 ms. Simple ASCII terminal input also works; disable appended
newlines because CR means the A button.

The script opens only WCH-Link VID:PID **1a86:8010**. It never automatically
opens a CH340/NanoKVM UART. Select `--port /dev/ttyACM1` or `--serial-number ...`
if needed; an explicit path still checks USB identity. No receive connection
or acknowledgement is required. A successful write reports host transmission,
not confirmed MCU execution. With PC0 wired, optional status is:

```sh
python3 tools/arcade_ch32.py listen
```

Protocol: ASCII 1..8 selects entries, 0/Q returns, WASD/J/K pulses a key for
120 ms, X releases serial keys, T enables TE, Y disables TE, ? prints help and
TE status (0=disabled, 1=missing, 2=detected). For exact holds, a single byte
`0x80 | mask` sets all six serial keys; bits 0..5 are A, up, down, right, left,
B. Send every 40 ms while held; `0x80` releases all serial keys.

## Build size (2026-09-08)

CH32V004 / GCC 14.2.0-3 / `-Os -flto`: **32,676 / 32,768 bytes Flash**,
leaving **92 bytes**. Static RAM is **3,144 / 6,144 bytes**; remaining memory
also supplies the runtime stack. The 1800-byte video framebuffer reuses the
same transient allocation as Conway/games.

Video starts at **t=18 s**, resized to 160x90 monochrome and stretched to the
16:9 canvas. The included clip loops **88 frames at 15 fps (5.867 s)** and
occupies **8,878 bytes**. Each frame chooses either a keyframe or an XOR
against its predecessor. Alternating zero/one pixel run lengths use
exponential-Golomb codes with separate per-frame orders for zero and one
runs. The first frame does not depend on data outside the clip.

The bounded C decoder uses the shared framebuffer and a small bit reader,
with no history dictionary. It checks encoded lengths, remaining pixel count,
truncation and final bit padding. This codec is different from the ordinary
demo's PackBits format; use this generator for arcade video:

```sh
# Requires FFmpeg, NumPy and a local source video.
python3 tools/gen_arcade_clip.py /path/to/badapple.mp4 mcu/ch32/arcade/clip_160x90.h --fps 15 --start 18 --budget 8950
make -C mcu/ch32/arcade PREFIX=/path/to/bin/riscv-none-elf
```

Earlier game-only build: 21,852 bytes Flash. The first PackBits video build
fit 46 frames / 3.067 s at 15 fps, 42 / 3.5 s at 12 fps, or 39 / 3.9 s at
10 fps. The new codec fit 100 / 6.667 s at 15 fps before adding the expanded
fractals and game-play changes. These are different builds, not interchangeable
asset budgets; always check the linked image size after changes.

This current image was programmed and read back byte-for-byte through WCH-Link;
SHA256 `08f442f59612404ffcd33c4185672eafbb1d3ea753858f65bd06e65f7617e550`.
The debugger automatically restarted the MCU. The previous game build was
played by the user; visible behavior and decode performance of the current
changes await user feedback.

## Validation and attribution

`sh mcu/ch32/arcade/tests/run.sh` compiles the actual game/controller/renderer
sources on the host with AddressSanitizer and UndefinedBehaviorSanitizer.
It cycles through all eight entries six times, checks SPI window boundaries,
serial control, simultaneous holds/releases, timer wrap and Snake storage
capacity and fresh-key game-over retries. TE tests cover 30/60 Hz detection,
missing/stopped/noisy input, counter wrap, enable/disable and cancellation.
Three application rounds include simulated TE edges and three use fallback. `python3 mcu/ch32/arcade/tests/test_control.py` checks host UART
identity selection and key packets. This does not
emulate peripheral timing or prove the visible board output.

With the local source video, the actual C decoder can be checked against
all source frames, including malformed packet rejection and synthetic data:

```sh
python3 mcu/ch32/arcade/tests/check_video.py /path/to/badapple.mp4 --reference /tmp/arcade-reference.raw
sh mcu/ch32/arcade/tests/run.sh /tmp/arcade-reference.raw
```

The second command also compares the actual player's SPI output with those
reference pixels across repeated video loops.

See [THIRD_PARTY.md](THIRD_PARTY.md) for source revisions, modifications and
licenses. The combined firmware is **GPL-3.0-or-later**.
