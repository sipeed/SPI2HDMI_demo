/* Procedural patterns: no additional bitmap assets. GPL-3.0-or-later. */
#include "app.h"
#include "fractal_tables.h"
#include "termGFX.h"
#include <string.h>
static uint8_t style,redraw;
static const char *const names[]={"MANDELBROT","JULIA 1","BURNING SHIP","TRICORN",
    "JULIA 2","JULIA 3","SIERPINSKI TRIANGLE","SIERPINSKI CARPET","XOR RAINBOW"};
#define STYLE_COUNT (sizeof(names)/sizeof(names[0]))
static void pixel(uint16_t p){display_byte(p>>8);display_byte(p);}
static uint16_t pattern(int x,int y) {
    if(style==8){unsigned v=((x<<1)^(y<<1))&255,h=((x+y)>>3)&63;
        unsigned r=(xor_r[h]*v)>>8,g=(xor_g[h]*v)>>8,b=(xor_b[h]*v)>>8;
        return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
    }
    if(style==6){
        int row=y-23,dx=x-(160-row);
        return row>=0&&row<128&&dx>=0&&dx<=2*row&&(((dx/2)&row)==dx/2)?
               mandel_pal[(row+dx)&63]:0;
    }
    if(style==7){
        int a=x-79,b=y-9;if(a<0||a>=162||b<0||b>=162)return 0;
        while(a||b){if(a%3==1&&b%3==1)return 0;a/=3;b/=3;}
        return julia_pal[(x+y)%48];
    }
    int julia=(style==1||style==4||style==5);
    int32_t zx,zy,kx,ky;
    if(julia){
        zx=(-14746+x*92)>>2;zy=(-8280+y*92)>>2;
        kx=style==1?-1638:style==4?-819:-1437;
        ky=style==1?319:style==4?1229:-787;
    }else{
        zx=zy=0;
        if(style==2){kx=-4096+x*22;ky=-3072+y*22;}
        else if(style==3){kx=-4096+x*30;ky=-2700+y*30;}
        else{kx=(-20480+x*82)>>2;ky=(-7380+y*82)>>2;}
    }
    unsigned it=0;
    for(;it<48;++it){
        int32_t x2=(zx*zx)>>11,y2=(zy*zy)>>11;
        if(x2+y2>(4<<11))break;
        if(style==2){if(zx<0)zx=-zx;if(zy<0)zy=-zy;}
        int32_t xy=(zx*zy)>>10;
        zy=(style==3?-xy:xy)+ky;zx=x2-y2+kx;
    }
    return julia?(it==48?JULIA_INSIDE:julia_pal[it]):
                  (it==48?MANDEL_INSIDE:mandel_pal[mandel_idx[it]]);
}
/* Full 16:9 canvas, Q11 escape arithmetic; titles share the game font. */
static void draw(void){
    if(!display_sync())return;
    const char *name=names[style];unsigned title_width=strlen(name)*8;
    display_window(0,0,320,180);
    for(int y=0;y<180;++y){
        for(int x=0;x<320;++x){
            if(y<8)pixel((unsigned)x<title_width &&
               ((term_char_map[(uint8_t)name[x/8]][y]>>(7-(x&7)))&1)?0xffff:0);
            else pixel(pattern(x,y));
        }
        if(app_cancelled()){display_end();return;}
    }
    display_end();uart_puts(name);uart_puts(" / J next / Q menu\r\n");
}
static void frame(void){if(input_take(BTN_ACTION)){style=(style+1)%STYLE_COUNT;redraw=1;}if(redraw){redraw=0;draw();}}
void fractal_enter(void){style=0;redraw=1;display_layout(1,0x55);display_clear(320,180,0x55);app_callback=frame;}
