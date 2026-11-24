# Source and licenses

This independent firmware is distributed under **GPL-3.0-or-later**; see
[COPYING](COPYING). The combined firmware includes Tiny Invaders GPL code.
Files imported from MiniGameConsole retain their Apache-2.0 license and
original author headers. No upstream SDK, compiler or build products
are bundled in this directory.

- **Tiny Invaders**: Daniel C. (2018), CH32V003 conversion by Stefan Wagner.
  Source: https://github.com/wagiminator/CH32V003-GameConsole
  Revision: `ad10df94a7571768d658b03238379f99f0a3f061`.
  Imported `software/tiny_invaders/src/main.c` as `vendor/tiny/invaders.c`,
  and `spritebank.h`. GPL-3.0-or-later. Changes: rename standalone main to
  `invaders_run`, allow cancellation in its loops, replace board/OLED/input
  through `compat/driver.h`. Convert the 128x64 OLED page format to SPI2HDMI
  row format. Sound is a no-op.
- **CH32V003 MiniGameConsole**: max7d3 / max.
  Source: https://github.com/max7d3/CH32V003-MiniGameConsole
  Revision: `edf73145d6e4cf39626b2fc60adab37fad4f8f0f`.
  Apache-2.0; license at [vendor/mini/LICENSE](vendor/mini/LICENSE).
  Imported Snake, Pong, Jump Bird, Dino Run, game-over handling, terminal
  renderer/font and RNG. Changes: replace board/engine/keys/display/tick/
  audio/high-score interfaces; add terminal/sprite bounds guards and renderer
  reset; enlarge game-over number buffer; bound Snake growth at its allocated
  segment capacity; game-over retries the current game on Up/J; slow three
  games to 20 updates/second and adjust the player paddle; enlarge Bird score
  text storage. High scores stay in RAM instead of
  writing the original 16 KiB target's flash address. Original game art and
  author comments remain in the selected source files.
- **ch32fun**: external repository submodule, not copied into this project.
  https://github.com/cnlohr/ch32fun — see that submodule's license files.

The menu, integration platform, fractal/Conway adaptation and host checks in
this directory are supplied under GPL-3.0-or-later. New integration does not
change the licenses of separately imported files.

The generated `clip_160x90.h` contains a short monochrome excerpt of the
Bad Apple video supplied for this project's existing demo. Its artwork is
third-party media, not original integration code; it is not relicensed by
the GPL declaration for the player. The full source video is not bundled.
