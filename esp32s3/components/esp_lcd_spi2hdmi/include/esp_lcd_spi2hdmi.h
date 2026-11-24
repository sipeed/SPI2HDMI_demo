// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool use_qspi;       // io lcd_cmd_bits=32 for QSPI, 8 for legacy SPI
    uint8_t scale;       // 0..3 => 640x480, 320x240, 160x120, 80x60 logical pixels
} spi2hdmi_vendor_config_t;

// RGB565 only. color_data bytes must be MSB first; DMA buffer ownership stays
// with the caller until esp_lcd's on_color_trans_done callback. No 128B rule.
esp_err_t esp_lcd_new_panel_spi2hdmi(esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *config, esp_lcd_panel_handle_t *out_panel);

// These calls wait for earlier queued color transfers through tx_param.
// Clear/layout changes include the specified 32ms erase wait before returning.
esp_err_t esp_lcd_spi2hdmi_clear(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_spi2hdmi_set_scale(esp_lcd_panel_handle_t panel, uint8_t scale);
/* 0 = 640x480@60 (default), 1 = 1280x720@30 (scale auto-raised to >=1) */
esp_err_t esp_lcd_spi2hdmi_set_mode(esp_lcd_panel_handle_t panel, uint8_t mode);

#ifdef __cplusplus
}
#endif
