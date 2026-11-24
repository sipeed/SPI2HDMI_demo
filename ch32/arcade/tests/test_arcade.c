#include "app.h"
#include "host_platform.h"
#include "termGFX.h"
#include "snake.h"
#include "pong.h"
#include "jumpbird.h"
#include "dinorun.h"
#include "gameover.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(snake_game_state_t)<=512,"snake state pool");
_Static_assert(sizeof(pong_game_state_t)<=512,"pong state pool");
_Static_assert(sizeof(jump_bird_game_state_t)<=512,"bird state pool");
_Static_assert(sizeof(dino_run_game_state_t)<=512,"dino state pool");
void app_step(void);
static uint32_t cycles=0xffff0000;
static unsigned tick_poll,invader_polls;
static unsigned scale,fmt,x0,y0,x1,y1,pos,capacity,windows;
static uint8_t stream[320*180*2];
static unsigned frames_by_app[9];
static uint8_t *video_reference;
static unsigned video_frames,video_checked;
static uint32_t te_source_period,te_next_edge;
uint32_t funSysTick32(void){
    cycles+=600;
    if(te_source_period && (int32_t)(cycles-te_next_edge)>=0){
        te_next_edge=cycles+te_source_period;
        te_edge();
    }
    return cycles;
}
void host_uart(char c){(void)c;}
uint8_t host_gpio(void){return 0;}
void host_poll(void){
    ++tick_poll;
    if(app_current==1){
        if(++invader_polls%400==0)host_serial('j');
        if(invader_polls%200==0)host_serial((invader_polls/200)%2?'a':'d');
        if(invader_polls>16000)host_serial('q');
    }
}
void s2h_cmd1(uint8_t c,uint8_t p){if(c==0x88){assert(p<=3);scale=p;}else{assert(c==0x3a);assert(p==0x11||p==0x55);fmt=p;}}
void s2h_tint(uint8_t r,uint8_t g,uint8_t b){assert(r==255&&g==255&&b==255);}
void s2h_window(unsigned a,unsigned b,unsigned c,unsigned d){
    assert(a<=c&&b<=d&&c<(640u>>scale)&&d<(360u>>scale));
    x0=a;y0=b;x1=c;y1=d;
    unsigned bits=fmt==0x55?16:1;
    assert(((x1-x0+1)*bits)%8==0);capacity=(x1-x0+1)*(y1-y0+1)*bits/8;
    assert(capacity<=sizeof(stream));
}
void s2h_cmd(uint8_t c){assert(c==0x2c);pos=0;}
void s2h_byte(uint8_t b){assert(pos<capacity);stream[pos++]=b;cycles+=2;}
void s2h_end(void){
    assert(pos==capacity || app_cancelled());++windows;++frames_by_app[app_current];
    if(video_reference){
        assert(app_current==8&&x0==0&&y0==0&&x1==159&&y1==89&&pos==1800);
        assert(memcmp(stream,video_reference+(video_checked%video_frames)*1800,1800)==0);
        ++video_checked;
    }
}
static void advance(unsigned n){for(unsigned i=0;i<n;++i){cycles+=200000;app_step();}}
int main(int argc,char **argv){
    /* Unconnected PD0 never waits. Acquire a 30 Hz source across counter
     * wrap, then lose it while waiting. Both stuck levels are edge-free. */
    uint32_t before=cycles;
    assert(te_status()==1 && display_sync());
    assert(cycles-before<6000);
    te_source_period=200000;te_next_edge=cycles+200000;
    while(te_status()!=2)funSysTick32();
    before=cycles;assert(display_sync());
    assert(cycles-before>=190000 && cycles-before<240000);
    te_source_period=0;before=cycles;assert(display_sync());
    assert(cycles-before>=240000 && cycles-before<250000);
    cycles+=600000;before=cycles;assert(te_status()==1&&display_sync());
    assert(cycles-before<6000);
    /* User can explicitly bypass or re-enable sync; implausibly rapid
     * pulses do not become a valid frame source. */
    host_serial('y');input_poll();assert(te_status()==0);
    host_serial('T');input_poll();assert(te_status()==1);
    te_source_period=12000;te_next_edge=cycles;
    for(unsigned i=0;i<200;++i)funSysTick32();
    assert(te_status()==1);
    te_source_period=100000;te_next_edge=cycles+100000;
    while(te_status()!=2)funSysTick32();
    before=cycles;assert(display_sync());assert(cycles-before<110000);
    /* A menu request interrupts a pending TE wait immediately. */
    host_serial('q');before=cycles;assert(!display_sync());assert(cycles-before<6000);
    te_source_period=0;
    puts("PASS TE absent/acquire 30+60Hz/wrap/stopped/noise/on-off/cancel");
    app_select(0);advance(3);assert(app_current==0);
    /* Actual parser: single-byte selection, case folding, release, invalid key. */
    host_serial('W');input_poll();assert(BTN_IsPressed(BTN_UP));host_serial('x');input_poll();assert(!BTN_IsPressed(BTN_UP));
    /* Exact held-state packets: simultaneous movement/fire, independent
     * release, and automatic release after a disconnected host. */
    host_serial(0x80|(1<<BTN_ACTION)|(1<<BTN_LEFT));input_poll();
    assert(BTN_IsPressed(BTN_ACTION)&&BTN_IsPressed(BTN_LEFT));
    assert(input_take(BTN_ACTION));
    host_serial(0x80|(1<<BTN_ACTION));input_poll();
    assert(BTN_IsPressed(BTN_ACTION)&&!BTN_IsPressed(BTN_LEFT));
    assert(!input_take(BTN_ACTION));
    cycles+=6000*121;input_poll();assert(!BTN_IsPressed(BTN_ACTION));
    for(unsigned round=0;round<6;++round){
        te_source_period=(round&1)?200000:0;te_next_edge=cycles+200000;
        for(unsigned id=1;id<=8;++id){
            invader_polls=0;host_serial('0'+id);advance(1);
            if(id==1){advance(2);assert(app_current==0);}
            else{
                assert(app_current==id);
                for(unsigned f=0;f<450;++f){
                    if(f%9==0)host_serial("wasdj"[(f/9+round)%5]);
                    if(id>=6 && f%100==0)host_serial('j');
                    advance(1);
                }
                host_serial('k');advance(2);assert(app_current==0);
            }
            assert(frames_by_app[id]>0);
        }
    }
    /* Bad cursor/sprite inputs must not write outside term arrays. */
    tGFX_SetCursor(20,10);tGFX_Print("LONG STRING THAT WRAPS THE TERMINAL",15,0);
    tGFX_SetChar('X',255,255,15,0);tGFX_SetSpritesCount(255);tGFX_ClearSprite(255);
    app_select(2);advance(1);
    snake_game_state_t *snake=(void *)app_memory.raw;
    snake->tail_len=128;snake->food_x=*snake->snake_pos_x;snake->food_y=*snake->snake_pos_y;
    SNAKE_Main();assert(snake->game_over);
    /* Continue retries the current game, with a fresh key after game-over.
     * A held flap/jump must not immediately dismiss the result screen. */
    for(unsigned id=2;id<=5;++id){
        app_select(id);advance(1);void (*playing)(void)=app_callback;
        host_serial(0x81);input_poll();
        GAMEOVER_Setup(GAMEOVER_TYPE_SCORE,123,456,id>=4?BTN_UP:BTN_ACTION,NULL);
        GAMEOVER_Show();void (*over)(void)=app_callback;
        app_callback();app_callback();assert(app_callback==over);
        host_serial(0x80);input_poll();app_callback();
        host_serial(id>=4?'w':'j');input_poll();app_callback();advance(1);
        assert(app_current==id&&app_callback==playing);
        host_serial('q');advance(1);assert(app_current==0);
    }
    if(argc>1){
        app_select(8);advance(1); /* Clear screen before checking video data. */
        FILE *file=fopen(argv[1],"rb");assert(file);
        assert(fseek(file,0,SEEK_END)==0);long size=ftell(file);rewind(file);
        assert(size>0&&size%1800==0);video_frames=size/1800;
        video_reference=malloc(size);assert(video_reference);
        assert(fread(video_reference,1,size,file)==(size_t)size);fclose(file);
        advance(video_frames*8+100);assert(video_checked>=video_frames*3);
        free(video_reference);video_reference=NULL;
        printf("PASS actual video SPI pixels vs source: %u frames including loops\n",video_checked);
    }
    printf("PASS six rounds of all eight apps; serial selection/back; %u SPI windows; timer wrap\n",windows);
}
