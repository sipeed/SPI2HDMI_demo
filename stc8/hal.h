/* Board interface.  stc8g.c implements it for the STC8G1K17A; tests/host.c for a PC. */
#ifndef HAL_H
#define HAL_H
#include <stdint.h>
#ifndef __SDCC
#define __code
#define __xdata
#define __idata
#endif
void hal_init(void);
void hal_cmd(uint8_t c);      /* start a command: CS low, D/C low, send c, D/C high (CS stays low) */
void hal_data(uint8_t b);     /* one parameter / pixel byte of the running command */
void hal_end(void);           /* CS high */
uint8_t hal_key(void);        /* next received serial character, 0 if none */
void hal_wait_tick(void);     /* block until the next 1/60 s tick */
uint8_t hal_tick_poll(void);  /* 1 once per elapsed tick, without waiting */
#define KEY_ESC 27
#endif
