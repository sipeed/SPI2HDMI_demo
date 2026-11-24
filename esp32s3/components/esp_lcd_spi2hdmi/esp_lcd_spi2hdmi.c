// SPDX-License-Identifier: MIT
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_spi2hdmi.h"

#define TRY(call) do { esp_err_t e_ = (call); if (e_ != ESP_OK) return e_; } while (0)
typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    bool quad, reset_high;
    int reset_pin, x_gap, y_gap;
    uint8_t scale, madctl, mode;   // mode: 0 = 640x480@60, 1 = 1280x720@30
} bridge_t;

static int command(bridge_t *p, uint8_t op, uint8_t reg)
{
    return p->quad ? ((int)op << 24) | ((int)reg << 8) : reg;
}
static esp_err_t param(bridge_t *p, uint8_t reg, const void *data, size_t n)
{
    return esp_lcd_panel_io_tx_param(p->io, command(p, 0x02, reg), data, n);
}
static void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms) + 1); }
static esp_err_t panel_delete(esp_lcd_panel_t *panel)
{
    bridge_t *p = (bridge_t *)panel;
    // Drain DMA before deleting; the IO handle remains owned by the caller.
    TRY(param(p, 0x00, NULL, 0));
    if (p->reset_pin >= 0) gpio_reset_pin(p->reset_pin);
    free(p);
    return ESP_OK;
}
static esp_err_t panel_reset(esp_lcd_panel_t *panel)
{
    bridge_t *p = (bridge_t *)panel;
    TRY(param(p, 0x00, NULL, 0));
    if (p->reset_pin >= 0) {
        TRY(gpio_set_level(p->reset_pin, p->reset_high)); delay_ms(2);
        TRY(gpio_set_level(p->reset_pin, !p->reset_high)); delay_ms(10);
    } else { TRY(param(p, 0x01, NULL, 0)); delay_ms(2); }
    return ESP_OK;
}
esp_err_t esp_lcd_spi2hdmi_clear(esp_lcd_panel_handle_t panel)
{
    if (!panel) return ESP_ERR_INVALID_ARG;
    const uint8_t erase = 1;
    TRY(param((bridge_t *)panel, 0x8C, &erase, 1));
    delay_ms(32);
    return ESP_OK;
}
esp_err_t esp_lcd_spi2hdmi_set_scale(esp_lcd_panel_handle_t panel, uint8_t scale)
{
    if (!panel || scale > 3) return ESP_ERR_INVALID_ARG;
    bridge_t *p = (bridge_t *)panel;
    if (p->mode && scale == 0) return ESP_ERR_INVALID_ARG; // 720p starts at 2x
    // `scale` is the pixel shift; the bridge's SCALESET code counts from the
    // mode's base zoom, so 720p code 0 already means 2x.
    const uint8_t code = p->mode ? scale - 1 : scale;
    TRY(param(p, 0x88, &code, 1));
    p->scale = scale;
    return esp_lcd_spi2hdmi_clear(panel);
}
esp_err_t esp_lcd_spi2hdmi_set_mode(esp_lcd_panel_handle_t panel, uint8_t mode)
{
    if (!panel || mode > 1) return ESP_ERR_INVALID_ARG;
    bridge_t *p = (bridge_t *)panel;
    TRY(param(p, 0x8B, &mode, 1));
    p->mode = mode;
    delay_ms(50);  // video PLL re-locks
    if (mode && p->scale == 0) p->scale = 1;
    return esp_lcd_spi2hdmi_set_scale(panel, p->scale);
}
static esp_err_t panel_init(esp_lcd_panel_t *panel)
{
    bridge_t *p = (bridge_t *)panel;
    const uint8_t rgb565 = 0x55;
    TRY(param(p, 0x3A, &rgb565, 1));
    TRY(param(p, 0x36, &p->madctl, 1));
    TRY(esp_lcd_spi2hdmi_set_scale(panel, p->scale));
    return param(p, 0x29, NULL, 0);
}
static esp_err_t panel_draw(esp_lcd_panel_t *panel, int x0, int y0,
                            int x1, int y1, const void *colors)
{
    bridge_t *p = (bridge_t *)panel;
    int w = (p->mode ? 1280 : 640) >> p->scale, h = (p->mode ? 720 : 480) >> p->scale;
    // Validate before adding gaps (avoids signed overflow on bad coordinates).
    if (!colors || x0 < 0 || y0 < 0 || x1 <= x0 || y1 <= y0 ||
        x1 > w || y1 > h || x1 > w-p->x_gap || y1 > h-p->y_gap)
        return ESP_ERR_INVALID_ARG;
    x0 += p->x_gap; x1 += p->x_gap; y0 += p->y_gap; y1 += p->y_gap;
    const uint8_t columns[] = {x0 >> 8, x0, (x1-1) >> 8, x1-1};
    const uint8_t rows[] = {y0 >> 8, y0, (y1-1) >> 8, y1-1};
    TRY(param(p, 0x2A, columns, sizeof(columns)));
    TRY(param(p, 0x2B, rows, sizeof(rows)));
    return esp_lcd_panel_io_tx_color(p->io, command(p, 0x32, 0x2C), colors,
                                    (size_t)(x1-x0) * (y1-y0) * 2);
}
static esp_err_t panel_gap(esp_lcd_panel_t *panel, int x, int y)
{
    bridge_t *p = (bridge_t *)panel;
    if (x < 0 || y < 0 || x >= ((p->mode ? 1280 : 640) >> p->scale) ||
        y >= ((p->mode ? 720 : 480) >> p->scale))
        return ESP_ERR_INVALID_ARG;
    p->x_gap = x; p->y_gap = y;
    return ESP_OK;
}
static esp_err_t panel_invert(esp_lcd_panel_t *panel, bool invert)
{ return param((bridge_t *)panel, invert ? 0x21 : 0x20, NULL, 0); }
static esp_err_t panel_display(esp_lcd_panel_t *panel, bool on)
{ return param((bridge_t *)panel, on ? 0x29 : 0x28, NULL, 0); }
static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool x, bool y)
{ (void)panel; return (x || y) ? ESP_ERR_NOT_SUPPORTED : ESP_OK; }
static esp_err_t panel_swap(esp_lcd_panel_t *panel, bool swap)
{ (void)panel; return swap ? ESP_ERR_NOT_SUPPORTED : ESP_OK; }

esp_err_t esp_lcd_new_panel_spi2hdmi(esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *config, esp_lcd_panel_handle_t *out_panel)
{
    if (!io || !config || !out_panel) return ESP_ERR_INVALID_ARG;
    *out_panel = NULL;
    if (config->bits_per_pixel != 16 || config->data_endian != LCD_RGB_DATA_ENDIAN_BIG ||
        (config->rgb_ele_order != LCD_RGB_ELEMENT_ORDER_RGB &&
         config->rgb_ele_order != LCD_RGB_ELEMENT_ORDER_BGR)) return ESP_ERR_NOT_SUPPORTED;
    const spi2hdmi_vendor_config_t defaults = {0};
    const spi2hdmi_vendor_config_t *v = config->vendor_config ? config->vendor_config : &defaults;
    if (v->scale > 3 || config->reset_gpio_num < -1) return ESP_ERR_INVALID_ARG;
    if (config->reset_gpio_num >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(config->reset_gpio_num))
        return ESP_ERR_INVALID_ARG;
    bridge_t *p = calloc(1, sizeof(*p));
    if (!p) return ESP_ERR_NO_MEM;
    p->io = io; p->quad = v->use_qspi; p->scale = v->scale;
    p->madctl = config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR ? 8 : 0;
    p->reset_pin = config->reset_gpio_num; p->reset_high = config->flags.reset_active_high;
    if (p->reset_pin >= 0) {
        gpio_config_t gc = {.pin_bit_mask = 1ULL << p->reset_pin, .mode = GPIO_MODE_OUTPUT};
        esp_err_t e = gpio_config(&gc);
        if (e == ESP_OK) e = gpio_set_level(p->reset_pin, !p->reset_high);
        if (e != ESP_OK) { gpio_reset_pin(p->reset_pin); free(p); return e; }
    }
    p->base.del = panel_delete; p->base.reset = panel_reset; p->base.init = panel_init;
    p->base.draw_bitmap = panel_draw; p->base.set_gap = panel_gap;
    p->base.invert_color = panel_invert; p->base.disp_on_off = panel_display;
    p->base.mirror = panel_mirror; p->base.swap_xy = panel_swap;
    *out_panel = &p->base;
    return ESP_OK;
}
