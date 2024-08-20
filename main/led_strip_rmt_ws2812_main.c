#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"
#include "driver/rmt.h"

// GPIO assignment
#define LED_STRIP_GPIO         48
#define LED_STRIP_NUM_LEDS     1
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000)

static const char *TAG = "MIDI_LED";
static led_strip_handle_t led_strip;

void configure_led(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_NUM_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
}

void process_midi_message(uint8_t *data, size_t len) {
    if (len < 3) return;  // MIDI messages are typically 3 bytes long

    uint8_t status = data[0];
    uint8_t note = data[1];
    uint8_t velocity = data[2];

    if ((status & 0xF0) == 0x90) {  // Note On message
        uint32_t color = (note * 2) << 16 | (255 - note * 2) << 8 | (velocity * 2);
        int ledIndex = (note - 21) * LED_STRIP_NUM_LEDS / (108 - 21);
        led_strip_set_pixel(led_strip, ledIndex, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
        led_strip_refresh(led_strip);
    } else if ((status & 0xF0) == 0x80) {  // Note Off message
        int ledIndex = (note - 21) * LED_STRIP_NUM_LEDS / (108 - 21);
        led_strip_set_pixel(led_strip, ledIndex, 0, 0, 0);
        led_strip_refresh(led_strip);
    }
}

void midi_task(void *arg) {
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    // Initialize and handle the USB device here.

    while (1) {
        // Simulate reading MIDI data
        uint8_t midi_data[3] = {0x90, 60, 100};  // Example MIDI Note On message
        process_midi_message(midi_data, sizeof(midi_data));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    configure_led();
    xTaskCreate(midi_task, "midi_task", 4096, NULL, 10, NULL);
}
