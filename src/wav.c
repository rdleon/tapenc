#include "wav.h"
#include <stdlib.h>
#include <string.h>

/* WAV file format structures */
typedef struct {
    char chunk_id[4];      /* "RIFF" */
    uint32_t chunk_size;
    char format[4];        /* "WAVE" */
} riff_header_t;

typedef struct {
    char subchunk_id[4];   /* "fmt " */
    uint32_t subchunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} fmt_subchunk_t;

typedef struct {
    char subchunk_id[4];   /* "data" */
    uint32_t subchunk_size;
} data_subchunk_t;

int wav_write(const char *filename, const float *samples, uint32_t num_samples, uint32_t sample_rate)
{
    if (!filename || !samples || num_samples == 0) return -1;
    
    FILE *file = fopen(filename, "wb");
    if (!file) return -1;
    
    uint32_t bytes_per_sample = 2;
    uint32_t byte_rate = sample_rate * 1 * bytes_per_sample;
    uint32_t data_size = num_samples * bytes_per_sample;
    
    /* Write RIFF header */
    riff_header_t riff = {
        .chunk_id = {'R', 'I', 'F', 'F'},
        .chunk_size = 36 + data_size,
        .format = {'W', 'A', 'V', 'E'}
    };
    fwrite(&riff, sizeof(riff), 1, file);
    
    /* Write fmt subchunk */
    fmt_subchunk_t fmt = {
        .subchunk_id = {'f', 'm', 't', ' '},
        .subchunk_size = 16,
        .audio_format = 1,  /* PCM */
        .num_channels = 1,  /* Mono */
        .sample_rate = sample_rate,
        .byte_rate = byte_rate,
        .block_align = 1 * bytes_per_sample,
        .bits_per_sample = 16
    };
    fwrite(&fmt, sizeof(fmt), 1, file);
    
    /* Write data subchunk header */
    data_subchunk_t data = {
        .subchunk_id = {'d', 'a', 't', 'a'},
        .subchunk_size = data_size
    };
    fwrite(&data, sizeof(data), 1, file);
    
    /* Convert and write samples as 16-bit PCM */
    for (uint32_t i = 0; i < num_samples; i++) {
        float sample = samples[i];
        
        /* Clamp to range [-1.0, 1.0] */
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        
        /* Convert to 16-bit signed integer */
        int16_t pcm_sample = (int16_t)(sample * 32767);
        fwrite(&pcm_sample, sizeof(pcm_sample), 1, file);
    }
    
    fclose(file);
    return 0;
}

int wav_read(const char *filename, float **samples, uint32_t *num_samples, wav_header_t *header)
{
    if (!filename || !samples || !num_samples) return -1;
    
    FILE *file = fopen(filename, "rb");
    if (!file) return -1;
    
    riff_header_t riff;
    if (fread(&riff, sizeof(riff), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    fmt_subchunk_t fmt;
    if (fread(&fmt, sizeof(fmt), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    data_subchunk_t data;
    if (fread(&data, sizeof(data), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    /* Validate format */
    if (fmt.audio_format != 1 || fmt.bits_per_sample != 16) {
        fclose(file);
        return -1;
    }
    
    uint32_t bytes_per_sample = fmt.bits_per_sample / 8;
    uint32_t num_read_samples = data.subchunk_size / bytes_per_sample;
    
    /* Allocate output buffer */
    *samples = malloc(num_read_samples * sizeof(float));
    if (!*samples) {
        fclose(file);
        return -1;
    }
    
    /* Read and convert samples */
    for (uint32_t i = 0; i < num_read_samples; i++) {
        int16_t pcm_sample;
        if (fread(&pcm_sample, sizeof(pcm_sample), 1, file) != 1) {
            free(*samples);
            fclose(file);
            return -1;
        }
        (*samples)[i] = (float)pcm_sample / 32768.0f;
    }
    
    /* Store header info */
    if (header) {
        header->sample_rate = fmt.sample_rate;
        header->num_channels = fmt.num_channels;
        header->bits_per_sample = fmt.bits_per_sample;
        header->num_samples = num_read_samples;
    }
    
    *num_samples = num_read_samples;
    fclose(file);
    return 0;
}

void wav_free(float *samples)
{
    if (samples) free(samples);
}
