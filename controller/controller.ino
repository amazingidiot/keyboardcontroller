// Max possible value for time measurement
// https://www.pjrc.com/teensy/td_timing_millis.html
static const uint32_t TIME_MAX = 4294967295;

// Value for debouncing in microseconds
static const uint32_t TIME_DEBOUNCE = 5000;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEON_TIME_MIN = 20;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEON_TIME_MAX = 120000;

// precalculate timing range
static const uint32_t NOTEON_TIME_RANGE = NOTEON_TIME_MAX - NOTEON_TIME_MIN;

// Curve for the noteon velocity
static const float NOTEON_VELOCITY_CURVE = 4.0f;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEOFF_TIME_MIN = 8000;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEOFF_TIME_MAX = 150000;

// precalculate timing range
static const uint32_t NOTEOFF_TIME_RANGE = NOTEOFF_TIME_MAX - NOTEOFF_TIME_MIN;

// Curve for the noteon velocity
static const float NOTEOFF_VELOCITY_CURVE = 1.0f;

// delay after digitalWrite() during scan_matrix()
static const uint32_t TIME_DELAY_SCANMATRIX = 2;

// T-Lines, see http://www.doepfer.de/DIY/Matrix_88.gif
static const uint8_t COUNT_T = 8;
// MK- and BR-Lines
static const uint8_t COUNT_MKBR = 11;

// Number of keys
static const uint8_t KEY_COUNT = 88;

// MIDI-channel for all MIDI data
static const uint8_t MIDI_CHANNEL = 1;
// key 0 is MIDI key 21
static const uint8_t MIDI_KEY_OFFSET = 21;
// resolution of MIDI High resolution
static const uint16_t MIDI_HIGHRES_RESOLUTION = 16256;
// Extended values for MIDI high resolution are send on controller 88
static const uint8_t MIDI_HIGHRES_CC = 88;

enum keystate {
    UP,
    GOING_DOWN,
    DOWN,
    GOING_UP
};

typedef struct {
    // time between switch activations
    uint32_t time = 0;

    // Debounce-times
    uint32_t debounce_0 = 0;
    uint32_t debounce_1 = 0;

    keystate state = UP;

    // BR-Line, upper switches
    bool trigger_0 = false;
    // MK-Line, lower switches
    bool trigger_1 = false;

    // previous value of trigger_0
    bool prev_trigger_0 = false;
    // previous value of trigger_1
    bool prev_trigger_1 = false;
} keyboard_key_t;

// Pins used for T-Lines
static const uint8_t T[COUNT_T] = { 23, 22, 21, 20,
    19, 18, 17, 16 };
// Pins used for MK-Lines
static const uint8_t MK[COUNT_MKBR] = { 0, 2, 4, 6, 8, 10,
    12, 25, 27, 29, 32 };
// Pins used for BR-Lines
static const uint8_t BR[COUNT_MKBR] = { 1, 3, 5, 7, 9, 11,
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

inline void save_key_time(int key)
{
    keys[key].time = current_time;
}

inline void save_time_difference(int key, uint32_t min, uint32_t max)
{
    // Handle rollover of timer
    if (current_time < last_time) {
        keys[key].time = constrain(TIME_MAX - keys[key].time + current_time, min, max) - min;
    } else {
        keys[key].time = constrain(current_time - keys[key].time, min, max) - min;
    }
}

uint16_t calculate_midi_velocity(uint8_t key, float velocity_curve, uint32_t time_range)
{
    float midi_velocity = 1.0f - (float)(keys[key].time) / (float)(time_range); // Value between 0.0f and 1.0f

    midi_velocity = pow(midi_velocity, velocity_curve);

    return (uint16_t)(midi_velocity * (MIDI_HIGHRES_RESOLUTION) + 128);
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

    for (int key = 0; key < KEY_COUNT; key++) {
        // Debounce and process keys

        if (keys[key].debounce_0 < (current_time - TIME_DEBOUNCE)) {
            if (keys[key].trigger_0 && !keys[key].prev_trigger_0 && keys[key].state == UP) {
                // trigger_0 is pressed, key is going down
                keys[key].prev_trigger_0 = true;
                keys[key].debounce_0 = current_time;
                keys[key].state = GOING_DOWN;

                save_key_time(key);
            } else if (!keys[key].trigger_0 && keys[key].prev_trigger_0 && keys[key].state == GOING_UP) {
                // trigger_0 is released, key is now up
                keys[key].prev_trigger_0 = false;
                keys[key].debounce_0 = current_time;

                keys[key].state = UP;

                save_time_difference(key, NOTEOFF_TIME_MIN, NOTEOFF_TIME_MAX);

                uint16_t velocity = calculate_midi_velocity(key, NOTEOFF_VELOCITY_CURVE, NOTEOFF_TIME_RANGE);

                usbMIDI.sendControlChange(MIDI_HIGHRES_CC, (uint8_t)(velocity & 0x7f), MIDI_CHANNEL);
                usbMIDI.sendNoteOff(key + MIDI_KEY_OFFSET, (uint8_t)(velocity >> 7), MIDI_CHANNEL);
            }
        }

        if (keys[key].debounce_1 < (current_time - TIME_DEBOUNCE)) {
            if (keys[key].trigger_1 && !keys[key].prev_trigger_1 && keys[key].state == GOING_DOWN) {
                // trigger_1 is pressed, key is now down
                keys[key].prev_trigger_1 = true;
                keys[key].debounce_1 = current_time;
                keys[key].state = DOWN;

                save_time_difference(key, NOTEON_TIME_MIN, NOTEON_TIME_MAX);

                uint16_t velocity = calculate_midi_velocity(key, NOTEON_VELOCITY_CURVE, NOTEON_TIME_RANGE);

                usbMIDI.sendControlChange(MIDI_HIGHRES_CC, (uint8_t)(velocity & 0x7f), MIDI_CHANNEL);
                usbMIDI.sendNoteOn(key + MIDI_KEY_OFFSET, (uint8_t)(velocity >> 7), MIDI_CHANNEL);
            } else if (!keys[key].trigger_1 && keys[key].prev_trigger_1 && keys[key].state == DOWN) {
                // trigger_1 is released, key is going up
                keys[key].prev_trigger_1 = false;
                keys[key].debounce_1 = current_time;
                keys[key].state = GOING_UP;

                save_key_time(key);
            }
        }
    }

    last_time = current_time;
}
