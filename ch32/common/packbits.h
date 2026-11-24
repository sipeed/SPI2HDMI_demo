#ifndef SPI2HDMI_PACKBITS_H
#define SPI2HDMI_PACKBITS_H

#include <stddef.h>
#include <stdint.h>

/* One length-delimited frame. Reject truncated packets, output overflow and
 * incomplete frames; never read across a compressed frame's boundary. */
static int packbits_decode(const uint8_t *src, size_t len,
                          uint8_t *dst, size_t capacity, int xor_delta)
{
    size_t i = 0, o = 0;
    while (i < len) {
        uint8_t c = src[i++];
        if (c == 128) continue;
        size_t n = c < 128 ? (size_t)c + 1 : 257u - c;
        size_t input = c < 128 ? n : 1;
        if (input > len - i || n > capacity - o) return 0;
        for (size_t j = 0; j < n; j++) {
            uint8_t v = src[i + (c < 128 ? j : 0)];
            if (xor_delta) dst[o++] ^= v;
            else dst[o++] = v;
        }
        i += input;
    }
    return o == capacity;
}

#endif
