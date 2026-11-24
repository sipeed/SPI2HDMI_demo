#pragma once
#include <stdint.h>
/* No speaker on this board; preserve the game API without claiming audio. */
static inline void SND_PlayNow(uint16_t f,uint8_t t) {(void)f;(void)t;}
static inline void SND_PlaySequence(uint16_t *p,uint8_t n) {(void)p;(void)n;}
static inline void SND_Mute(void) {}
static inline void SND_Unmute(void) {}
