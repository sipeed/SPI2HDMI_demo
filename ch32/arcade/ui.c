#include "app.h"
#include "termGFX.h"
static uint8_t selected,last_te=255;
static const char *const names[]={"1 INVADERS","2 SNAKE","3 PONG","4 JUMP BIRD","5 DINO RUN","6 FRACTALS","7 CONWAY","8 BAD APPLE"};
static void menu_draw(void){
    last_te=255;
    tGFX_Clear(0);tGFX_SetCursor(0,0);tGFX_Print("SPI2HDMI ARCADE",11,0);
    for(unsigned i=0;i<8;++i){tGFX_SetCursor(0,i+1);tGFX_Print(i==selected?">":" ",15,0);tGFX_Print((char *)names[i],i==selected?15:7,i==selected?2:0);}
    tGFX_SetCursor(0,9);tGFX_Print("W/S SELECT J PLAY",10,0);
}
static void menu_frame(void){
    unsigned te=te_status();
    if(te!=last_te){last_te=te;tGFX_SetCursor(17,0);tGFX_Print(te==2?"TE+":te==1?"TE?":"OFF",te==2?10:7,0);}
    if(input_take(BTN_UP)){selected=(selected+7)%8;menu_draw();}
    if(input_take(BTN_DOWN)){selected=(selected+1)%8;menu_draw();}
    if(input_take(BTN_ACTION))app_select(selected+1);
}
void app_menu(void){selected=0;last_te=255;menu_draw();app_callback=menu_frame;}
