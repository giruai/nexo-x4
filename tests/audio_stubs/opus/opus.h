/* Deliberately small Opus double API; the product still includes real Opus. */
#pragma once
#include <stdint.h>
typedef int32_t opus_int32;
typedef struct OpusDecoder { unsigned resets, calls, plc_calls; } OpusDecoder;
enum { OPUS_RESET_STATE = 4028 };
OpusDecoder *opus_decoder_create(int, int, int *);
void opus_decoder_destroy(OpusDecoder *);
int opus_decoder_ctl(OpusDecoder *, int, ...);
int opus_decode(OpusDecoder *, const unsigned char *, opus_int32, int16_t *, int, int);
int opus_packet_get_nb_samples(const unsigned char *, opus_int32, int);
