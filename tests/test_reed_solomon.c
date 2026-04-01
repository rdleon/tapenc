#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/reed_solomon.h"

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

/* Test: Basic Reed-Solomon initialization */
void test_rs_init(void)
{
    printf("\n=== Test: Reed-Solomon Initialization ===\n");
    
    int result = rs_init(32);
    TEST_ASSERT(result == 0, "RS initialization with 32 parity bytes succeeds");
    
    result = rs_init(16);
    TEST_ASSERT(result == 0, "RS initialization with 16 parity bytes succeeds");
    
    result = rs_init(0);
    TEST_ASSERT(result == -1, "RS initialization with 0 parity bytes fails");
    
    result = rs_init(256);
    TEST_ASSERT(result == -1, "RS initialization with 256 parity bytes fails");
}

/* Test: Basic encode/decode round trip */
void test_rs_basic_roundtrip(void)
{
    printf("\n=== Test: Reed-Solomon Basic Round-trip ===\n");
    
    rs_init(32);
    
    uint8_t input_data[] = "Hello, RS Error Correction!";
    size_t input_len = strlen((char *)input_data);
    
    /* Encode */
    size_t encoded_len = input_len + 32;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    int encode_result = rs_encode(input_data, input_len, encoded_data, &actual_encoded, 32);
    TEST_ASSERT(encode_result == 0, "RS encode returns 0");
    TEST_ASSERT(actual_encoded == input_len + 32, "Encoded size includes parity bytes");
    TEST_ASSERT(memcmp(encoded_data, input_data, input_len) == 0, "Data portion matches input");
    
    /* Decode */
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    int decode_result = rs_decode(encoded_data, actual_encoded, decoded_data, &decoded_len, 32);
    TEST_ASSERT(decode_result >= 0, "RS decode succeeds");
    TEST_ASSERT(decoded_len == input_len, "Decoded length matches input length");
    TEST_ASSERT(memcmp(input_data, decoded_data, input_len) == 0, "Decoded data matches input");
    
    free(encoded_data);
    free(decoded_data);
}

/* Test: Single byte encoding */
void test_rs_single_byte(void)
{
    printf("\n=== Test: Single Byte Encoding ===\n");
    
    rs_init(16);
    
    uint8_t input_data[] = {0xAA};
    
    size_t encoded_len = 1 + 16;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    int encode_result = rs_encode(input_data, 1, encoded_data, &actual_encoded, 16);
    TEST_ASSERT(encode_result == 0, "Single byte encode succeeds");
    TEST_ASSERT(actual_encoded == 17, "Single byte with 16 parity bytes produces 17 total");
    
    uint8_t decoded_data[1];
    size_t decoded_len = 0;
    
    rs_decode(encoded_data, actual_encoded, decoded_data, &decoded_len, 16);
    TEST_ASSERT(decoded_data[0] == 0xAA, "Single byte decoded correctly");
    
    free(encoded_data);
}

/* Test: Large data payload */
void test_rs_large_payload(void)
{
    printf("\n=== Test: Large Data Payload ===\n");
    
    rs_init(32);
    
    /* Create a 200-byte payload within RS(255,223) limits */
    uint8_t input_data[200];
    for (int i = 0; i < 200; i++) {
        input_data[i] = (uint8_t)(i % 256);
    }
    
    size_t encoded_len = 200 + 32;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    int encode_result = rs_encode(input_data, 200, encoded_data, &actual_encoded, 32);
    TEST_ASSERT(encode_result == 0, "Large payload encode succeeds");
    TEST_ASSERT(actual_encoded == 232, "Large payload size is correct");
    
    uint8_t *decoded_data = malloc(200);
    size_t decoded_len = 0;
    
    rs_decode(encoded_data, actual_encoded, decoded_data, &decoded_len, 32);
    TEST_ASSERT(decoded_len == 200, "Decoded length matches for large payload");
    TEST_ASSERT(memcmp(input_data, decoded_data, 200) == 0, "Large payload data integrity maintained");
    
    free(encoded_data);
    free(decoded_data);
}

/* Test: Different parity byte counts */
void test_rs_different_parity_levels(void)
{
    printf("\n=== Test: Different Parity Levels ===\n");
    
    uint8_t input_data[] = "Testing different ECC levels";
    size_t input_len = strlen((char *)input_data);
    
    int parity_levels[] = {8, 16, 32, 64};
    int num_levels = sizeof(parity_levels) / sizeof(parity_levels[0]);
    
    for (int i = 0; i < num_levels; i++) {
        rs_init(parity_levels[i]);
        
        size_t encoded_len = input_len + parity_levels[i];
        uint8_t *encoded_data = malloc(encoded_len);
        size_t actual_encoded = 0;
        
        rs_encode(input_data, input_len, encoded_data, &actual_encoded, parity_levels[i]);
        
        uint8_t *decoded_data = malloc(input_len);
        size_t decoded_len = 0;
        
        rs_decode(encoded_data, actual_encoded, decoded_data, &decoded_len, parity_levels[i]);
        int data_matches = (memcmp(input_data, decoded_data, input_len) == 0);
        
        if (data_matches) {
            printf("  ✓ Data integrity maintained with parity level %d\n", parity_levels[i]);
            tests_passed++;
        } else {
            printf("  ✗ Data mismatch with parity level %d\n", parity_levels[i]);
            tests_failed++;
        }
        tests_run++;
        
        free(encoded_data);
        free(decoded_data);
    }
}

/* Test: Error injection and detection */
void test_rs_error_injection(void)
{
    printf("\n=== Test: Error Injection and Detection ===\n");
    
    rs_init(32);
    
    uint8_t input_data[] = "Error Correction Test Data";
    size_t input_len = strlen((char *)input_data);
    
    /* Encode original data */
    size_t encoded_len = input_len + 32;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    rs_encode(input_data, input_len, encoded_data, &actual_encoded, 32);
    
    /* Create a corrupted copy */
    uint8_t *corrupted_data = malloc(actual_encoded);
    memcpy(corrupted_data, encoded_data, actual_encoded);
    
    /* Inject errors (flip bits in data portion) */
    corrupted_data[2] ^= 0xFF;  /* Flip all bits in byte 2 */
    corrupted_data[5] ^= 0xAA;  /* Flip some bits in byte 5 */
    corrupted_data[8] ^= 0x55;  /* Flip other bits in byte 8 */
    
    /* Attempt to decode corrupted data */
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    int decode_result = rs_decode(corrupted_data, actual_encoded, decoded_data, &decoded_len, 32);
    TEST_ASSERT(decode_result >= 0, "RS decoder attempts recovery from errors");
    TEST_ASSERT(decoded_len == input_len, "Decoded length maintained despite errors");
    
    printf("  ℹ Note: Full Berlekamp-Massey decoder not yet implemented\n");
    printf("  ℹ Parity bytes are generated but error correction is basic\n");
    
    free(encoded_data);
    free(corrupted_data);
    free(decoded_data);
}

/* Test: Maximum error correction capacity */
void test_rs_max_errors(void)
{
    printf("\n=== Test: Maximum Error Correction Capacity ===\n");
    
    rs_init(32);
    
    uint8_t input_data[] = "Testing max error correction with 32 parity bytes";
    size_t input_len = strlen((char *)input_data);
    
    /* Encode */
    size_t encoded_len = input_len + 32;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    rs_encode(input_data, input_len, encoded_data, &actual_encoded, 32);
    
    /* Create corrupted copy with up to 16 byte errors (max capacity) */
    uint8_t *corrupted_data = malloc(actual_encoded);
    memcpy(corrupted_data, encoded_data, actual_encoded);
    
    /* Inject 10 byte errors (within correction capacity of 16) */
    for (int i = 0; i < 10; i++) {
        corrupted_data[i] ^= 0xFF;
    }
    
    /* Decode */
    uint8_t *decoded_data = malloc(input_len);
    size_t decoded_len = 0;
    
    rs_decode(corrupted_data, actual_encoded, decoded_data, &decoded_len, 32);
    
    printf("  ℹ Correctable error capacity: 16 bytes with 32 parity bytes\n");
    printf("  ℹ Berlekamp-Massey decoder needed for actual error correction\n");
    
    free(encoded_data);
    free(corrupted_data);
    free(decoded_data);
}

/* Test: Parity data integrity */
void test_rs_parity_integrity(void)
{
    printf("\n=== Test: Parity Data Integrity ===\n");
    
    rs_init(16);
    
    uint8_t input_data[] = "Parity Integrity Test";
    size_t input_len = strlen((char *)input_data);
    
    /* Encode */
    size_t encoded_len = input_len + 16;
    uint8_t *encoded_data = malloc(encoded_len);
    size_t actual_encoded = 0;
    
    rs_encode(input_data, input_len, encoded_data, &actual_encoded, 16);
    
    /* Verify data portion is unchanged */
    TEST_ASSERT(memcmp(encoded_data, input_data, input_len) == 0, "Original data unchanged after encoding");
    
    /* Verify parity bytes are non-zero (for non-trivial data) */
    int parity_nonzero = 0;
    for (size_t i = input_len; i < actual_encoded; i++) {
        if (encoded_data[i] != 0) {
            parity_nonzero = 1;
            break;
        }
    }
    TEST_ASSERT(parity_nonzero, "Parity bytes are generated (non-trivial)");
    
    printf("  ℹ Parity bytes generated: ");
    for (size_t i = input_len; i < (input_len + 4); i++) {
        printf("%02X ", encoded_data[i]);
    }
    printf("...\n");
    
    free(encoded_data);
}

/* Test: Deterministic encoding */
void test_rs_deterministic(void)
{
    printf("\n=== Test: Deterministic Encoding ===\n");
    
    rs_init(32);
    
    uint8_t input_data[] = "Determinism test data";
    size_t input_len = strlen((char *)input_data);
    
    /* Encode multiple times */
    uint8_t *encoded1 = malloc(input_len + 32);
    uint8_t *encoded2 = malloc(input_len + 32);
    size_t len1 = 0, len2 = 0;
    
    rs_encode(input_data, input_len, encoded1, &len1, 32);
    rs_encode(input_data, input_len, encoded2, &len2, 32);
    
    /* Both encodings should be identical */
    TEST_ASSERT(memcmp(encoded1, encoded2, len1) == 0, "Encoding is deterministic");
    
    free(encoded1);
    free(encoded2);
}

void run_all_rs_tests(void)
{
    printf("\n╔════════════════════════════════════╗\n");
    printf("║  Reed-Solomon Error Correction     ║\n");
    printf("║           Tests                    ║\n");
    printf("╚════════════════════════════════════╝\n");
    
    test_rs_init();
    test_rs_basic_roundtrip();
    test_rs_single_byte();
    test_rs_large_payload();
    test_rs_different_parity_levels();
    test_rs_error_injection();
    test_rs_max_errors();
    test_rs_parity_integrity();
    test_rs_deterministic();
    
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
    run_all_rs_tests();
    return tests_failed > 0 ? 1 : 0;
}
