// TIME

// Max possible value for time measurement
// https://www.pjrc.com/teensy/td_timing_millis.html
static const uint32_t TIME_MAX = 4294967295;

// Value for debouncing in microseconds
static const uint32_t TIME_DEBOUNCE = 5000;

// store current time every loop for velocity calculations
static uint32_t current_time = 0;
// time of last loop, needed for timer rollover
static uint32_t last_time = 0;

// MIDI

// MIDI-channel for all MIDI data
static const uint8_t MIDI_CHANNEL = 1;
// key 0 is MIDI key 21
static const uint8_t MIDI_KEY_OFFSET = 21;
// resolution of MIDI High resolution
static const uint16_t MIDI_HIGHRES_RESOLUTION = 16256;
// Extended values for MIDI high resolution are send on controller 88
static const uint8_t MIDI_HIGHRES_CC = 88;

// KEYBOARD

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

// All possible states of a key
enum keystate { UP, GOING_DOWN, DOWN, GOING_UP };

typedef struct {
    // time between switch activations
    uint32_t time = 0;

    // debounce-times
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
static const uint8_t T[COUNT_T] = {23, 22, 21, 20, 19, 18, 17, 16};
// Pins used for MK-Lines
static const uint8_t MK[COUNT_MKBR] = {0, 2, 4, 6, 8, 10, 12, 25, 27, 29, 32};
// Pins used for BR-Lines
static const uint8_t BR[COUNT_MKBR] = {1, 3, 5, 7, 9, 11, 24, 26, 28, 31, 30};

static keyboard_key_t keys[KEY_COUNT];

// PEDALS

// Number of possible pedals
static const uint8_t PEDAL_COUNT = 3;

// Pedal modes
enum pedalmode {
    SINGLE,  // Single pedal, triggered by connecting tip and ground
    DOUBLE,  // Double pedal, tip and ring are different pedals
    ANALOG,  // Analog input
};

typedef struct {
    // Pins
    uint8_t pin_tip = 0;
    uint8_t pin_ring = 0;
    uint8_t pin_ground = 0;

    // previous values of the pins for single and double digital pedals
    bool prev_p0 = false;
    bool prev_p1 = false;

    // debounce timers
    uint32_t debounce_0 = 0;
    uint32_t debounce_1 = 0;

    // MIDI control change, midi_cc0 is also used for analog pedals
    uint8_t midi_cc0 = 64;
    uint8_t midi_cc1 = 65;

    // MIDI minimum and maximum to send, midi_min_value0 and midi_max_value0 are
    // also used for analog pedals
    uint8_t midi_min_value0 = 0;
    uint8_t midi_max_value0 = 127;

    uint8_t midi_min_value1 = 0;
    uint8_t midi_max_value1 = 127;

    // Analog reading minimum and maximum
    uint16_t analog_min_value = 0;
    uint16_t analog_max_value = 1023;
    uint16_t analog_current_value = 0;
    uint16_t analog_last_value = 0;

    // Mode
    pedalmode mode = SINGLE;
} pedal_t;

static pedal_t pedals[PEDAL_COUNT] = {
    {.pin_tip = 33, .pin_ring = 34, .pin_ground = 35},
    {.pin_tip = 36, .pin_ring = 37, .pin_ground = 38},
    {.pin_tip = 39, .pin_ring = 14, .pin_ground = 15}};

// Functions

inline void save_key_time(int key) { keys[key].time = current_time; }

inline void save_time_difference(int key, uint32_t min, uint32_t max) {
    // Handle rollover of timer
    if (current_time < last_time) {
        keys[key].time =
            constrain(TIME_MAX - keys[key].time + current_time, min, max) - min;
    } else {
        keys[key].time =
            constrain(current_time - keys[key].time, min, max) - min;
    }
}

inline uint16_t calculate_midi_velocity(uint8_t key, float velocity_curve,
                                        uint32_t time_range) {
    float midi_velocity =
        1.0f - (float)(keys[key].time) /
                   (float)(time_range);  // Value between 0.0f and 1.0f

    midi_velocity = pow(midi_velocity, velocity_curve);

    return (uint16_t)(midi_velocity * (MIDI_HIGHRES_RESOLUTION) + 128);
}

void setup_pedal(pedal_t* pedal) {
    switch (pedal->mode) {
        case (SINGLE): {
            pinMode(pedal->pin_tip, INPUT_PULLDOWN);
            pinMode(pedal->pin_ring, INPUT);
            pinMode(pedal->pin_ground, OUTPUT);

            digitalWrite(pedal->pin_ground, HIGH);
        }; break;
        case (DOUBLE): {
            pinMode(pedal->pin_tip, INPUT_PULLDOWN);
            pinMode(pedal->pin_ring, INPUT_PULLDOWN);
            pinMode(pedal->pin_ground, OUTPUT);

            digitalWrite(pedal->pin_ground, HIGH);
        }; break;

        case (ANALOG): {
            pinMode(pedal->pin_tip, OUTPUT);
            pinMode(pedal->pin_ring, INPUT);
            pinMode(pedal->pin_ground, INPUT);

            digitalWrite(pedal->pin_tip, HIGH);
        }; break;
    }
}

void setup() {
    for (int i = 0; i < COUNT_T; i++) {
        pinMode(T[i], OUTPUT);
        digitalWrite(T[i], LOW);
    }

    for (int i = 0; i < COUNT_MKBR; i++) {
        pinMode(MK[i], INPUT_PULLDOWN);
        pinMode(BR[i], INPUT_PULLDOWN);
    }

    pedals[2].mode = SINGLE;
    pedals[2].midi_cc0 = 74;

    for (int pedal = 0; pedal < PEDAL_COUNT; pedal++) {
        setup_pedal(&pedals[pedal]);
    }
}

void loop() {
    current_time = micros();
    uint32_t current_debounce = current_time - TIME_DEBOUNCE;

    // Keyboard

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

    for (int key = 0; key < KEY_COUNT; key++) {
        // Debounce and process keys

        if (keys[key].debounce_0 < current_debounce) {
            if (keys[key].trigger_0 && !keys[key].prev_trigger_0 &&
                keys[key].state == UP) {
                // trigger_0 is pressed, key is going down
                keys[key].prev_trigger_0 = true;
                keys[key].debounce_0 = current_time;
                keys[key].state = GOING_DOWN;

                save_key_time(key);
            } else if (!keys[key].trigger_0 && keys[key].prev_trigger_0 &&
                       keys[key].state == GOING_UP) {
                // trigger_0 is released, key is now up
                keys[key].prev_trigger_0 = false;
                keys[key].debounce_0 = current_time;

                keys[key].state = UP;

                save_time_difference(key, NOTEOFF_TIME_MIN, NOTEOFF_TIME_MAX);

                uint16_t velocity = calculate_midi_velocity(
                    key, NOTEOFF_VELOCITY_CURVE, NOTEOFF_TIME_RANGE);

                usbMIDI.sendControlChange(
                    MIDI_HIGHRES_CC, (uint8_t)(velocity & 0x7f), MIDI_CHANNEL);
                usbMIDI.sendNoteOff(key + MIDI_KEY_OFFSET,
                                    (uint8_t)(velocity >> 7), MIDI_CHANNEL);
            }
        }

        if (keys[key].debounce_1 < current_debounce) {
            if (keys[key].trigger_1 && !keys[key].prev_trigger_1 &&
                keys[key].state == GOING_DOWN) {
                // trigger_1 is pressed, key is now down
                keys[key].prev_trigger_1 = true;
                keys[key].debounce_1 = current_time;
                keys[key].state = DOWN;

                save_time_difference(key, NOTEON_TIME_MIN, NOTEON_TIME_MAX);

                uint16_t velocity = calculate_midi_velocity(
                    key, NOTEON_VELOCITY_CURVE, NOTEON_TIME_RANGE);

                usbMIDI.sendControlChange(
                    MIDI_HIGHRES_CC, (uint8_t)(velocity & 0x7f), MIDI_CHANNEL);
                usbMIDI.sendNoteOn(key + MIDI_KEY_OFFSET,
                                   (uint8_t)(velocity >> 7), MIDI_CHANNEL);
            } else if (!keys[key].trigger_1 && keys[key].prev_trigger_1 &&
                       keys[key].state == DOWN) {
                // trigger_1 is released, key is going up
                keys[key].prev_trigger_1 = false;
                keys[key].debounce_1 = current_time;
                keys[key].state = GOING_UP;

                save_key_time(key);
            }
        }
    }

    // Pedals

    for (int pedal = 0; pedal < PEDAL_COUNT; pedal++) {
        switch (pedals[pedal].mode) {
            case DOUBLE: {
                if (pedals[pedal].debounce_1 < current_debounce) {
                    if (digitalRead(pedals[pedal].pin_ring)) {
                        // Pedal pressed
                        if (!pedals[pedal].prev_p1) {
                            usbMIDI.sendControlChange(
                                MIDI_CHANNEL, pedals[pedal].midi_cc1,
                                pedals[pedal].midi_max_value1);
                            pedals[pedal].prev_p1 = true;
                            pedals[pedal].debounce_1 = current_time;
                        }

                    } else {
                        // Pedal not pressed
                        if (pedals[pedal].prev_p1) {
                            usbMIDI.sendControlChange(
                                MIDI_CHANNEL, pedals[pedal].midi_cc1,
                                pedals[pedal].midi_min_value1);
                            pedals[pedal].prev_p1 = false;
                            pedals[pedal].debounce_1 = current_time;
                        }
                    }
                }
            };  // Falltrough to SINGLE
            case SINGLE: {
                if (pedals[pedal].debounce_0 < current_debounce) {
                    if (digitalRead(pedals[pedal].pin_tip)) {
                        // Pedal pressed
                        if (!pedals[pedal].prev_p0) {
                            usbMIDI.sendControlChange(
                                pedals[pedal].midi_cc0,
                                pedals[pedal].midi_max_value0, MIDI_CHANNEL);
                            pedals[pedal].prev_p0 = true;
                            pedals[pedal].debounce_0 = current_time;
                        }

                    } else {
                        // Pedal not pressed
                        if (pedals[pedal].prev_p0) {
                            usbMIDI.sendControlChange(
                                pedals[pedal].midi_cc0,
                                pedals[pedal].midi_min_value0, MIDI_CHANNEL);
                            pedals[pedal].prev_p0 = false;
                            pedals[pedal].debounce_0 = current_time;
                        }
                    }
                }
            }; break;

            case ANALOG: {
                pedals[pedal].analog_current_value =
                    analogRead(pedals[pedal].pin_ring);

                if (pedals[pedal].analog_current_value !=
                    pedals[pedal].analog_last_value) {
                    usbMIDI.sendControlChange(
                        pedals[pedal].midi_cc0,
                        map(pedals[pedal].analog_current_value,
                            pedals[pedal].analog_min_value,
                            pedals[pedal].analog_max_value,
                            pedals[pedal].midi_min_value0,
                            pedals[pedal].midi_max_value0),
                        MIDI_CHANNEL);
                    pedals[pedal].analog_last_value =
                        pedals[pedal].analog_current_value;
                }
            }; break;
        }
    }

    last_time = current_time;

    while (usbMIDI.read()) {
        // Implement logic to set various parameters per MIDI
    }
}
