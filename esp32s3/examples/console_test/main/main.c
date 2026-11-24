// SPDX-License-Identifier: MIT
// SPI2HDMI console test for an ESP32-S3 dev board (ESP32-S3-WROOM-1).
// Interactive console on UART0; drives the module in BOTH transports.  The
// module picks SPI or QSPI from the level on IO3 when RST is released, so this
// firmware re-straps and resets it on every `mode` command.
//
//   ESP32-S3    role (SPI mode)   role (QSPI mode)    SPI2HDMI pin
//   GPIO12      SCK               SCK                 SCK
//   GPIO10      CS                CS                  CS
//   GPIO11      MOSI              D0                  IO0
//   GPIO13      DC                D1                  IO1
//   GPIO14      -                 D2                  IO2
//   GPIO9       -                 D3 + mode strap     IO3
//   GPIO8       RST               RST                 RST
//   GPIO3       TE (frame sync)   TE                  TE
//   5V / GND    power             power               5V / GND
//
// Commands (newline-terminated):
//   mode spi|qspi        re-strap the bridge and re-init the transport
//   freq <MHz>           SPI/QSPI clock (default 40; ESP32-S3 grants 80/40/26.7/20..)
//   480 <1|2|4|8|16>     VGA output, zoom
//   720 <2|4|8|16>       720p output, zoom
//   bars|grid|grad|checker   draw a test pattern
//   inv <0|1>            display inversion
//   te <0|1>             wait for the TE edge before each repaint
//   tone <0|1>           1: 200..2000Hz sine sweep sent as HDMI audio (0x8E); 0: stop
//   status               print current configuration (incl. measured TE rate)
//   redraw               repaint the last pattern
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_spi2hdmi.h"

#define PIN_SCK   12
#define PIN_CS    10
#define PIN_D0    11
#define PIN_D1_DC 13
#define PIN_D2    14
#define PIN_D3    9
#define PIN_RST   8
#define PIN_TE    3   // GPIO3's JTAG strap is efuse-gated, safe as input
#define ROWS      48

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_done;
static uint8_t *s_buf;
static bool s_qspi = true;
static int s_mhz = 40;
static int s_mode = 0;      // 0 = 480p, 1 = 720p
static int s_scale = 0;     // zoom = 1 << scale
static char s_pattern[12] = "bars";
static uint16_t s_fill = 0xFFFF;   // color for the "solid" pattern (RGB565, BE)
static bool s_te_sync = false;
static SemaphoreHandle_t s_te_sem;
static volatile int64_t s_te_last, s_te_period;
static volatile uint32_t s_te_count;

static void IRAM_ATTR te_isr(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();
    s_te_period = now - s_te_last;
    s_te_last = now;
    s_te_count++;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_te_sem, &wake);
    if (wake == pdTRUE)
        portYIELD_FROM_ISR();
}

static void te_wait(void)
{
    if (!s_te_sync)
        return;
    xSemaphoreTake(s_te_sem, 0);   // discard a stale edge, sync to the next one
    if (xSemaphoreTake(s_te_sem, pdMS_TO_TICKS(100)) != pdTRUE)
        printf("te: no edge within 100ms (TE wired? bridge alive?)\n");
}

static bool color_done(esp_lcd_panel_io_handle_t io,
                       esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    (void)io; (void)ev;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx, &wake);
    return wake == pdTRUE;
}

// hold the bridge in reset, present the strap on D3, release, keep the strap
// stable through the strap-sampling window, then hand D3 back to the bus
static void strap_bridge(bool qspi)
{
    gpio_reset_pin(PIN_D3);
    gpio_set_direction(PIN_D3, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_D3, qspi);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));   // strap window is ~26us; 10ms is ample
    gpio_reset_pin(PIN_D3);          // release to the SPI peripheral / pull-down
}

static void transport_down(void)
{
    if (s_panel) { esp_lcd_panel_del(s_panel); s_panel = NULL; }
    if (s_io) { esp_lcd_panel_io_del(s_io); s_io = NULL; }
    spi_bus_free(SPI2_HOST);
}

static esp_err_t transport_up(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCK,
        .mosi_io_num = PIN_D0,
        .miso_io_num = s_qspi ? PIN_D1_DC : -1,
        .quadwp_io_num = s_qspi ? PIN_D2 : -1,
        .quadhd_io_num = s_qspi ? PIN_D3 : -1,
        .max_transfer_sz = 1280 * ROWS * 2,
    };
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (e != ESP_OK) return e;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = s_qspi ? -1 : PIN_D1_DC,
        .spi_mode = 0,
        .pclk_hz = s_mhz * 1000 * 1000,
        .trans_queue_depth = 4,
        .lcd_cmd_bits = s_qspi ? 32 : 8,
        .lcd_param_bits = 8,
        .cs_ena_posttrans = 1,
        .on_color_trans_done = color_done,
        .user_ctx = s_done,
        .flags.quad_mode = s_qspi,
    };
    e = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &s_io);
    if (e != ESP_OK) return e;
    spi2hdmi_vendor_config_t vendor = { .use_qspi = s_qspi, .scale = s_scale };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1, .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .vendor_config = &vendor,
    };
    e = esp_lcd_new_panel_spi2hdmi(s_io, &panel_config, &s_panel);
    if (e != ESP_OK) return e;
    e = esp_lcd_panel_init(s_panel);
    if (e == ESP_OK && s_mode)
        e = esp_lcd_spi2hdmi_set_mode(s_panel, 1);
    return e;
}

static void set_transport(bool qspi)
{
    transport_down();
    s_qspi = qspi;
    strap_bridge(qspi);
    s_mode = 0; s_scale = 0;   // hardware reset restores bridge defaults
    esp_err_t e = transport_up();
    printf("%s transport %s (bridge re-strapped, 480p 1x)\n",
           qspi ? "QSPI" : "SPI", e == ESP_OK ? "ready" : esp_err_to_name(e));
}

static void geometry(int *w, int *h)
{
    *w = (s_mode ? 1280 : 640) >> s_scale;
    *h = (s_mode ? 720 : 480) >> s_scale;
}

static uint16_t pat_pixel(const char *pat, int x, int y, int w, int h)
{
    static const uint16_t bars[8] =
        {0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFE0, 0xF81F, 0x07FF, 0x0000};
    if (!strcmp(pat, "bars")) {
        if (y >= h - h / 8)  // bottom band: fine gradient for banding checks
            return ((x * 31 / (w - 1)) << 11) | ((x * 63 / (w - 1)) << 5) | (x * 31 / (w - 1));
        return bars[x * 8 / w];
    }
    if (!strcmp(pat, "grid")) {
        bool line = (x % 32) == 0 || (y % 32) == 0 || x == w - 1 || y == h - 1;
        bool corner = (x < 8 && y < 8) || (x >= w - 8 && y < 8) ||
                      (x < 8 && y >= h - 8) || (x >= w - 8 && y >= h - 8);
        return corner ? 0xF800 : line ? 0xFFFF : 0x0000;
    }
    if (!strcmp(pat, "grad"))
        return ((y * 31 / (h - 1)) << 11) | ((x * 63 / (w - 1)) << 5) |
               (((x + y) * 31 / (w + h - 2)));
    if (!strcmp(pat, "solid"))
        return s_fill;
    // checker
    return (((x >> 4) ^ (y >> 4)) & 1) ? 0xFFFF : 0x0000;
}

static void draw_pattern(const char *pat)
{
    int w, h;
    geometry(&w, &h);
    strncpy(s_pattern, pat, sizeof(s_pattern) - 1);
    te_wait();
    int64_t bytes = 0;
    for (int y = 0; y < h; y += ROWS) {
        int rows = (y + ROWS <= h) ? ROWS : (h - y);
        for (int r = 0; r < rows; ++r)
            for (int x = 0; x < w; ++x) {
                uint16_t c = pat_pixel(pat, x, y + r, w, h);
                s_buf[2 * (r * w + x)] = c >> 8;
                s_buf[2 * (r * w + x) + 1] = c;
            }
        if (esp_lcd_panel_draw_bitmap(s_panel, 0, y, w, y + rows, s_buf) != ESP_OK) {
            printf("draw failed at y=%d\n", y);
            return;
        }
        xSemaphoreTake(s_done, portMAX_DELAY);
        bytes += (int64_t)w * rows * 2;
    }
    printf("%s %dx%d done (%lld bytes)\n", pat, w, h, bytes);
}

// ---- 0x8E audio: 200..2000Hz sine sweep, 32kHz 16-bit mono ----
static volatile bool s_tone_run;
static TaskHandle_t s_tone_task;

static int s_tone_mode = 1;   // 1 sine sweep, 2 ramp, 3 sine small-chunks

static void tone_task(void *arg)
{
    (void)arg;
    static int16_t sine[256];
    for (int i = 0; i < 256; ++i)
        sine[i] = (int16_t)(18000.0f * sinf(2.0f * (float)M_PI * i / 256.0f));

    const int chunk = (s_tone_mode == 3) ? 96 : 480;   // 2ms or 10ms at 48kHz
    const int period_ms = (s_tone_mode == 3) ? 2 : 10;
    uint8_t *buf = heap_caps_malloc(chunk * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf);

    uint32_t phase = 0;
    uint32_t t = 0;                        // sample index within the 5s sweep
    uint16_t ramp = 0;
    TickType_t wake = xTaskGetTickCount();
    printf("tone: mode %d started (chunk %d / %dms)\n", s_tone_mode, chunk, period_ms);

    while (s_tone_run) {
        for (int i = 0; i < chunk; ++i) {
            int16_t v;
            if (s_tone_mode == 2) {
                v = (int16_t)ramp++;                           // sawtooth marker
            } else {
                uint32_t freq = 200 + (1800 * t) / (5 * 48000);
                phase += (uint32_t)(((uint64_t)freq << 32) / 48000);
                v = sine[phase >> 24];
                if (++t >= 5 * 48000) t = 0;
            }
            buf[2 * i]     = (uint8_t)v;                       // little-endian
            buf[2 * i + 1] = (uint8_t)((uint16_t)v >> 8);
        }
        int hdr = s_qspi ? (0x02000000 | (0x8E << 8)) : 0x8E;
        esp_err_t te = esp_lcd_panel_io_tx_param(s_io, hdr, buf, chunk * 2);
        static int terr_n;
        if (te != ESP_OK && terr_n++ < 5)
            printf("tone: tx_param failed: %s\n", esp_err_to_name(te));
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(period_ms));
    }

    free(buf);
    printf("tone: stopped\n");
    s_tone_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_status(void)
{
    int w, h;
    geometry(&w, &h);
    printf("transport=%s clk=%dMHz output=%s zoom=%dx logical=%dx%d pattern=%s\n",
           s_qspi ? "QSPI" : "SPI", s_mhz, s_mode ? "1280x720@30" : "640x480@60",
           1 << s_scale, w, h, s_pattern);
    int64_t period = s_te_period;
    if (period > 0 && esp_timer_get_time() - s_te_last < 500000)
        printf("te=%.2fHz edges=%lu sync=%s\n", 1e6 / (double)period,
               (unsigned long)s_te_count, s_te_sync ? "on" : "off");
    else
        printf("te=inactive sync=%s\n", s_te_sync ? "on" : "off");
}

void app_main(void)
{
    s_done = xSemaphoreCreateBinary();
    assert(s_done);
    s_buf = heap_caps_malloc(1280 * ROWS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(s_buf);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RST, 1);

    s_te_sem = xSemaphoreCreateBinary();
    assert(s_te_sem);
    gpio_config(&(gpio_config_t){
        .pin_bit_mask = 1ULL << PIN_TE,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
    });
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_TE, te_isr, NULL);

    set_transport(true);         // default QSPI 40MHz
    draw_pattern("bars");
    printf("spi2hdmi-s3-test ready. type 'help'\n");

    char line[64];
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) { vTaskDelay(1); continue; }
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = 0;
        if (!line[0]) continue;
        char a[16] = "", b[16] = "";
        sscanf(line, "%15s %15s", a, b);
        if (!strcmp(a, "help")) {
            printf("mode spi|qspi / freq N / 480 z / 720 z / bars grid grad checker /"
                   " inv 0|1 / te 0|1 / tone 0|1 / status / redraw\n");
        } else if (!strcmp(a, "mode")) {
            set_transport(!strcmp(b, "qspi"));
            draw_pattern(s_pattern);
        } else if (!strcmp(a, "freq")) {
            int mhz = atoi(b);
            if (mhz >= 1 && mhz <= 80) {
                s_mhz = mhz;
                transport_down();
                printf("reinit at %dMHz: %s\n", mhz,
                       transport_up() == ESP_OK ? "ok" : "FAILED");
                draw_pattern(s_pattern);
            } else printf("freq 1..80\n");
        } else if (!strcmp(a, "480") || !strcmp(a, "720")) {
            int z = atoi(b), mode = !strcmp(a, "720");
            int sc = (z == 1) ? 0 : (z == 2) ? 1 : (z == 4) ? 2 : (z == 8) ? 3 : -1;
            if (sc < 0 || (mode && sc == 0)) { printf("bad zoom\n"); continue; }
            esp_err_t e = ESP_OK;
            if (mode != s_mode) e = esp_lcd_spi2hdmi_set_mode(s_panel, mode);
            if (e == ESP_OK) e = esp_lcd_spi2hdmi_set_scale(s_panel, sc);
            if (e == ESP_OK) { s_mode = mode; s_scale = sc; draw_pattern(s_pattern); }
            else printf("%s\n", esp_err_to_name(e));
        } else if (!strcmp(a, "tone")) {
            int m = b[0] ? atoi(b) : 1;
            if (m > 0 && !s_tone_task) {
                s_tone_mode = m;
                s_tone_run = true;
                xTaskCreatePinnedToCore(tone_task, "tone", 4096, NULL, 5, &s_tone_task, 1);
            } else if (m == 0) {
                s_tone_run = false;
            } else printf("tone already running\n");
        } else if (!strcmp(a, "te")) {
            s_te_sync = atoi(b) != 0;
            printf("te sync %s\n", s_te_sync ? "on" : "off");
        } else if (!strcmp(a, "inv")) {
            esp_lcd_panel_invert_color(s_panel, atoi(b) != 0);
        } else if (!strcmp(a, "status")) {
            cmd_status();
        } else if (!strcmp(a, "redraw")) {
            draw_pattern(s_pattern);
        } else if (!strcmp(a, "fill")) {
            unsigned rgb = (unsigned)strtoul(b, NULL, 16);   // RRGGBB
            uint8_t r = rgb >> 16, g = rgb >> 8, bl = rgb;
            s_fill = ((r >> 3) << 11) | ((g >> 2) << 5) | (bl >> 3);
            printf("fill RGB888=%02x%02x%02x -> RGB565=%04x\n", r, g, bl, s_fill);
            draw_pattern("solid");
        } else if (!strcmp(a, "bars") || !strcmp(a, "grid") ||
                   !strcmp(a, "grad") || !strcmp(a, "checker")) {
            draw_pattern(a);
        } else {
            printf("unknown '%s' (try help)\n", a);
        }
    }
}
