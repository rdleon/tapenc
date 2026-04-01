#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qpsk.h"
#include "wav.h"
#include "reed_solomon.h"

#define MAX_INPUT_SIZE (1024 * 1024)  /* 1MB max */

typedef enum {
    MODE_ENCODE,
    MODE_DECODE
} operation_mode_t;

void print_usage(const char *prog)
{
    printf("Audio Encoder - QPSK Modulation for Cassette Tapes\n");
    printf("Usage: %s -e|-d [OPTIONS] INPUT OUTPUT\n\n", prog);
    printf("Modes:\n");
    printf("  -e                          Encode mode (data → audio)\n");
    printf("  -d                          Decode mode (audio → data)\n\n");
    printf("Positional Arguments:\n");
    printf("  INPUT                       Input file\n");
    printf("  OUTPUT                      Output file\n\n");
    printf("Options:\n");
    printf("  -r <Hz>                     Sample rate (default: 16000)\n");
    printf("  -f <Hz>                     Carrier frequency (default: 1200)\n");
    printf("  -p <bytes>                  Reed-Solomon parity bytes (default: 32)\n");
    printf("  -h                          Show this help message\n");
}

int main(int argc, char *argv[])
{
    operation_mode_t mode = MODE_ENCODE;
    const char *input_file = NULL;
    const char *output_file = NULL;
    uint32_t sample_rate = QPSK_SAMPLE_RATE;
    uint32_t carrier_freq = QPSK_CARRIER_FREQ;
    int ecc_bytes = 32;
    
    /* Parse command line arguments */
    int opt_idx = 1;
    
    /* First argument must be -e or -d */
    if (opt_idx >= argc) {
        fprintf(stderr, "Error: Mode (-e or -d) is required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[opt_idx], "-e") == 0) {
        mode = MODE_ENCODE;
        opt_idx++;
    } else if (strcmp(argv[opt_idx], "-d") == 0) {
        mode = MODE_DECODE;
        opt_idx++;
    } else if (strcmp(argv[opt_idx], "-h") == 0 || strcmp(argv[opt_idx], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Error: Unknown mode '%s'\n", argv[opt_idx]);
        print_usage(argv[0]);
        return 1;
    }
    
    /* Parse optional flags */
    while (opt_idx < argc && argv[opt_idx][0] == '-') {
        if (strcmp(argv[opt_idx], "-r") == 0) {
            if (opt_idx + 1 >= argc) {
                fprintf(stderr, "Error: -r requires an argument\n");
                return 1;
            }
            sample_rate = atoi(argv[++opt_idx]);
            opt_idx++;
        } else if (strcmp(argv[opt_idx], "-f") == 0) {
            if (opt_idx + 1 >= argc) {
                fprintf(stderr, "Error: -f requires an argument\n");
                return 1;
            }
            carrier_freq = atoi(argv[++opt_idx]);
            opt_idx++;
        } else if (strcmp(argv[opt_idx], "-p") == 0) {
            if (opt_idx + 1 >= argc) {
                fprintf(stderr, "Error: -p requires an argument\n");
                return 1;
            }
            ecc_bytes = atoi(argv[++opt_idx]);
            opt_idx++;
        } else if (strcmp(argv[opt_idx], "-h") == 0 || strcmp(argv[opt_idx], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[opt_idx]);
            return 1;
        }
    }
    
    /* Get positional arguments: input and output files */
    if (opt_idx >= argc) {
        fprintf(stderr, "Error: INPUT file is required\n");
        print_usage(argv[0]);
        return 1;
    }
    input_file = argv[opt_idx++];
    
    if (opt_idx >= argc) {
        fprintf(stderr, "Error: OUTPUT file is required\n");
        print_usage(argv[0]);
        return 1;
    }
    output_file = argv[opt_idx++];
    
    /* Initialize modules */
    qpsk_init(sample_rate, carrier_freq, QPSK_SAMPLES_PER_SYMBOL);
    if (rs_init(ecc_bytes) != 0) {
        fprintf(stderr, "Error: Failed to initialize Reed-Solomon\n");
        return 1;
    }
    
    printf("Audio Encoder - QPSK\n");
    printf("Mode: %s\n", mode == MODE_ENCODE ? "Encode" : "Decode");
    printf("Sample Rate: %u Hz\n", sample_rate);
    printf("Carrier Freq: %u Hz\n", carrier_freq);
    printf("ECC Parity Bytes: %d\n\n", ecc_bytes);
    
    if (mode == MODE_ENCODE) {
        /* Read input file */
        FILE *fd = fopen(input_file, "rb");
        if (!fd) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
            return 1;
        }
        
        fseek(fd, 0, SEEK_END);
        size_t file_size = ftell(fd);
        rewind(fd);
        
        if (file_size == 0 || file_size > MAX_INPUT_SIZE) {
            fprintf(stderr, "Error: Input file size out of range (0 to %d bytes)\n", MAX_INPUT_SIZE);
            fclose(fd);
            return 1;
        }
        
        uint8_t *input_data = malloc(file_size);
        if (!input_data || fread(input_data, 1, file_size, fd) != file_size) {
            fprintf(stderr, "Error: Failed to read input file\n");
            free(input_data);
            fclose(fd);
            return 1;
        }
        fclose(fd);
        
        /* Add error correction */
        size_t encoded_size = file_size + ecc_bytes;
        uint8_t *encoded_data = malloc(encoded_size);
        size_t actual_encoded = 0;
        
        if (rs_encode(input_data, file_size, encoded_data, &actual_encoded, ecc_bytes) != 0) {
            fprintf(stderr, "Error: Reed-Solomon encoding failed\n");
            free(input_data);
            free(encoded_data);
            return 1;
        }
        
        /* QPSK modulate */
        size_t max_samples = actual_encoded * 8 * QPSK_SAMPLES_PER_SYMBOL;
        float *audio_samples = malloc(max_samples * sizeof(float));
        size_t num_samples = 0;
        
        if (qpsk_encode(encoded_data, actual_encoded, audio_samples, &num_samples) != 0) {
            fprintf(stderr, "Error: QPSK encoding failed\n");
            free(input_data);
            free(encoded_data);
            free(audio_samples);
            return 1;
        }
        
        /* Write WAV file */
        if (wav_write(output_file, audio_samples, num_samples, sample_rate) != 0) {
            fprintf(stderr, "Error: Failed to write WAV file\n");
            free(input_data);
            free(encoded_data);
            free(audio_samples);
            return 1;
        }
        
        printf("Encode successful:\n");
        printf("  Input size: %zu bytes\n", file_size);
        printf("  Encoded size (with ECC): %zu bytes\n", actual_encoded);
        printf("  Audio samples: %zu\n", num_samples);
        printf("  Duration: %.2f seconds\n", (float)num_samples / sample_rate);
        
        free(input_data);
        free(encoded_data);
        free(audio_samples);
        
    } else {
        /* MODE_DECODE */
        
        /* Read WAV file */
        float *audio_samples = NULL;
        uint32_t num_samples = 0;
        wav_header_t header;
        
        if (wav_read(input_file, &audio_samples, &num_samples, &header) != 0) {
            fprintf(stderr, "Error: Failed to read WAV file\n");
            return 1;
        }
        
        printf("Audio loaded:\n");
        printf("  Samples: %u\n", num_samples);
        printf("  Duration: %.2f seconds\n", (float)num_samples / header.sample_rate);
        
        /* QPSK demodulate */
        size_t max_bytes = (num_samples / QPSK_SAMPLES_PER_SYMBOL / 4) + 1;
        uint8_t *encoded_data = malloc(max_bytes);
        size_t encoded_len = 0;
        
        if (qpsk_decode(audio_samples, num_samples, encoded_data, &encoded_len) != 0) {
            fprintf(stderr, "Error: QPSK decoding failed\n");
            wav_free(audio_samples);
            free(encoded_data);
            return 1;
        }
        
        /* Decode error correction */
        size_t output_len = 0;
        uint8_t *output_data = malloc(encoded_len);
        
        if (rs_decode(encoded_data, encoded_len, output_data, &output_len, ecc_bytes) < 0) {
            fprintf(stderr, "Error: Reed-Solomon decoding failed\n");
            wav_free(audio_samples);
            free(encoded_data);
            free(output_data);
            return 1;
        }
        
        /* Write output file */
        FILE *fo = fopen(output_file, "wb");
        if (!fo || fwrite(output_data, 1, output_len, fo) != output_len) {
            fprintf(stderr, "Error: Failed to write output file\n");
            wav_free(audio_samples);
            free(encoded_data);
            free(output_data);
            if (fo) fclose(fo);
            return 1;
        }
        fclose(fo);
        
        printf("Decode successful:\n");
        printf("  Encoded size: %zu bytes\n", encoded_len);
        printf("  Output size: %zu bytes\n", output_len);
        
        wav_free(audio_samples);
        free(encoded_data);
        free(output_data);
    }
    
    printf("\nOutput written to: %s\n", output_file);
    return 0;
}
