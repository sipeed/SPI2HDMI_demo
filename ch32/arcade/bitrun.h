/* Bounded pixel-run exponential-Golomb decoder. GPL-3.0-or-later. */
#ifndef ARCADE_BITRUN_H
#define ARCADE_BITRUN_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {const uint8_t *p;size_t n,pos;} BitReader;
static int bitread(BitReader *r) {
    if(r->pos>=r->n*8)return -1;
    int b=(r->p[r->pos>>3]>>(7-(r->pos&7)))&1;++r->pos;return b;
}
static int bitrun_decode(const uint8_t *src,size_t len,uint8_t *dst,size_t bytes) {
    if(len<2 || (src[0]&~3u) || (src[1]&0x88))return 0;
    unsigned bit=src[0]&1,is_delta=src[0]&2;
    unsigned order[2]={src[1]&7,(src[1]>>4)&7};
    BitReader r={src+2,len-2,0};size_t pos=0,pixels=bytes*8;
    if(!is_delta)memset(dst,0,bytes);
    while(pos<pixels){
        unsigned zeros=0,value=1,k=order[bit];int b;
        while((b=bitread(&r))==0)if(++zeros>=15)return 0;
        if(b<0)return 0;
        while(zeros--){b=bitread(&r);if(b<0)return 0;value=(value<<1)|b;}
        unsigned n=(value-1)<<k;
        for(unsigned i=k;i;--i){b=bitread(&r);if(b<0)return 0;n|=(unsigned)b<<(i-1);}
        ++n;if(n>pixels-pos)return 0;
        if(bit){
            size_t end=pos+n;
            while(pos<end){unsigned shift=pos&7,count=8-shift;if(count>end-pos)count=end-pos;
                dst[pos>>3]^=(uint8_t)((0xffu>>shift)&(0xffu<<(8-shift-count)));
                pos+=count;
            }
        }else pos+=n;
        bit^=1;
    }
    /* Only zero padding to the next byte is allowed after the final run. */
    if((r.pos+7)/8!=r.n)return 0;
    while(r.pos<r.n*8)if(bitread(&r)!=0)return 0;
    return 1;
}
#endif
