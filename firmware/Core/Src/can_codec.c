#include "can_codec.h"

static uint16_t clamp_u16(float x) {
    if (x < 0.0f) return 0;
    if (x > 65535.0f) return 65535;
    return (uint16_t)(x + 0.5f);
}
static int16_t clamp_i16(float x) {
    if (x > 32767.0f) return 32767;
    if (x < -32768.0f) return -32768;
    return (int16_t)(x >= 0 ? x + 0.5f : x - 0.5f);
}
static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }

sd_frame_t codec_speed(float kmh)  { sd_frame_t f = { ID_SPEED, 2, {0} };   put16(f.data, clamp_u16(kmh * 10.0f)); return f; }
sd_frame_t codec_dist(float mm)    { sd_frame_t f = { ID_DIST, 2, {0} };    put16(f.data, clamp_u16(mm));         return f; }
sd_frame_t codec_current(float a)  { sd_frame_t f = { ID_CURRENT, 2, {0} }; put16(f.data, (uint16_t)clamp_i16(a * 100.0f)); return f; }
sd_frame_t codec_temp(float t)     { sd_frame_t f = { ID_ENV, 2, {0} };     put16(f.data, (uint16_t)clamp_i16(t * 10.0f)); return f; }
sd_frame_t codec_hb(uint16_t id, uint8_t c, uint8_t s) { sd_frame_t f = { id, 2, { c, s } }; return f; }
