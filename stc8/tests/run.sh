#!/bin/sh
# Builds the firmware for the PC and renders frames to build/frame_*.pgm.
#   tests/run.sh                      scripted tour through the menu, the dino game and two CHIP-8 programs
#   tests/run.sh rom FILE QUIRKS RATE TICKS SCRIPT DUMP...     run one CHIP-8 file
set -e
cd "$(dirname "$0")/.."
mkdir -p build
for f in main gfx dino chip8; do gcc -O1 -Wall -Dmain=game_main -c $f.c -o build/host_$f.o; done
gcc -O1 -Wall -c tests/host.c -o build/host_hal.o
gcc -o build/host_arcade build/host_main.o build/host_gfx.o build/host_dino.o build/host_chip8.o build/host_hal.o
rm -f build/frame_*.pgm
if [ "$1" = rom ]; then shift; exec ./build/host_arcade rom "$@"; fi
S=$(python3 - <<'PY'
s = ['.'] * 2400
def key(t, c): s[t] = c
key(60, '_')                      # start DINO RUN
key(100, '_'); key(160, '_'); key(215, '_'); key(280, '_')
key(600, '^')                     # back to the menu
key(640, 's'); key(650, '_')      # BR8KOUT
for t in range(700, 1100, 12): key(t, 'a' if (t // 120) % 2 else 'd')
key(1150, '^')
key(1180, 's'); key(1190, 's'); key(1200, 's'); key(1210, 's'); key(1220, '_')   # 5th entry
for t in range(1260, 1700, 20): key(t, 'w')
key(1750, '^')
key(1780, 'd'); key(1800, '_')    # second column
key(2300, '^')
print(''.join(s))
PY
)
./build/host_arcade menu 2400 "$S" 50 130 300 630 660 800 1000 1140 1215 1300 1600 1795 1900 2250 2390
