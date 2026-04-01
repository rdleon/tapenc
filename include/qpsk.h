#ifndef QPSK_H
#define QPSK_H

#include <stdint.h>
#include <stdlib.h>

/**
 * QPSK (Quadrature Phase Shift Keying) modulator/demodulator
 * for encoding binary data as audio signals suitable for cassette tapes
 */

#define QPSK_SAMPLES_PER_SYMBOL 160  /* samples per QPSK symbol at 16kHz */
#define QPSK_CARRIER_FREQ 1200       /* carrier frequency in Hz */
#define QPSK_SAMPLE_RATE 16000       /* audio sample rate in Hz */

typedef struct {
    float i;  /* in-phase component */
    float q;  /* quadrature component */
} qpsk_symbol_t;

/**
 * Initialize QPSK modulator
 * @param sample_rate: audio sample rate in Hz
 * @param carrier_freq: carrier frequency in Hz
 * @param samples_per_symbol: samples per transmitted symbol
 */
void qpsk_init(uint32_t sample_rate, uint32_t carrier_freq, uint32_t samples_per_symbol);

/**
 * Encode binary data to QPSK modulated audio samples
 * @param input: input binary data (bits packed in bytes)
 * @param input_len: length of input in bytes
 * @param output: output audio samples (float array)
 * @param output_len: pointer to store output sample count
 * @return: 0 on success, -1 on error
 */
int qpsk_encode(uint8_t *input, size_t input_len, float *output, size_t *output_len);

/**
 * Decode QPSK modulated audio samples to binary data
 * @param input: input audio samples (float array)
 * @param input_len: length of input samples
 * @param output: output binary data (bits packed in bytes)
 * @param output_len: pointer to store output byte count
 * @return: 0 on success, -1 on error
 */
int qpsk_decode(float *input, size_t input_len, uint8_t *output, size_t *output_len);

#endif /* QPSK_H */
