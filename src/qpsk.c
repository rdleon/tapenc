#include "qpsk.h"
#include <math.h>
#include <string.h>

/* Define M_PI if not available */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Global state */
static uint32_t g_sample_rate = QPSK_SAMPLE_RATE;
static uint32_t g_carrier_freq = QPSK_CARRIER_FREQ;
static uint32_t g_samples_per_symbol = QPSK_SAMPLES_PER_SYMBOL;

/* Precomputed cosine and sine tables for carrier signal */
static float *g_carrier_i = NULL;
static float *g_carrier_q = NULL;

void qpsk_init(uint32_t sample_rate, uint32_t carrier_freq, uint32_t samples_per_symbol)
{
    g_sample_rate = sample_rate;
    g_carrier_freq = carrier_freq;
    g_samples_per_symbol = samples_per_symbol;
    
    /* Allocate carrier signal tables */
    if (g_carrier_i != NULL) free(g_carrier_i);
    if (g_carrier_q != NULL) free(g_carrier_q);
    
    g_carrier_i = malloc(samples_per_symbol * sizeof(float));
    g_carrier_q = malloc(samples_per_symbol * sizeof(float));
    
    /* Precompute carrier signal */
    float phase_increment = 2.0f * M_PI * carrier_freq / sample_rate;
    for (uint32_t i = 0; i < samples_per_symbol; i++) {
        float phase = phase_increment * i;
        g_carrier_i[i] = cosf(phase);
        g_carrier_q[i] = -sinf(phase);  /* negative for standard QPSK */
    }
}

int qpsk_encode(uint8_t *input, size_t input_len, float *output, size_t *output_len)
{
    if (!input || !output || !output_len) return -1;
    if (input_len == 0) {
        *output_len = 0;
        return 0;
    }
    
    size_t output_idx = 0;
    float symbol_scale = 1.0f / sqrtf(2.0f);  /* normalized QPSK amplitude */
    
    /* Process each bit pair (dibits) */
    for (size_t i = 0; i < input_len; i++) {
        uint8_t byte = input[i];
        
        for (int bit_idx = 0; bit_idx < 8; bit_idx += 2) {
            /* Extract two bits for QPSK symbol */
            int bit0 = (byte >> (7 - bit_idx)) & 1;
            int bit1 = (byte >> (7 - bit_idx - 1)) & 1;
            
            /* Map bits to QPSK symbol (00=0, 01=π/2, 10=π, 11=3π/2) */
            float i_sym = (bit0 == 0) ? symbol_scale : -symbol_scale;
            float q_sym = (bit1 == 0) ? symbol_scale : -symbol_scale;
            
            /* Modulate: multiply symbol by carrier and add to output */
            for (uint32_t j = 0; j < g_samples_per_symbol; j++) {
                float i_sample = i_sym * g_carrier_i[j];
                float q_sample = q_sym * g_carrier_q[j];
                output[output_idx++] = i_sample + q_sample;
            }
        }
    }
    
    *output_len = output_idx;
    return 0;
}

int qpsk_decode(float *input, size_t input_len, uint8_t *output, size_t *output_len)
{
    if (!input || !output || !output_len) return -1;
    if (input_len == 0) {
        *output_len = 0;
        return 0;
    }
    
    size_t output_idx = 0;
    size_t num_symbols = input_len / g_samples_per_symbol;
    if (num_symbols == 0) return -1;
    
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    /* Process each symbol */
    for (size_t sym = 0; sym < num_symbols; sym++) {
        /* Demodulate: correlate received signal with carriers */
        float i_accum = 0.0f, q_accum = 0.0f;
        
        for (uint32_t j = 0; j < g_samples_per_symbol; j++) {
            size_t idx = sym * g_samples_per_symbol + j;
            if (idx >= input_len) break;
            
            float sample = input[idx];
            i_accum += sample * g_carrier_i[j];
            q_accum += sample * g_carrier_q[j];
        }
        
        /* Normalize */
        i_accum /= g_samples_per_symbol;
        q_accum /= g_samples_per_symbol;
        
        /* Hard decision: determine bits based on quadrant */
        int bit0 = (i_accum > 0) ? 0 : 1;
        int bit1 = (q_accum > 0) ? 0 : 1;
        
        /* Pack bits into byte */
        current_byte |= (bit0 << (7 - bit_count));
        current_byte |= (bit1 << (6 - bit_count));
        bit_count += 2;
        
        if (bit_count >= 8) {
            output[output_idx++] = current_byte;
            current_byte = 0;
            bit_count = 0;
        }
    }
    
    /* Flush remaining bits */
    if (bit_count > 0) {
        output[output_idx++] = current_byte;
    }
    
    *output_len = output_idx;
    return 0;
}
