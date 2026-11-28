/* Board stand-in for cycle counting in the uCsim 8051 simulator: SPI bytes are written to the data
 * register without waiting, ticks never elapse.  The SPI wait time is added separately. */
#include "../hal.h"
__sfr __at (0xCF) SPDAT;
void hal_init(void) {}
void hal_cmd(uint8_t c)  { SPDAT = c; }
void hal_data(uint8_t b) { SPDAT = b; }
void hal_end(void) {}
uint8_t hal_key(void) { return 0; }
void hal_wait_tick(void) {}
uint8_t hal_tick_poll(void) { return 0; }
