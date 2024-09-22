#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include <Adafruit_NeoPixel.h> // Include the Adafruit NeoPixel library
#include "USB.h"
#include "USBMIDI.h"
#include <array>
#include <vector>

USBMIDI MIDI;

#define MIN_BRIGHTNESS 20 // Minimum brightness to ensure visibility
#define RGB_BRIGHTNESS 255 // Maximum brightness level
#define DATA_PIN 6 // Define the data pin for the NeoPixel strip (change as needed)

const float LEN_KEYBOARD = 1.5; // Length of the keyboard in meters
const int NUM_LEDS = 60; // Number of LEDs in the circuit

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800); // Initialize NeoPixel strip

// Define colors for 128 MIDI notes, with 88 keys (21-108) evenly spaced between Red and Violet
const int NUM_MIDI_NOTES = 128;
const int NUM_COLORS = 7;
std::array<uint32_t, NUM_MIDI_NOTES> colors;

// Color steps (RGB values)
const uint32_t colorSteps[NUM_COLORS] = {
    0xFF0000, // Red
    0xFF7F00, // Orange
    0xFFFF00, // Yellow
    0x00FF00, // Green
    0x0000FF, // Blue
    0x4B0082, // Indigo
    0x9400D3  // Violet
};

// Vector to store the state of each LED
std::vector<uint32_t> ledStates(NUM_LEDS, 0);

// Function to interpolate between two colors
uint32_t interpolateColor(uint32_t color1, uint32_t color2, float ratio) {
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;

    uint8_t r = r1 * (1 - ratio) + r2 * ratio;
    uint8_t g = g1 * (1 - ratio) + g2 * ratio;
    uint8_t b = b1 * (1 - ratio) + b2 * ratio;

    return (r << 16) | (g << 8) | b;
}

// Function to map MIDI note to LED index
int mapNoteToLED(int note) {
    if (note < 21 || note > 108) return -1; // Out of range
    float notePosition = (note - 21) / 87.0; // Normalize to 0-1 range
    return int(notePosition * NUM_LEDS);
}

void setup() {
    Serial.begin(115200);
    MIDI.begin();
    USB.begin();

    // Initialize NeoPixel strip
    strip.begin();
    strip.show(); // Clear all LEDs initially

    // Initialize color mapping
    for (int i = 0; i < NUM_MIDI_NOTES; i++) {
        if (i < 21 || i > 108) {
            colors[i] = 0; // Black for notes outside 88-key range
        } else {
            int colorIndex = ((i - 21) * (NUM_COLORS - 1)) / (108 - 21);
            float ratio = ((i - 21) % ((108 - 21) / (NUM_COLORS - 1))) / float((108 - 21) / (NUM_COLORS - 1));
            colors[i] = interpolateColor(colorSteps[colorIndex], colorSteps[(colorIndex + 1) % NUM_COLORS], ratio);
        }
    }
}

void loop() {
    midiEventPacket_t midi_packet_in = {0, 0, 0, 0};

    if (MIDI.readPacket(&midi_packet_in)) {
        printDetails(midi_packet_in);
        updateLEDs(midi_packet_in);
    }
}

void printDetails(midiEventPacket_t &midi_packet_in) {
    uint8_t cable_num = MIDI_EP_HEADER_CN_GET(midi_packet_in.header);
    midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);

    Serial.println("Received a USB MIDI packet:");
    Serial.printf("| CN: %d | CIN: %X | STATUS: %X | DATA 0: %X | DATA 1: %X |\n", cable_num, code_index_num, midi_packet_in.byte1, midi_packet_in.byte2, midi_packet_in.byte3);
}

void updateLEDs(midiEventPacket_t &midi_packet_in) {
    midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);
    uint8_t note = midi_packet_in.byte2;
    uint8_t velocity = midi_packet_in.byte3;

    int ledIndex = mapNoteToLED(note);
    if (ledIndex == -1) {
        Serial.printf("Note %d is out of range and will not light any LED.\n", note);
        return;
    }

    uint32_t color = colors[note];

    if (code_index_num == MIDI_CIN_NOTE_ON && velocity > 0) {
        uint8_t brightness = map(velocity, 1, 127, MIN_BRIGHTNESS, RGB_BRIGHTNESS);
        uint8_t r = (((color >> 16) & 0xFF) * brightness / RGB_BRIGHTNESS);
        uint8_t g = (((color >> 8) & 0xFF) * brightness / RGB_BRIGHTNESS);
        uint8_t b = ((color & 0xFF) * brightness / RGB_BRIGHTNESS);

        ledStates[ledIndex] = (r << 16) | (g << 8) | b;
        Serial.printf("Note On: %d -> LED: %d, Color: RGB(%d, %d, %d), Brightness: %d\n", note, ledIndex, r, g, b, brightness);

        // Update surrounding LEDs with diminishing brightness
        for (int i = 1; i <= 2; i++) {
            if (ledIndex - i >= 0) updateSurroundingLED(ledIndex - i, color, brightness, i);
            if (ledIndex + i < NUM_LEDS) updateSurroundingLED(ledIndex + i, color, brightness, i);
        }
    } else if (code_index_num == MIDI_CIN_NOTE_OFF || (code_index_num == MIDI_CIN_NOTE_ON && velocity == 0)) {
        ledStates[ledIndex] = 0;
        Serial.printf("Note Off: %d -> LED: %d turned off.\n", note, ledIndex);

        // Turn off surrounding LEDs
        for (int i = 1; i <= 2; i++) {
            if (ledIndex - i >= 0) ledStates[ledIndex - i] = 0;
            if (ledIndex + i < NUM_LEDS) ledStates[ledIndex + i] = 0;
        }
    }

    // Update all LEDs on the strip
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t state = ledStates[i];
        strip.setPixelColor(i, (state >> 16) & 0xFF, (state >> 8) & 0xFF, state & 0xFF); // Set RGB values
    }
    strip.show(); // Refresh LEDs with new colors
}

void updateSurroundingLED(int ledIndex, uint32_t color, uint8_t brightness, int distance) {
    float dimFactor = 1.0 / (distance + 1);
    uint8_t dimmedBrightness = brightness * dimFactor;

    uint8_t r = (((color >> 16) & 0xFF) * dimmedBrightness / RGB_BRIGHTNESS);
    uint8_t g = (((color >> 8) & 0xFF) * dimmedBrightness / RGB_BRIGHTNESS);
    uint8_t b = ((color & 0xFF) * dimmedBrightness / RGB_BRIGHTNESS);

    ledStates[ledIndex] = (r << 16) | (g << 8) | b;
    Serial.printf("Surrounding LED: %d, Color: RGB(%d, %d, %d), Dimming Factor: %.2f\n", ledIndex, r, g, b, dimFactor);
}
//hello
#endif