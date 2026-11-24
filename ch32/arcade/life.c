#include "app.h"
#include <string.h>
#define LIFE_W 128
#define LIFE_H 64
#define WPR 4
#define grid app_memory.life
#define LASTMASK ((LIFE_W % 32) ? (0xFFFFFFFFu << (32 - (LIFE_W % 32))) : 0xFFFFFFFFu)
static uint8_t cur;

#define MAJ(a,b,c) (((a)&(b)) | ((c)&((a)^(b))))
static inline uint32_t shl_sp(const uint32_t *row, int w)
{
    uint32_t v = row[w] << 1;
    if (w + 1 < WPR) v |= row[w+1] >> 31;
    return v;
}
static inline uint32_t shr_sp(const uint32_t *row, int w)
{
    uint32_t v = row[w] >> 1;
    if (w > 0) v |= row[w-1] << 31;
    return v;
}

static void life_step(void)
{
    const uint32_t (*g)[WPR] = grid[cur];
    uint32_t (*n)[WPR] = grid[cur ^ 1];
    static const uint32_t zrow[WPR] = {0};
    for (int y = 0; y < LIFE_H; y++) {
        const uint32_t *ra = (y > 0) ? g[y-1] : zrow;
        const uint32_t *rb = g[y];
        const uint32_t *rc = (y < LIFE_H-1) ? g[y+1] : zrow;
        for (int w = 0; w < WPR; w++) {
            uint32_t a = ra[w], al = shr_sp(ra, w), ar = shl_sp(ra, w);
            uint32_t bl = shr_sp(rb, w), br = shl_sp(rb, w);
            uint32_t c = rc[w], cl = shr_sp(rc, w), cr = shl_sp(rc, w);
            uint32_t s0 = al^a^ar, c0 = MAJ(al,a,ar);
            uint32_t s1 = cl^c^cr, c1 = MAJ(cl,c,cr);
            uint32_t s2 = bl^br,   c2 = bl&br;
            uint32_t lo = s0^s1^s2, lc = MAJ(s0,s1,s2);
            uint32_t t0 = c0^c1^c2, t1 = MAJ(c0,c1,c2);
            uint32_t b1 = t0^lc, b1c = t0&lc;
            uint32_t b2 = t1^b1c, b3 = t1&b1c;
            uint32_t nx = ~b2 & ~b3 & b1 & (lo | rb[w]);
            n[y][w] = (w == WPR-1) ? (nx & LASTMASK) : nx;
        }
    }
    cur ^= 1;
}

static void life_send(void)
{
    if(!display_sync())return;
    display_window(16,13,LIFE_W,LIFE_H);
    for (int y = 0; y < LIFE_H; y++)
        for (int w = 0; w < WPR; w++) {
            uint32_t v = grid[cur][y][w];
            int nb = (w == WPR-1 && (LIFE_W % 32)) ? (LIFE_W % 32) / 8 : 4;
            for (int k = 0; k < nb; k++)
                display_byte((uint8_t)(v >> (24 - 8*k)));
        }
    display_end();
}

static void put_cell(int x, int y)
{
    if (x >= 0 && x < LIFE_W && y >= 0 && y < LIFE_H)
        grid[cur][y][x >> 5] |= 0x80000000u >> (x & 31);
}

/* patterns as (x,y) pairs */
static const uint8_t GUN[] = {0,4,0,5,1,4,1,5,10,4,10,5,10,6,11,3,11,7,12,2,
    12,8,13,2,13,8,14,5,15,3,15,7,16,4,16,5,16,6,17,5,20,2,20,3,20,4,21,2,
    21,3,21,4,22,1,22,5,24,0,24,1,24,5,24,6,34,2,34,3,35,2,35,3};
static const uint8_t RPENT[] = {1,0,2,0,0,1,1,1,1,2};
static const uint8_t LWSS[] = {0,0,3,0,4,1,0,2,4,2,1,3,2,3,3,3,4,3};

static void place(const uint8_t *cells, int n, int ox, int oy)
{
    for (int i = 0; i < n; i += 2)
        put_cell(ox + cells[i], oy + cells[i+1]);
}

static uint8_t style;
static void seed(void) {
    cur=0; memset(app_memory.life,0,sizeof(app_memory.life));
    if(style==0)place(GUN,sizeof(GUN),12,8);
    else if(style==1)place(RPENT,sizeof(RPENT),62,30);
    else if(style==2)for(int i=0;i<5;++i)place(LWSS,sizeof(LWSS),4+(i&1)*8,4+i*12);
    else {uint32_t r=0x12345;for(int y=8;y<56;++y)for(int x=16;x<112;++x){r=r*1664525u+1013904223u;if((r>>29)==0)put_cell(x,y);}}
    uart_puts("CONWAY ");uart_number(style);uart_puts(" / J next / Q menu\r\n");
}
static void frame(void){if(input_take(BTN_ACTION)){style=(style+1)%4;seed();}life_send();life_step();}
void life_enter(void){style=0;display_layout(2,0x11);display_clear(160,90,0x11);seed();app_frame_ms=50;app_callback=frame;}
