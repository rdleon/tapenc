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
    printf("Tape Encoder - QPSK Modulation for Cassette Tapes\n");
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
        
        /* Add error correction with chunking */
        size_t chunk_size = 255 - ecc_bytes;
        size_t num_chunks = (file_size + chunk_size - 1) / chunk_size;
        size_t header_size = 4 + 4 + num_chunks * 4;  /* file_size + num_chunks + chunk_sizes */
        size_t encoded_size = header_size;
        
        /* First pass: calculate total encoded size */
        size_t current_pos = 0;
        for (size_t i = 0; i < num_chunks; i++) {
            size_t len = (current_pos + chunk_size > file_size) ? file_size - current_pos : chunk_size;
            encoded_size += len + ecc_bytes;
            current_pos += len;
        }
        
        uint8_t *encoded_data = malloc(encoded_size);
        if (!encoded_data) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(input_data);
            return 1;
        }
        
        /* Write header */
        memcpy(encoded_data, &file_size, 4);
        memcpy(encoded_data + 4, &num_chunks, 4);
        size_t header_offset = 8;
        
        size_t encoded_offset = header_size;
        current_pos = 0;
        for (size_t i = 0; i < num_chunks; i++) {
            size_t len = (current_pos + chunk_size > file_size) ? file_size - current_pos : chunk_size;
            
            uint8_t encoded_chunk[256];  /* Max 255 bytes */
            size_t chunk_encoded_len = 0;
            if (rs_encode(&input_data[current_pos], len, encoded_chunk, &chunk_encoded_len, ecc_bytes) != 0) {
                fprintf(stderr, "Error: Reed-Solomon encoding failed for chunk %zu\n", i);
                free(input_data);
                free(encoded_data);
                return 1;
            }
            
            /* Write chunk size to header */
            memcpy(encoded_data + header_offset, &len, 4);
            header_offset += 4;
            
            /* Copy encoded chunk */
            memcpy(&encoded_data[encoded_offset], encoded_chunk, chunk_encoded_len);
            encoded_offset += chunk_encoded_len;
            
            current_pos += len;
        }
        
        size_t actual_encoded = encoded_size;
        
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
        printf("  Encoded size (with ECC and chunking): %zu bytes\n", actual_encoded);
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
        
        /* Decode error correction with chunking */
        if (encoded_len < 8) {
            fprintf(stderr, "Error: Encoded data too short for header\n");
            wav_free(audio_samples);
            free(encoded_data);
            return 1;
        }
        
        uint32_t original_size;
        uint32_t num_chunks;
        memcpy(&original_size, encoded_data, 4);
        memcpy(&num_chunks, encoded_data + 4, 4);
        
        size_t header_size = 8 + num_chunks * 4;
        if (encoded_len < header_size) {
            fprintf(stderr, "Error: Encoded data too short for chunk headers\n");
            wav_free(audio_samples);
            free(encoded_data);
            return 1;
        }
        
        uint8_t *output_data = malloc(original_size);
        if (!output_data) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            wav_free(audio_samples);
            free(encoded_data);
            return 1;
        }
        
        size_t data_offset = header_size;
        size_t output_offset = 0;
        for (size_t i = 0; i < num_chunks; i++) {
            uint32_t chunk_data_len;
            memcpy(&chunk_data_len, encoded_data + 8 + i * 4, 4);
            
            size_t chunk_encoded_len = chunk_data_len + ecc_bytes;
            if (data_offset + chunk_encoded_len > encoded_len) {
                fprintf(stderr, "Error: Encoded data truncated\n");
                wav_free(audio_samples);
                free(encoded_data);
                free(output_data);
                return 1;
            }
            
            uint8_t *chunk_encoded = &encoded_data[data_offset];
            uint8_t chunk_decoded[255];  /* Max data + parity */
            size_t decoded_len = 0;
            
            if (rs_decode(chunk_encoded, chunk_encoded_len, chunk_decoded, &decoded_len, ecc_bytes) < 0) {
                fprintf(stderr, "Error: Reed-Solomon decoding failed for chunk %zu\n", i);
                wav_free(audio_samples);
                free(encoded_data);
                free(output_data);
                return 1;
            }
            
            if (decoded_len != chunk_data_len) {
                fprintf(stderr, "Error: Decoded length mismatch for chunk %zu\n", i);
                wav_free(audio_samples);
                free(encoded_data);
                free(output_data);
                return 1;
            }
            
            memcpy(&output_data[output_offset], chunk_decoded, chunk_data_len);
            output_offset += chunk_data_len;
            data_offset += chunk_encoded_len;
        }
        
        if (output_offset != original_size) {
            fprintf(stderr, "Error: Total decoded size mismatch\n");
            wav_free(audio_samples);
            free(encoded_data);
            free(output_data);
            return 1;
        }
        
        size_t output_len = original_size;
        
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
        printf("  Encoded size (with header and chunking): %zu bytes\n", encoded_len);
        printf("  Output size: %zu bytes\n", output_len);
        
        wav_free(audio_samples);
        free(encoded_data);
        free(output_data);
    }
    
    printf("\nOutput written to: %s\n", output_file);
    return 0;
}
