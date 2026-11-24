# CH32V003/V004 examples

The separate [CH32V004 arcade](arcade/README.md) combines Tiny Invaders, four
MiniGameConsole games, fractals, Conway and Bad Apple in a UART/physical-key
menu. It uses 720p output, supports TE on PD0 and defaults to V004; the demo
described below remains independent.

These examples use [ch32fun](https://github.com/cnlohr/ch32fun), checked out at `../ch32fun` as a git submodule. Only this project's source files and generated frame data are kept here; compiler outputs are ignored.

The default target is CH32V003. Set `TARGET_MCU=CH32V004` for the larger flash/RAM variant. The V004 SPI1 pin mapping must be checked against the package and board being used.

## Toolchain

Install a RISC-V embedded GCC toolchain and a WCH-compatible programmer such as `minichlink`. The Makefiles use `riscv-none-elf-gcc` by default; set `PREFIX` if the executable has a different prefix.

```sh
git submodule update --init --recursive
export PATH=/path/to/riscv-none-elf/bin:$PATH
```

## Wiring used by the V003 example

| CH32 pin | Function | SPI2HDMI module |
|---|---|---|
| PC5 | SPI1 SCK | SCK |
| PC6 | SPI1 MOSI | IO0 (MOSI) |
| PC4 | CS | CS |
| PC3 | D/C | IO1 (DC) |
| GND | ground | board GND |

The examples use SPI mode 0 at 24 MHz on both V003 and V004. On the V004 test
board, an active-high LED on PD6 breathes throughout the demo.

The shared driver defines both output modes and all scale values. Send `S2H_MODESET` with `S2H_MODE_1280X720`; 720p starts at 2×, so use `S2H_SCALE_720P_2X` through `S2H_SCALE_720P_16X` to select source sizes from 640×360 down to 80×45. The `S2H_SCALE_1X` through `S2H_SCALE_16X` names are the VGA codes.

## Build

```sh
cd ch32/demo
make                         # CH32V003
make clean                   # clean before switching MCU targets
make TARGET_MCU=CH32V004
make flash TARGET_MCU=CH32V004
```

The one `demo/` project builds for either MCU. It cycles through an embedded
Bad Apple clip, Conway patterns, and fixed-point fractals. The `clip_*.h` files
and `fractal_tables.h` are source data compiled into flash. Rebuild outputs
(`.bin`, `.elf`, `.hex`, `.map`, `.lst`) are ignored.

The VGA demo sequence is Bad Apple → three Conway patterns → Mandelbrot →
Julia → XOR rainbow, then repeat. Both fractals compute 320×240 RGB565 and use
2× pixel replication. V004 plays 79 frames of 160×120 monochrome Bad Apple at
15 fps (4×); V003 plays 108 frames at 80×60 (8×). The decoder checks packet and
frame boundaries. Regenerated clips have been compared against every source
video frame. Julia has fine, high-contrast edges; it uses the same pixel count,
Q11 arithmetic and 48-iteration limit as Mandelbrot.

To regenerate the RGB565 palettes, run `python3 tools/gen_tables.py` from the
repository root. It requires only Python's standard library. `tools/gen_clip.py`
can regenerate video headers from a local source video using FFmpeg and NumPy.
