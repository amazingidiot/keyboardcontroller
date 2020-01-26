#include "time.h"
#include "keyboard.h"
#include "pedal.h"

// MIDI-channel for all MIDI data
static const uint8_t MIDI_CHANNEL = 1;

void setup()
{
    keyboard_setup();
    pedal_setup();
}

void loop()
{
    current_time = micros();

    keyboard_loop();

    last_time = current_time;
}
