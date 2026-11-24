/* Independent CH32V004 SPI2HDMI arcade. GPL-3.0-or-later. */
#include "app.h"
#include "termGFX.h"
#include <string.h>

void (*app_callback)(void);
uint8_t app_current;
uint16_t app_frame_ms=33;
static int pending=-1;
static uint8_t cancel;
static uint32_t next_frame;

void app_select(unsigned id){if(id<=8){pending=id;cancel=1;}}
int app_cancelled(void){input_poll();return cancel;}
void app_service(void){input_poll();}
void MENU_MainMenuProcess(void){app_select(0);}
void MENU_ForceBTNNotReleased(void){input_clear();}

static void activate(unsigned id) {
    app_current=id;app_callback=0;app_frame_ms=33;
    input_clear();tGFX_Reset();memset(&app_memory,0,sizeof(app_memory));
    uart_puts("APP ");uart_number(id);uart_puts("\r\n");
    if(id==1) {
        display_layout(2,0x11);display_clear(160,90,0x11);
        invaders_run();
        if(pending<0)app_select(0);
    } else if(id==6)fractal_enter();
    else if(id==7)life_enter();
    else if(id==8)video_enter();
    else {
        display_layout(2,0x55);display_clear(160,90,0x55);
        extern void SNAKE_Init(void),PONG_Init(void),JUMPBIRD_Init(void),DINORUN_Init(void);
        if(id==2)SNAKE_Init();
        else if(id==3)PONG_Init();
        else if(id==4)JUMPBIRD_Init();
        else if(id==5)DINORUN_Init();
        else app_menu();
        /* These upstream games advance physics per update. Give keyboard
         * players 50% more reaction time, while HDMI remains 720p30. */
        if(id>=3&&id<=5)app_frame_ms=50;
    }
    next_frame=TICK_Get();
}
void app_step(void) {
    input_poll();
    if(input_take(BTN_BACK))app_select(0);
    if(pending>=0){unsigned id=pending;pending=-1;cancel=0;activate(id);return;}
    uint32_t now=TICK_Get();
    if((int32_t)(now-next_frame)>=0){
        next_frame=now+app_frame_ms;
        if(app_callback)app_callback();
        if(!cancel && app_current<=5 && app_current!=1)tGFX_Update();
    }
}
#ifndef ARCADE_HOST
int main(void) {
    board_init();uart_puts("\r\nS2HARCADE1 115200 720p30\r\n? help / 1-8 select / q menu\r\n");
    app_select(0);
    for(;;)app_step();
}
#endif
