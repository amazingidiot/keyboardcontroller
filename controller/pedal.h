#ifndef PEDAL_H
#define PEDAL_H

// Pedals
static const PEDAL_COUNT = 3;

enum pedalmode {
  SINGLE,
  DOUBLE,
  ANALOG
}

typedef struct {
  uint8_t pin_tip = 0;
  uint8_t pin_ring = 0;
  uint8_t pin_ground = 0;

  uint8_t midi_cc = 64;

  uint8_t midi_min_value = 0;
  uint8_t midi_max_value = 127;

  uint16_t ad_min_value = 0;
  uint16_t ad_max_value = 1023;

  pedalmode mode = SINGLE;
} pedal_t;

static pedal_t pedals[PEDAL_COUNT];

#endif
