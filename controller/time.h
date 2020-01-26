#ifndef TIME_H
#define TIME_H

// Max possible value for time measurement
// https://www.pjrc.com/teensy/td_timing_millis.html
static const uint32_t TIME_MAX = 4294967295;

// Value for debouncing in microseconds
static const uint32_t TIME_DEBOUNCE = 5000;

// store current time every loop for velocity calculations
static int current_time = 0;
// time of last loop, needed for timer rollover
static int last_time = 0;

#endif
