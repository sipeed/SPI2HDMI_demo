/* Bad Apple: key/XOR frames, bounded pixel-run exponential Golomb, shared 1800-byte RAM.
 * Player code GPL-3.0-or-later; video artwork retains its original rights. */
#include "app.h"
#include "bitrun.h"
#include "clip_160x90.h"
#ifndef CLIP_CODEC_BITRUN1
#error "Generate arcade video with tools/gen_arcade_clip.py (bitrun format)"
#endif
#include <string.h>

#define VIDEO_BYTES (CLIP_W*CLIP_H/8)
_Static_assert(VIDEO_BYTES<=sizeof(app_memory.raw),"video buffer capacity");
static unsigned offset,frame_number;
static uint32_t deadline;
static uint8_t phase;

static void restart(void) {
    memset(app_memory.raw,0,VIDEO_BYTES);offset=frame_number=phase=0;
    deadline=TICK_Get();
}
static void frame(void) {
    if(input_take(BTN_ACTION))restart();
    uint32_t now=TICK_Get();
    if((int32_t)(now-deadline)<0)return;
    if(frame_number==CLIP_FRAMES){
        memset(app_memory.raw,0,VIDEO_BYTES);offset=frame_number=0;
    }
    if(CLIP_BYTES-offset<2)goto corrupt;
    unsigned n=clip_data[offset]|((unsigned)clip_data[offset+1]<<8);
    offset+=2;
    if(n>CLIP_BYTES-offset ||
       !bitrun_decode(clip_data+offset,n,app_memory.raw,VIDEO_BYTES))goto corrupt;
    offset+=n;++frame_number;
    if(!display_sync())return;
    display_window(0,0,CLIP_W,CLIP_H);
    for(unsigned i=0;i<VIDEO_BYTES;++i)display_byte(app_memory.raw[i]);
    display_end();
    /* Fractional milliseconds (e.g. 66/67/67 at 15 fps) include transfer time. */
    unsigned step=1000+phase;
    deadline+=step/CLIP_FPS;phase=step%CLIP_FPS;
    if((int32_t)(now-deadline)>200)deadline=now;
    return;
corrupt:
    uart_puts("VIDEO DATA ERROR\r\n");app_select(0);
}
void video_enter(void) {
    display_layout(2,0x11);display_clear(CLIP_W,CLIP_H,0x11);
    restart();app_frame_ms=1;app_callback=frame;
}
