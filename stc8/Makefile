# STC8G1K17A demos: palette animation (palette.hex) and arcade (dino + CHIP-8, arcade.hex).  Needs SDCC (mcs51).  make / make test / make flash PORT=/dev/ttyUSB0
SDCC  ?= sdcc
FOSC  ?= 24000000
CFLAGS = -mmcs51 --model-small --opt-code-size --std-sdcc99 -DFOSC=$(FOSC)UL --code-size 17408 --xram-size 1024 --iram-size 256
OBJ    = build/main.rel build/gfx.rel build/dino.rel build/chip8.rel build/stc8g.rel

ROM_BYTES ?=
all: build/palette.hex build/arcade.hex
assets.h: gen_assets.py
	python3 gen_assets.py
roms.h: gen_roms.py roms/manifest.txt
	python3 gen_roms.py $(ROM_BYTES)
build/%.rel: %.c hal.h gfx.h assets.h roms.h
	@mkdir -p build
	$(SDCC) $(CFLAGS) -c $< -o $@
paltab.h: gen_paltab.py
	python3 gen_paltab.py
build/palette.rel: palette.c hal.h assets.h paltab.h
	@mkdir -p build
	$(SDCC) $(CFLAGS) -c $< -o $@
build/palette.ihx: build/palette.rel build/stc8g.rel
	$(SDCC) $(CFLAGS) $^ -o $@
build/palette.hex: build/palette.ihx
	packihx $< > $@ 2>/dev/null
	@tail -5 build/palette.mem
build/arcade.ihx: $(OBJ)
	$(SDCC) $(CFLAGS) $(OBJ) -o $@
build/arcade.hex: build/arcade.ihx
	packihx $< > $@ 2>/dev/null
	@grep -A12 'Stack starts' build/arcade.mem | head -3; tail -6 build/arcade.mem
test:
	sh tests/run.sh
HEX ?= build/palette.hex
flash: $(HEX)
	-stty -F $(PORT) 115200 raw && for c in @ S T C I S P '#'; do printf '%s' "$$c" > $(PORT); sleep 0.03; done   # ask a running image to enter the download program; one character per firmware tick, it polls the UART once per 1/60 s
	stcgal -P stc8g -t $$(( $(FOSC) / 1000 )) -p $(PORT) $(HEX)
clean:
	rm -rf build
