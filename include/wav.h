#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <stdio.h>

/**
 * WAV file I/O for audio samples
 * Supports 16-bit PCM mono audio
 */

typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t num_samples;
} wav_header_t;

/**
 * Write audio samples to a WAV file
 * @param filename: output filename
 * @param samples: audio samples (float array, range -1.0 to 1.0)
 * @param num_samples: number of samples
 * @param sample_rate: sample rate in Hz
 * @return: 0 on success, -1 on error
 */
int wav_write(const char *filename, const float *samples, uint32_t num_samples, uint32_t sample_rate);

/**
 * Read audio samples from a WAV file
 * @param filename: input filename
 * @param samples: pointer to store audio samples (allocated by this function)
 * @param num_samples: pointer to store number of samples read
 * @param header: pointer to store WAV header info
 * @return: 0 on success, -1 on error
 */
int wav_read(const char *filename, float **samples, uint32_t *num_samples, wav_header_t *header);

/**
 * Free memory allocated by wav_read
 */
void wav_free(float *samples);

#endif /* WAV_H */
