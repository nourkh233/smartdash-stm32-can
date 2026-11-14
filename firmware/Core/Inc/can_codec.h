#pragma once
#include <stdint.h>

#define ID_SPEED   0x110u
#define ID_DIST    0x120u
#define ID_CURRENT 0x130u
#define ID_ENV     0x140u
#define ID_HB_A    0x7F0u
#define ID_HB_B    0x7F1u

typedef struct { uint16_t id; uint8_t dlc; uint8_t data[8]; } sd_frame_t;

sd_frame_t codec_speed(float kmh);
sd_frame_t codec_dist(float mm);
sd_frame_t codec_current(float amps);
sd_frame_t codec_temp(float temp_c);
sd_frame_t codec_hb(uint16_t id, uint8_t counter, uint8_t state);
