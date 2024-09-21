#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBMIDI.h"
#include <array>

USBMIDI MIDI;

#ifdef RGB_BUILTIN
#define RGB_BRIGHTNESS 255
#define MIN_BRIGHTNESS 20 // Minimum brightness to ensure visibility
#endif

// Define colors for 88 keys, evenly spaced between Red and Violet
const int NUM_KEYS = 88;
const int NUM_COLORS = 7;
std::array<uint32_t, NUM_KEYS> colors;

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

void setup() {
    Serial.begin(115200);
    MIDI.begin();
    USB.begin();

    #ifdef RGB_BUILTIN
    pinMode(RGB_BUILTIN, OUTPUT);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Turn off LED initially
    #endif

    // Initialize color mapping
    for (int i = 0; i < NUM_KEYS; i++) {
        int colorIndex = (i * (NUM_COLORS - 1)) / (NUM_KEYS - 1);
        float ratio = (i % (NUM_KEYS / NUM_COLORS)) / float(NUM_KEYS / NUM_COLORS);
        colors[i] = interpolateColor(colorSteps[colorIndex], colorSteps[(colorIndex + 1) % NUM_COLORS], ratio);
    }
}

void loop() {
    midiEventPacket_t midi_packet_in = {0, 0, 0, 0};

    if (MIDI.readPacket(&midi_packet_in)) {
        printDetails(midi_packet_in);
        updateLED(midi_packet_in);
    }
}

void printDetails(midiEventPacket_t &midi_packet_in) {
  // See Chapter 4: USB-MIDI Event Packets (page 16) of the spec.
  uint8_t cable_num = MIDI_EP_HEADER_CN_GET(midi_packet_in.header);
  midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);

  Serial.println("Received a USB MIDI packet:");
  Serial.println(".----.-----.--------.--------.--------.");
  Serial.println("| CN | CIN | STATUS | DATA 0 | DATA 1 |");
  Serial.println("+----+-----+--------+--------+--------+");
  Serial.printf("| %d  |  %X  |   %X   |   %X   |   %X   |\n", cable_num, code_index_num, midi_packet_in.byte1, midi_packet_in.byte2, midi_packet_in.byte3);
  Serial.println("'----'-----'--------.--------'--------'\n");
  Serial.print("Description: ");

  switch (code_index_num) {
    case MIDI_CIN_MISC:        Serial.println("This is a Miscellaneous event"); break;
    case MIDI_CIN_CABLE_EVENT: Serial.println("This is a Cable event"); break;
    case MIDI_CIN_SYSCOM_2BYTE:
    case MIDI_CIN_SYSCOM_3BYTE: Serial.println("This is a System Common (SysCom) event"); break;
    case MIDI_CIN_SYSEX_START:
    case MIDI_CIN_SYSEX_END_1BYTE:
    case MIDI_CIN_SYSEX_END_2BYTE:
    case MIDI_CIN_SYSEX_END_3BYTE: Serial.println("This is a system exclusive (SysEx) event"); break;
    case MIDI_CIN_NOTE_ON: Serial.printf("This is a Note-On event of Note %d with a Velocity of %d\n", midi_packet_in.byte2, midi_packet_in.byte3); break;
    case MIDI_CIN_NOTE_OFF: Serial.printf("This is a Note-Off event of Note %d with a Velocity of %d\n", midi_packet_in.byte2, midi_packet_in.byte3); break;
    case MIDI_CIN_POLY_KEYPRESS: Serial.printf("This is a Poly Aftertouch event for Note %d and Value %d\n", midi_packet_in.byte2, midi_packet_in.byte3); break;
    case MIDI_CIN_CONTROL_CHANGE: Serial.printf("This is a Control Change event of Controller %d with a Value of %d\n", midi_packet_in.byte2, midi_packet_in.byte3); break;
    case MIDI_CIN_PROGRAM_CHANGE: Serial.printf("This is a Program Change event with a Value of %d\n", midi_packet_in.byte2); break;
    case MIDI_CIN_CHANNEL_PRESSURE: Serial.printf("This is a Channel Pressure event with a Value of %d\n", midi_packet_in.byte2); break;
    case MIDI_CIN_PITCH_BEND_CHANGE: Serial.printf("This is a Pitch Bend Change event with a Value of %d\n", ((uint16_t)midi_packet_in.byte2) << 7 | midi_packet_in.byte3); break;
    case MIDI_CIN_1BYTE_DATA: Serial.printf("This is an embedded Serial MIDI event byte with Value %X\n", midi_packet_in.byte1); break;
  }

  Serial.println();
}

void updateLED(midiEventPacket_t &midi_packet_in) {
    midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);
    uint8_t note = midi_packet_in.byte2;
    uint8_t velocity = midi_packet_in.byte3;
    
    if (note >= 21 && note <= 108) { // Standard 88-key range
        int colorIndex = note - 21;
        uint32_t color = colors[colorIndex];
        
        if (code_index_num == MIDI_CIN_NOTE_ON && velocity > 0) {
            // Map velocity to brightness, ensuring a minimum brightness
            uint8_t brightness = map(velocity, 1, 127, MIN_BRIGHTNESS, RGB_BRIGHTNESS);
            
            uint8_t r = (((color >> 16) & 0xFF) * brightness / RGB_BRIGHTNESS);
            uint8_t g = (((color >> 8) & 0xFF) * brightness / RGB_BRIGHTNESS);
            uint8_t b = ((color & 0xFF) * brightness / RGB_BRIGHTNESS);
            
            #ifdef RGB_BUILTIN
            neopixelWrite(RGB_BUILTIN, r, g, b);
            #endif
            
            Serial.printf("Note On: %d, Velocity: %d, Color: #%06X, Brightness: %d\n", note, velocity, color, brightness);
        } else if (code_index_num == MIDI_CIN_NOTE_OFF || (code_index_num == MIDI_CIN_NOTE_ON && velocity == 0)) {
            // Turn off LED for Note Off or Note On with velocity 0
            #ifdef RGB_BUILTIN
            neopixelWrite(RGB_BUILTIN, 0, 0, 0);
            #endif
            
            Serial.printf("Note Off: %d\n", note);
        }
    }
}

#endif