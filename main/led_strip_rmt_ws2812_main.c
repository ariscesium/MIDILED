#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"
#include "usb/usb_midi.h"

// GPIO assignment
#define LED_STRIP_GPIO         18
#define LED_STRIP_NUM_LEDS     60
#define LED_STRIP_RMT_RES_HZ   (10 * 1000 * 1000)

static const char *TAG = "MIDI_LED";

// Define a queue to handle MIDI events
static QueueHandle_t midi_event_queue;

// LED Strip handle
static led_strip_handle_t led_strip;

// Function to configure the LED strip
led_strip_handle_t configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_NUM_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
#endif
    };

    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

// Function to map MIDI note and velocity to LED color
uint32_t get_color_from_note_and_velocity(uint8_t note, uint8_t velocity)
{
    uint8_t brightness = velocity * 2; // Scale velocity (0-127) to brightness (0-255)
    uint8_t red = (note % 12) * 21;    // Map the note to a color
    uint8_t green = 255 - red;
    uint8_t blue = (note / 12) * 21;
    return (red << 16) | (green << 8) | blue;
}

// Task to handle MIDI events
void midi_task(void *arg)
{
    usb_host_handle_t usb_host;
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    while (1) {
        usb_midi_event_t midi_event;
        if (usb_midi_read_event(&midi_event) == ESP_OK) {
            // Send MIDI event to the queue
            xQueueSend(midi_event_queue, &midi_event, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Adjust the delay for optimal performance
    }
}

// Task to control the LED strip based on MIDI events
void led_task(void *arg)
{
    usb_midi_event_t midi_event;

    while (1) {
        if (xQueueReceive(midi_event_queue, &midi_event, portMAX_DELAY)) {
            if (midi_event.type == MIDI_NOTE_ON && midi_event.velocity > 0) {
                uint32_t color = get_color_from_note_and_velocity(midi_event.note, midi_event.velocity);
                int ledIndex = (midi_event.note - 21) * LED_STRIP_NUM_LEDS / (108 - 21); // Map note range to LED range
                led_strip_set_pixel(led_strip, ledIndex, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                led_strip_refresh(led_strip);
            } else if (midi_event.type == MIDI_NOTE_OFF || (midi_event.type == MIDI_NOTE_ON && midi_event.velocity == 0)) {
                int ledIndex = (midi_event.note - 21) * LED_STRIP_NUM_LEDS / (108 - 21);
                led_strip_set_pixel(led_strip, ledIndex, 0, 0, 0);
                led_strip_refresh(led_strip);
            }
        }
    }
}

void app_main(void)
{
    // Configure the LED strip
    led_strip = configure_led();
    
    // Initialize the queue for MIDI events
    midi_event_queue = xQueueCreate(10, sizeof(usb_midi_event_t));
    
    // Create FreeRTOS tasks
    xTaskCreate(midi_task, "midi_task", 4096, NULL, 10, NULL);
    xTaskCreate(led_task, "led_task", 4096, NULL, 10, NULL);
}
