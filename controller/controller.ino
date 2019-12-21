// Max possible value for time measurement
// https://www.pjrc.com/teensy/td_timing_millis.html
const uint32_t TIME_MAX = 4294967295;

// Value for debouncing
const uint32_t TIME_DEBOUNCE = 50000;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
const uint32_t TIME_NOTEON_MIN = 30;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
const uint32_t TIME_NOTEON_MAX = 60000;

// precalculate timing range
const uint32_t TIME_NOTEON_RANGE = TIME_NOTEON_MAX - TIME_NOTEON_MIN;

// delay after digitalWrite() during scan_matrix()
const uint32_t TIME_DELAY_SCANMATRIX = 2;

// Curve for the velocity
const float VELOCITY_CURVE_FACTOR = 3.0f;

// T-Lines, see http://www.doepfer.de/DIY/Matrix_88.gif
const uint8_t COUNT_T = 8;
// MK- and BR-Lines
const uint8_t COUNT_MKBR = 11;

// Number of keys
const uint8_t KEY_COUNT = 88;

// MIDI-channel for all MIDI data
const uint8_t MIDI_CHANNEL = 1;
// key 0 is MIDI key 21
const uint8_t MIDI_KEY_OFFSET = 21;
// resolution of MIDI High resolution
const uint16_t MIDI_HIGHRES_RESOLUTION = 16348;
// Extended values for MIDI high resolution are send on controller 88
const uint8_t MIDI_HIGHRES_CC = 88;

typedef struct {
    uint32_t time;
    uint32_t debounce;

    bool trigger_0 = false; // BR-Line
    bool trigger_1 = false; // MK-Line

    bool prev_trigger_0 = false; // previous value of trigger_0
    bool prev_trigger_1 = false; // previous value of trigger_1

} keyboard_key_t;

// Pins used for T-Lines
const uint8_t T[COUNT_T] = { 23, 22, 21, 20,
    19, 18, 17, 16 };
// Pins used for MK-Lines
const uint8_t MK[COUNT_MKBR] = { 0, 2, 4, 6, 8, 10,
    12, 25, 27, 29, 32 };
// Pins used for BR-Lines
const uint8_t BR[COUNT_MKBR] = { 1, 3, 5, 7, 9, 11,
    24, 26, 28, 31, 30 };

keyboard_key_t keys[KEY_COUNT];

// store current time every loop for velocity calculations
int current_time = 0;
// time of last loop, needed for timer rollover
int last_time = 0;

void scan_matrix()
{
    int current_key = 0;

    for (int MKBR_LINE = 0; MKBR_LINE < COUNT_MKBR; MKBR_LINE++) {
        for (int T_LINE = 0; T_LINE < COUNT_T; T_LINE++) {
            // Set T-Line HIGH
            digitalWrite(T[T_LINE], HIGH);
            delayMicroseconds(TIME_DELAY_SCANMATRIX);
            // Read MK-Line
            keys[current_key].trigger_0 = digitalRead(BR[MKBR_LINE]);

            // Read BR-Line
            keys[current_key].trigger_1 = digitalRead(MK[MKBR_LINE]);

            // Set T-Line DOWN
            digitalWrite(T[T_LINE], LOW);

            delayMicroseconds(TIME_DELAY_SCANMATRIX);
            current_key++;
        }
    }
}

inline void save_key_time(int key) { keys[key].time = current_time; }

inline void save_time_difference(int key)
{
    // Handle rollover of timer
    if (current_time < last_time) {
        keys[key].time = constrain(TIME_MAX - keys[key].time + current_time,
                             TIME_NOTEON_MIN, TIME_NOTEON_MAX)
            - TIME_NOTEON_MIN;
    } else {
        keys[key].time = constrain(current_time - keys[key].time,
                             TIME_NOTEON_MIN, TIME_NOTEON_MAX)
            - TIME_NOTEON_MIN;
    }
}

uint16_t calculate_midi_noteon_velocity(uint8_t key)
{
    float key_press_time = keys[key].time - TIME_NOTEON_MIN;

    if (key_press_time < 0.0f)
        key_press_time = 0.0f;
    if (key_press_time > TIME_NOTEON_RANGE)
        key_press_time = (float)TIME_NOTEON_RANGE;

    float midi_velocity = 1.0f - key_press_time / (float)(TIME_NOTEON_RANGE); // Value between 0.0f and 1.0f

    midi_velocity = pow(midi_velocity, VELOCITY_CURVE_FACTOR);

    return (uint16_t)(midi_velocity * (MIDI_HIGHRES_RESOLUTION - 128) + 128);
}

uint8_t calculate_midi_noteoff_velocity(uint8_t key)
{
    float key_press_time = keys[key].time - TIME_NOTEON_MIN;

    if (key_press_time < 0.0f)
        key_press_time = 0.0f;
    if (key_press_time > TIME_NOTEON_RANGE)
        key_press_time = (float)TIME_NOTEON_RANGE;

    float midi_velocity = 1.0f - key_press_time / (float)(TIME_NOTEON_RANGE); // Value between 0.0f and 1.0f

    midi_velocity = pow(midi_velocity, VELOCITY_CURVE_FACTOR);

    return (uint8_t)(midi_velocity * 126 + 1);
}

void setup()
{
    for (int i = 0; i < COUNT_T; i++) {
        pinMode(T[i], OUTPUT);
        digitalWrite(T[i], LOW);
    }

    for (int i = 0; i < COUNT_MKBR; i++) {
        pinMode(MK[i], INPUT_PULLDOWN);
        pinMode(BR[i], INPUT_PULLDOWN);
    }
}

void loop()
{
    current_time = micros();

    scan_matrix();

#define NORMAL_MODE
#ifdef SIMPLE_MODE
    for (int i = 0; i < KEY_COUNT; i++) {
        if (keys[i].trigger_1 && !keys[i].prev_trigger_1) {
            usbMIDI.sendNoteOn(i + MIDI_KEY_OFFSET, 64, MIDI_CHANNEL);
        }
        if (!keys[i].trigger_1 && keys[i].prev_trigger_1) {
            usbMIDI.sendNoteOff(i + MIDI_KEY_OFFSET, 64, MIDI_CHANNEL);
        }

        keys[i].prev_trigger_1 = keys[i].trigger_1;
    }
#endif

#ifdef NORMAL_MODE
    for (int i = 0; i < KEY_COUNT; i++) {
        if (!keys[i].trigger_0 && !keys[i].trigger_1) {
            // Both triggers off
            if (keys[i].prev_trigger_0) {
                // trigger_0 is now off
                if (keys[i].debounce < (current_time - TIME_DEBOUNCE)) {
                    save_time_difference(i);

                    usbMIDI.sendNoteOff(i + MIDI_KEY_OFFSET, calculate_midi_noteoff_velocity(i),
                        MIDI_CHANNEL);
                }
            }
        }

        else if (keys[i].trigger_0 && !keys[i].trigger_1) { // trigger_0 pressed
            if (!keys[i].prev_trigger_0) {
                // trigger_0 is now pressed, key is going down.
                if (keys[i].debounce < (current_time - TIME_DEBOUNCE)) {
                    save_key_time(i);
                }
            }
            if (keys[i].prev_trigger_1) {
                // trigger_1 is now off, key is going up
                if (keys[i].debounce < (current_time - TIME_DEBOUNCE)) {
                    save_key_time(i);
                }
            }
        }

        else if (keys[i].trigger_0 && keys[i].trigger_1) {
            if (!keys[i].prev_trigger_1) {
                // trigger_0 and trigger_1 are now pressed
                if (keys[i].debounce < (current_time - TIME_DEBOUNCE)) {
                    keys[i].debounce = current_time;

                    save_time_difference(i);

                    uint16_t velocity = calculate_midi_noteon_velocity(i);

                    usbMIDI.sendControlChange(MIDI_HIGHRES_CC, (uint8_t)(velocity & 0x7f), MIDI_CHANNEL);
                    usbMIDI.sendNoteOn(i + MIDI_KEY_OFFSET, (uint8_t)(velocity >> 7),
                        MIDI_CHANNEL);
                }
            }
        }

        keys[i].prev_trigger_0 = keys[i].trigger_0;
        keys[i].prev_trigger_1 = keys[i].trigger_1;
    }
#endif

    last_time = current_time;
}
