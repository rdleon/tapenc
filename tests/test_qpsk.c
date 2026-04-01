#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/qpsk.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Simple test assertion macro */
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  ✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", message); \
    } \
} while(0)

/* Test: Basic QPSK encode/decode round trip */
void test_qpsk_basic_roundtrip(void)
{
    printf("\n=== Test: Basic QPSK Round-trip ===\n");
    
    /* Initialize QPSK */
    qpsk_init(16000, 1200, 160);
    
    /* Test data: simple byte sequence */
    uint8_t input_data[] = {0x55, 0xAA, 0xFF, 0x00, 0xCC};
    size_t input_len = sizeof(input_data);
    
    /* Encode */
    size_t max_samples = input_len * 8 * 160;
    float *encoded_samples = malloc(max_samples * sizeof(float));
    size_t num_samples = 0;
    
    int encode_result = qpsk_encode(input_data, input_len, encoded_samples, &num_samples);
    TEST_ASSERT(encode_result == 0, "QPSK encode returns 0");
    TEST_ASSERT(num_samples > 0, "QPSK encode produces samples");
    printf("  ℹ Input: %zu bytes, Output: %zu samples\n", input_len, num_samples);
    
    /* Decode */
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    int decode_result = qpsk_decode(encoded_samples, num_samples, decoded_data, &decoded_len);
    TEST_ASSERT(decode_result == 0, "QPSK decode returns 0");
    TEST_ASSERT(decoded_len == input_len, "Decoded length matches input length");
    
    /* Verify data integrity */
    int data_matches = (memcmp(input_data, decoded_data, input_len) == 0);
    TEST_ASSERT(data_matches, "Decoded data matches input data");
    
    free(encoded_samples);
    free(decoded_data);
}

/* Test: Single byte encoding */
void test_qpsk_single_byte(void)
{
    printf("\n=== Test: Single Byte Encoding ===\n");
    
    qpsk_init(16000, 1200, 160);
    
    uint8_t input_data[] = {0xA5};  /* Binary: 10100101 */
    size_t input_len = 1;
    
    size_t max_samples = input_len * 8 * 160;
    float *encoded_samples = malloc(max_samples * sizeof(float));
    size_t num_samples = 0;
    
    qpsk_encode(input_data, input_len, encoded_samples, &num_samples);
    printf("  ℹ Single byte produces %zu samples\n", num_samples);
    TEST_ASSERT(num_samples > 0, "Single byte produces samples");
    
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    qpsk_decode(encoded_samples, num_samples, decoded_data, &decoded_len);
    TEST_ASSERT(decoded_data[0] == 0xA5, "Single byte decoded correctly");
    
    free(encoded_samples);
    free(decoded_data);
}

/* Test: Pattern encoding (all zeros, all ones) */
void test_qpsk_patterns(void)
{
    printf("\n=== Test: Pattern Encoding ===\n");
    
    qpsk_init(16000, 1200, 160);
    
    /* Test all zeros */
    uint8_t zeros[] = {0x00, 0x00, 0x00};
    size_t zeros_len = sizeof(zeros);
    
    size_t max_samples = zeros_len * 8 * 160;
    float *encoded_zeros = malloc(max_samples * sizeof(float));
    size_t num_samples = 0;
    
    qpsk_encode(zeros, zeros_len, encoded_zeros, &num_samples);
    
    uint8_t *decoded_zeros = malloc(zeros_len);
    size_t decoded_len = 0;
    qpsk_decode(encoded_zeros, num_samples, decoded_zeros, &decoded_len);
    
    TEST_ASSERT(memcmp(zeros, decoded_zeros, zeros_len) == 0, "All-zeros pattern decoded correctly");
    
    /* Test all ones */
    uint8_t ones[] = {0xFF, 0xFF, 0xFF};
    float *encoded_ones = malloc(max_samples * sizeof(float));
    num_samples = 0;
    
    qpsk_encode(ones, 3, encoded_ones, &num_samples);
    
    uint8_t *decoded_ones = malloc(3);
    decoded_len = 0;
    qpsk_decode(encoded_ones, num_samples, decoded_ones, &decoded_len);
    
    TEST_ASSERT(memcmp(ones, decoded_ones, 3) == 0, "All-ones pattern decoded correctly");
    
    free(encoded_zeros);
    free(encoded_ones);
    free(decoded_zeros);
    free(decoded_ones);
}

/* Test: Larger data payload */
void test_qpsk_large_payload(void)
{
    printf("\n=== Test: Large Payload ===\n");
    
    qpsk_init(16000, 1200, 160);
    
    /* Create a 256-byte test payload with varying patterns */
    uint8_t input_data[256];
    for (int i = 0; i < 256; i++) {
        input_data[i] = (uint8_t)i;
    }
    
    size_t input_len = 256;
    size_t max_samples = input_len * 8 * 160;
    float *encoded_samples = malloc(max_samples * sizeof(float));
    size_t num_samples = 0;
    
    int encode_result = qpsk_encode(input_data, input_len, encoded_samples, &num_samples);
    TEST_ASSERT(encode_result == 0, "Large payload encode successful");
    printf("  ℹ Input: %zu bytes, Output: %zu samples\n", input_len, num_samples);
    TEST_ASSERT(num_samples > 0, "Large payload produces samples");
    
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    int decode_result = qpsk_decode(encoded_samples, num_samples, decoded_data, &decoded_len);
    TEST_ASSERT(decode_result == 0, "Large payload decode successful");
    TEST_ASSERT(decoded_len == input_len, "Decoded length matches for large payload");
    TEST_ASSERT(memcmp(input_data, decoded_data, input_len) == 0, "Large payload decoded data matches");
    
    free(encoded_samples);
    free(decoded_data);
}

/* Test: Different carrier frequencies */
void test_qpsk_different_frequencies(void)
{
    printf("\n=== Test: Different Carrier Frequencies ===\n");
    
    uint8_t test_data[] = {0x42, 0x55, 0xAA};
    size_t test_len = sizeof(test_data);
    
    uint32_t frequencies[] = {800, 1200, 2400, 4800};
    int num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);
    
    for (int i = 0; i < num_freqs; i++) {
        qpsk_init(16000, frequencies[i], 160);
        
        size_t max_samples = test_len * 8 * 160;
        float *encoded = malloc(max_samples * sizeof(float));
        size_t num_samples = 0;
        
        qpsk_encode(test_data, test_len, encoded, &num_samples);
        
        uint8_t *decoded = malloc(test_len);
        size_t decoded_len = 0;
        
        qpsk_decode(encoded, num_samples, decoded, &decoded_len);
        
        if (memcmp(test_data, decoded, test_len) == 0) {
            printf("  ✓ Carrier frequency %u Hz works correctly\n", frequencies[i]);
            tests_passed++;
        } else {
            printf("  ✗ Carrier frequency %u Hz failed\n", frequencies[i]);
            tests_failed++;
        }
        tests_run++;
        
        free(encoded);
        free(decoded);
    }
}

/* Test: Different sample rates */
void test_qpsk_different_sample_rates(void)
{
    printf("\n=== Test: Different Sample Rates ===\n");
    
    uint8_t test_data[] = {0x5A, 0xA5};
    size_t test_len = sizeof(test_data);
    
    uint32_t sample_rates[] = {8000, 16000, 32000, 44100};
    int num_rates = sizeof(sample_rates) / sizeof(sample_rates[0]);
    
    for (int i = 0; i < num_rates; i++) {
        qpsk_init(sample_rates[i], 1200, 160);
        
        size_t max_samples = test_len * 8 * 160;
        float *encoded = malloc(max_samples * sizeof(float));
        size_t num_samples = 0;
        
        qpsk_encode(test_data, test_len, encoded, &num_samples);
        
        uint8_t *decoded = malloc(test_len);
        size_t decoded_len = 0;
        
        qpsk_decode(encoded, num_samples, decoded, &decoded_len);
        
        if (memcmp(test_data, decoded, test_len) == 0) {
            printf("  ✓ Sample rate %u Hz works correctly\n", sample_rates[i]);
            tests_passed++;
        } else {
            printf("  ✗ Sample rate %u Hz failed\n", sample_rates[i]);
            tests_failed++;
        }
        tests_run++;
        
        free(encoded);
        free(decoded);
    }
}

/* Test: Empty input handling */
void test_qpsk_empty_input(void)
{
    printf("\n=== Test: Empty Input Handling ===\n");
    
    qpsk_init(16000, 1200, 160);
    
    float *samples = malloc(1);
    size_t num_samples = 0;
    
    qpsk_encode(NULL, 0, samples, &num_samples);
    TEST_ASSERT(num_samples == 0, "Empty encode produces zero samples");
    
    uint8_t decoded_data[1];
    size_t decoded_len = 0;
    qpsk_decode(samples, 0, decoded_data, &decoded_len);
    TEST_ASSERT(decoded_len == 0, "Empty decode produces zero bytes");
    
    free(samples);
}

/* Test: Signal amplitude and envelope */
void test_qpsk_signal_properties(void)
{
    printf("\n=== Test: Signal Properties ===\n");
    
    qpsk_init(16000, 1200, 160);
    
    uint8_t test_data[] = {0xFF};  /* All ones */
    
    size_t max_samples = 8 * 160;
    float *encoded = malloc(max_samples * sizeof(float));
    size_t num_samples = 0;
    
    qpsk_encode(test_data, 1, encoded, &num_samples);
    
    /* Check signal amplitude is within reasonable bounds */
    float max_amplitude = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        float abs_sample = (encoded[i] < 0) ? -encoded[i] : encoded[i];
        if (abs_sample > max_amplitude) max_amplitude = abs_sample;
    }
    
    printf("  ℹ Max signal amplitude: %.4f\n", max_amplitude);
    /* Allow for normalized QPSK amplitude variations */
    TEST_ASSERT(max_amplitude > 0.5f && max_amplitude < 1.2f, "Signal amplitude in reasonable range");
    
    free(encoded);
}

void run_all_qpsk_tests(void)
{
    printf("\n╔════════════════════════════════════╗\n");
    printf("║   QPSK Encoder/Decoder Tests      ║\n");
    printf("╚════════════════════════════════════╝\n");
    
    test_qpsk_basic_roundtrip();
    test_qpsk_single_byte();
    test_qpsk_patterns();
    test_qpsk_large_payload();
    test_qpsk_different_frequencies();
    test_qpsk_different_sample_rates();
    test_qpsk_empty_input();
    test_qpsk_signal_properties();
    
    printf("\n╔════════════════════════════════════╗\n");
    printf("║         Test Summary               ║\n");
    printf("╚════════════════════════════════════╝\n");
    printf("Total:  %d tests\n", tests_run);
    printf("Passed: %d tests ✓\n", tests_passed);
    printf("Failed: %d tests ✗\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed!\n");
    } else {
        printf("\n⚠️  Some tests failed.\n");
    }
}

int main(void)
{
    run_all_qpsk_tests();
    return tests_failed > 0 ? 1 : 0;
}
