/* SPI2HDMI arcade platform, CH32V004. GPL-3.0-or-later. */
#ifdef ARCADE_HOST
#include "host_platform.h"
#else
#include "ch32fun.h"
#endif
#include "app.h"
#ifndef ARCADE_HOST
#include "s2h_ch32.h"
#endif
#include "highscore.h"
#include <string.h>

AppMemory app_memory;
uint32_t display_bytes;
static uint8_t rx[64];
static volatile uint8_t rx_head,rx_tail;
static uint32_t held_until[BTN_COUNT];
static uint8_t down_mask,edge_mask,physical_mask,raw_previous;
static uint32_t gpio_changed;
static uint16_t common_pos;
static uint32_t scores[HIGHSCORE_DATA_ID_COUNT];
static unsigned tiny_offset;

uint32_t TICK_Get(void) {
    static uint32_t last_cycles,milliseconds;
    uint32_t elapsed=funSysTick32()-last_cycles;
    if(elapsed>=DELAY_MS_TIME){uint32_t ms=elapsed/DELAY_MS_TIME;milliseconds+=ms;last_cycles+=ms*DELAY_MS_TIME;}
    return milliseconds;
}
#ifndef ARCADE_HOST
void USART1_IRQHandler(void) __attribute__((interrupt));
void USART1_IRQHandler(void) {
    uint16_t sr=USART1->STATR;
    if(sr & (USART_STATR_RXNE | USART_STATR_ORE)) {
        uint8_t c=USART1->DATAR,n=(rx_head+1)&63;
        if(n!=rx_tail) {rx[rx_head]=c;rx_head=n;}
    }
}
static void uart_char(char c) {while(!(USART1->STATR&USART_STATR_TXE)); USART1->DATAR=c;}
#else
static void uart_char(char c){host_uart(c);}
#endif
void uart_puts(const char *s) {while(*s) uart_char(*s++);}
void uart_number(uint32_t n) {char b[11];unsigned i=0;do {b[i++]='0'+n%10;n/=10;}while(n);while(i)uart_char(b[--i]);}
char *itoa(int v,char *out,int base) {
    (void)base; char b[11];unsigned i=0;char *p=out;
    uint32_t n=v; if(v<0){*p++='-';n=0u-(uint32_t)v;}
    do{b[i++]='0'+n%10;n/=10;}while(n);while(i)*p++=b[--i];*p=0;return out;
}
#ifndef ARCADE_HOST
/* The test board has an active-high LED on PD6.  A 10 kHz SysTick interrupt
 * drives pulse-density modulation so it keeps breathing while the foreground
 * streams a frame over SPI. */
#define LED_PIN PD6
static volatile uint8_t led_level;

static void led_breathe_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;
    GPIOD->CFGLR &= ~(0xFu << (4 * 6));
    GPIOD->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4 * 6);
    GPIOD->BSHR = 1u << (16 + 6);

    /* SysTick stays on HCLK/8, the same time base used by Delay_Ms. */
    SysTick->CTLR = 0;
    SysTick->CMP = DELAY_US_TIME * 100 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE;
    NVIC_EnableIRQ(SysTick_IRQn);
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    static uint8_t pdm_acc;
    static uint8_t envelope;
    static uint8_t envelope_div;
    static int8_t direction = 1;

    SysTick->CMP += DELAY_US_TIME * 100;
    SysTick->SR = 0;

    uint8_t previous = pdm_acc;
    pdm_acc += led_level;
    GPIOD->BSHR = (pdm_acc < previous) ? (1u << 6) : (1u << (16 + 6));

    /* One envelope step per 5 ms: about 1.28 s up and 1.28 s down.
     * Squaring the ramp better matches perceived LED brightness. */
    if (++envelope_div == 50) {
        envelope_div = 0;
        if (direction > 0) {
            if (++envelope == 255) direction = -1;
        } else if (envelope == 0) {
            direction = 1;
            envelope = 1;
        } else {
            --envelope;
        }
        led_level = ((uint16_t)envelope * envelope + 255) >> 8;
    }
}

void board_init(void) {
    SystemInit();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                       RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1;
    /* USART1 remap 3: PC0 TX, PC1 RX. Hardware flow control disabled. */
    AFIO->PCFR1=(AFIO->PCFR1 & ~AFIO_PCFR1_USART1_RM) | (3u<<6);
    funPinMode(PC0,GPIO_CFGLR_OUT_10Mhz_AF_PP);
    funPinMode(PC1,GPIO_CFGLR_IN_PUPD); funDigitalWrite(PC1,1);
    USART1->BRR=(FUNCONF_SYSTEM_CORE_CLOCK+57600)/115200;
    USART1->CTLR2=0; USART1->CTLR3=0;
    USART1->CTLR1=USART_CTLR1_UE|USART_CTLR1_TE|USART_CTLR1_RE|USART_CTLR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);
    /* Six normally-open keys to ground, internal pull-ups. */
    const uint8_t pins[]={PC2,PD2,PD3,PD5,PD4,PC7};
    for(unsigned i=0;i<6;++i){funPinMode(pins[i],GPIO_CFGLR_IN_PUPD);funDigitalWrite(pins[i],1);}
    led_breathe_init();
    te_init();
    s2h_init();
    s2h_cmd1(S2H_MODESET,1); /* 720p30; source 160x90 at 8x. */
    Delay_Ms(20);
}
static uint8_t gpio_keys(void) {
    return ((!funDigitalRead(PC2))<<BTN_ACTION) | ((!funDigitalRead(PD2))<<BTN_UP) |
      ((!funDigitalRead(PD3))<<BTN_DOWN) | ((!funDigitalRead(PD5))<<BTN_RIGHT) |
      ((!funDigitalRead(PD4))<<BTN_LEFT) | ((!funDigitalRead(PC7))<<BTN_BACK);
}
#else
static uint8_t gpio_keys(void){return host_gpio();}
#endif
static int serial_key(uint8_t c) {
    if(c>='A'&&c<='Z')c+='a'-'A';
    switch(c) {case 'w':return BTN_UP;case 's':return BTN_DOWN;case 'a':return BTN_LEFT;
    case 'd':return BTN_RIGHT;case 'j':case ' ':case '\r':return BTN_ACTION;
    case 'k':return BTN_BACK;default:return -1;}
}
void input_clear(void){memset(held_until,0,sizeof(held_until));down_mask=edge_mask=0;}
#ifdef ARCADE_HOST
void host_serial(uint8_t c){uint8_t n=(rx_head+1)&63;if(n!=rx_tail){rx[rx_head]=c;rx_head=n;}}
#endif
void input_poll(void) {
#ifdef ARCADE_HOST
    host_poll();
#endif
    uint32_t now=TICK_Get();
    uint8_t g=gpio_keys();
    if(g!=raw_previous){raw_previous=g;gpio_changed=now;}
    if((uint32_t)(now-gpio_changed)>=15)physical_mask=g;
    uint8_t m=physical_mask;
    for(unsigned i=0;i<BTN_COUNT;++i) if((int32_t)(held_until[i]-now)>0)m|=1u<<i;
    edge_mask|=m&~down_mask;down_mask=m;
    while(rx_tail!=rx_head) {
        uint8_t c=rx[rx_tail];rx_tail=(rx_tail+1)&63;
        /* Optional host keyboard heartbeat: 0x80 | six-button mask.
         * An explicit release preserves other held keys; a lost host releases
         * all serial keys after 120 ms. Physical keys remain independent. */
        if((c&0xc0)==0x80){
            uint8_t mask=(c&0x3f)|physical_mask;
            for(unsigned i=0;i<BTN_COUNT;++i)held_until[i]=(c&(1u<<i))?now+120:0;
            edge_mask|=mask&~down_mask;down_mask=mask;continue;
        }
        int b=serial_key(c);
        if(b>=0){held_until[b]=now+120;edge_mask|=(1u<<b)&~down_mask;down_mask|=1u<<b;}
        else if(c=='x'||c=='X')input_clear();
        else if(c=='q'||c=='Q'||c=='0')app_select(0);
        else if(c>='1'&&c<='8')app_select(c-'0');
        else if(c=='t'||c=='T')te_enabled=1;
        else if(c=='y'||c=='Y')te_enabled=0;
        else if(c=='?'){uart_puts("S2HARCADE1 app=");uart_number(app_current);uart_puts(" TE=");uart_number(te_status());uart_puts("\r\n1 Invaders 2 Snake 3 Pong 4 Bird 5 Dino 6 Fractal 7 Conway 8 BadApple\r\nWASD move J/SPACE=A K=B Q=menu X=release T=TE Y=free\r\n");}
    }
    if(edge_mask&(1u<<BTN_BACK)){edge_mask&=~(1u<<BTN_BACK);app_select(0);}
}
uint8_t BTN_IsPressed(buttons_names_t b){input_poll();return (down_mask>>b)&1;}
uint8_t BTN_IsPressedRAW(buttons_names_t b){return BTN_IsPressed(b);}
uint8_t input_take(buttons_names_t b){uint8_t v=edge_mask&(1u<<b);edge_mask&=~(1u<<b);return !!v;}
void app_delay(uint32_t ms){uint32_t t=TICK_Get();while((uint32_t)(TICK_Get()-t)<ms&&!app_cancelled())input_poll();}

void display_layout(uint8_t scale,uint8_t fmt) {
    s2h_cmd1(S2H_SCALESET,scale);s2h_cmd1(S2H_COLMOD,fmt);s2h_tint(255,255,255);
}
void display_window(unsigned x,unsigned y,unsigned w,unsigned h){s2h_window(x,y,x+w-1,y+h-1);s2h_cmd(S2H_RAMWR);}
void display_byte(uint8_t b){s2h_byte(b);++display_bytes;}
void display_end(void){s2h_end();}
void display_clear(unsigned w,unsigned h,uint8_t fmt){
    if(!display_sync())return;
    display_window(0,0,w,h);unsigned row=fmt==0x55?w*2:fmt==0x11?w/8:w;
    for(unsigned y=0;y<h;++y){for(unsigned x=0;x<row;++x)display_byte(0);input_poll();}
    display_end();
}
void LCD_SetWindow(uint16_t x,uint16_t y,uint16_t w,uint16_t h){display_window(x,y+5,w,h);}
void LCD_WriteRawData(uint16_t *p,uint16_t n){while(n--){display_byte(*p>>8);display_byte(*p++);}display_end();input_poll();}
void LCD_WriteRawDataDMA(uint16_t *p,uint16_t n){LCD_WriteRawData(p,n);}
void LCD_DMA_Wait(void){}
void LCD_OptimizedFill(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){LCD_SetWindow(x,y,w,h);for(unsigned n=(unsigned)w*h;n;--n){display_byte(c>>8);display_byte(c);}display_end();}
void ENG_SetInFrameCallback(void (*f)(void)){app_callback=f;}
void ENG_UpdateInFrameCallback(void (*f)(void)){app_callback=f;}
void ENG_SetOutFrameCallback(void (*f)(void)){(void)f;}
void ENG_SetFPS(uint8_t fps){app_frame_ms=fps?1000/fps:33;}
void ENG_ClearCommonBuf(void){memset(app_memory.raw,0,512);common_pos=0;}
void *ENG_CommonBufAlloc(uint16_t n){n=(n+3)&~3u;if(common_pos+n>512)return NULL;void *p=app_memory.raw+common_pos;common_pos+=n;return p;}
uint16_t ENG_CommonBufGetFreeBytes(void){return 512-common_pos;}
uint32_t HIGHSCORE_Read(highscore_data_id_t id){return id<HIGHSCORE_DATA_ID_COUNT?scores[id]:0;}
void HIGHSCORE_Write(highscore_data_id_t id,uint32_t n){if(id<HIGHSCORE_DATA_ID_COUNT)scores[id]=n;}
void tiny_page(unsigned y){tiny_offset=y*128;}
void tiny_byte(uint8_t b){if(tiny_offset<1024)app_memory.raw[tiny_offset++]=b;}
void tiny_flush(void) {
    if(tiny_offset!=1024)return;
    if(!display_sync())return;
    display_window(16,13,128,64);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;x+=8){
        uint8_t b=0;for(unsigned k=0;k<8;++k)b|=((app_memory.raw[(y/8)*128+x+k]>>(y&7))&1)<<(7-k);
        display_byte(b);
    }
    display_end();input_poll();
}
