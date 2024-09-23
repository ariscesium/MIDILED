import mido
import time

# MIDI notes from A0 (21) to C8 (108) with a human-like timing effect
flourish_notes = [(note, 64, max(0.02, 0.06 - note * 0.0005)) for note in range(21, 109)]

def play_flourish(output):
    for note, velocity, duration in flourish_notes:
        # Note on
        output.send(mido.Message('note_on', note=note, velocity=velocity))
        time.sleep(duration)
        
        # Note off
        output.send(mido.Message('note_off', note=note, velocity=0))
        
        time.sleep(0.01)  # Small delay between notes for a more natural feel

def main():
    # List available output ports
    print("Available MIDI output ports:")
    for port in mido.get_output_names():
        print(f"- {port}")

    # Look for the ESP32 TinyUSB MIDI device
    esp32_port = None
    for port in mido.get_output_names():
        if "TinyUSB MIDI" in port:
            esp32_port = port
            break

    if esp32_port:
        print(f"\nFound ESP32 TinyUSB MIDI device: {esp32_port}")
        with mido.open_output(esp32_port) as output:
            print("Playing flourish...")
            play_flourish(output)
            print("Finished playing.")
    else:
        print("\nESP32 TinyUSB MIDI device not found. Please make sure it's connected and recognized by your system.")

if __name__ == "__main__":
    main()
