#ifndef REED_SOLOMON_H
#define REED_SOLOMON_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Reed-Solomon error correction code
 * Implements RS(255, 223) - 32 parity bytes can correct up to 16 byte errors
 */

typedef struct {
    uint8_t *data;
    size_t data_len;
    uint8_t *parity;
    size_t parity_len;
} reed_solomon_block_t;

/**
 * Initialize Reed-Solomon encoder/decoder
 * @param nsym: number of parity symbols (error correction capability = nsym/2)
 * @return: 0 on success, -1 on error
 */
int rs_init(int nsym);

/**
 * Encode data with Reed-Solomon error correction
 * @param input: input data
 * @param input_len: length of input data
 * @param output: output buffer (will contain data + parity bytes)
 * @param output_len: pointer to store output length
 * @param nsym: number of parity symbols to generate
 * @return: 0 on success, -1 on error
 */
int rs_encode(uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, int nsym);

/**
 * Decode data with Reed-Solomon error correction
 * @param input: encoded data (data + parity bytes)
 * @param input_len: length of input
 * @param output: output buffer (decoded data)
 * @param output_len: pointer to store output length
 * @param nsym: number of parity symbols in input
 * @return: 0 on success, -1 on error, number of corrected errors on success (>= 0)
 */
int rs_decode(uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, int nsym);

#endif /* REED_SOLOMON_H */
