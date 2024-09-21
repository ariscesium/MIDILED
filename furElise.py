import mido
import time

# Für Elise MIDI notes (more complete version with timings)
fur_elise_notes = [
    (76, 64, 0.4),  # E
    (75, 64, 0.4),  # D#
    (76, 64, 0.4),  # E
    (75, 64, 0.4),  # D#
    (76, 64, 0.4),  # E
    (71, 64, 0.4),  # B
    (74, 64, 0.4),  # D
    (72, 64, 0.4),  # C
    (69, 64, 2),  # A (longer note)
    (60, 64, 0.4),  # C (lower octave)
    (64, 64, 0.4),  # E
    (69, 64, 0.4),  # A (longer note)
    (71, 64, 2),  # B (longer note)
    (64, 64, 0.4),  # E
    (68, 64, 0.4),  # G#
    (71, 64, 0.4),  # B
    (72, 64, 2),  # C (longer note)
]

def play_fur_elise(output):
    for note, velocity, duration in fur_elise_notes:
        if note == 0:  # Rest
            time.sleep(duration)
        else:
            # Note on
            output.send(mido.Message('note_on', note=note, velocity=velocity))
            time.sleep(duration)
            
            # Note off
            output.send(mido.Message('note_off', note=note, velocity=0))
            
        time.sleep(0.01)  # Small delay between notes for clarity

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
            print("Playing Für Elise...")
            play_fur_elise(output)
            print("Finished playing.")
    else:
        print("\nESP32 TinyUSB MIDI device not found. Please make sure it's connected and recognized by your system.")

if __name__ == "__main__":
    main()