/* TE on PD0: synchronize one batch of writes, never each tile/scanline.
 * GPL-3.0-or-later. Missing/stopped TE must not freeze the application. */
#ifdef ARCADE_HOST
#include "host_platform.h"
#else
#include "ch32fun.h"
#endif
#include "app.h"

/* Kept in RAM for SWIO inspection even when PC0/UART TX is not connected.
 * Time fields use the 6 MHz SysTick counter and unsigned wrap arithmetic. */
volatile struct { uint32_t edges,last,period,waits,timeouts; } te_stats;
uint8_t te_enabled=1;

void te_edge(void) {
    uint32_t now=funSysTick32();
    te_stats.period=now-te_stats.last;
    te_stats.last=now;
    ++te_stats.edges;
}
#ifndef ARCADE_HOST
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void) {
    if(EXTI->INTFR&1){EXTI->INTFR=1;te_edge();}
}
void te_init(void) {
    funPinMode(PD0,GPIO_CFGLR_IN_PUPD);funDigitalWrite(PD0,0);
    AFIO->EXTICR=(AFIO->EXTICR&~AFIO_EXTICR_EXTI0)|AFIO_EXTICR_EXTI0_PD;
    EXTI->FTENR&=~1u;EXTI->RTENR|=1;EXTI->INTFR=1;EXTI->INTENR|=1;
    NVIC_EnableIRQ(EXTI7_0_IRQn);
}
#endif
/* 0=disabled, 1=waiting/missing, 2=recent plausible VGA/720p edges. */
unsigned te_status(void) {
    if(!te_enabled)return 0;
    uint32_t period=te_stats.period;
    return te_stats.edges>=2 && period>=10*DELAY_MS_TIME && period<=50*DELAY_MS_TIME &&
           (uint32_t)(funSysTick32()-te_stats.last)<100*DELAY_MS_TIME ? 2:1;
}
int display_sync(void) {
    if(te_status()!=2)return !app_cancelled();
    uint32_t edge=te_stats.edges,start=funSysTick32();
    while(te_stats.edges==edge && te_enabled){
        if(app_cancelled())return 0;
        if((uint32_t)(funSysTick32()-start)>=40*DELAY_MS_TIME){
            ++te_stats.timeouts;return 1;
        }
    }
    if(te_enabled)++te_stats.waits;
    return 1;
}
