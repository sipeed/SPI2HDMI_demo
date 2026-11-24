#pragma once
#include "app.h"
#include <stdlib.h>
#define JOY_init() ((void)0)
#define JOY_act_pressed() BTN_IsPressed(BTN_ACTION)
#define JOY_left_pressed() BTN_IsPressed(BTN_LEFT)
#define JOY_right_pressed() BTN_IsPressed(BTN_RIGHT)
#define JOY_DLY_ms(ms) app_delay(ms)
#define JOY_SLOWDOWN() app_delay(20)
static inline void JOY_sound(uint8_t f,uint8_t d) {(void)f;(void)d;}
static inline uint16_t JOY_random(void) {static uint16_t s=0xace1; s=(s>>1)^(-(s&1)&0xb400);return s;}
#define JOY_OLED_data_start(y) tiny_page(y)
#define JOY_OLED_send(b) tiny_byte(b)
#define JOY_OLED_end() tiny_flush()
