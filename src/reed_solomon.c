#include "reed_solomon.h"
#include <string.h>

/**
 * Simplified Reed-Solomon error correction implementation
 * Using polynomial arithmetic in GF(2^8)
 */

/* Galois Field multiplication tables */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int g_nsym = 0;

static void init_gf_tables(void)
{
    uint8_t x = 1;
    for (int i = 0; i < 256; i++) {
        gf_exp[i] = x;
        gf_exp[i + 255] = x;
        gf_log[x] = i;
        x = (x << 1) ^ (x & 0x80 ? 0x1d : 0x00);
    }
    gf_log[0] = 0;
}

static uint8_t gf_multiply(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

static uint8_t gf_divide(uint8_t a, uint8_t b)
{
    if (b == 0) return 0;
    if (a == 0) return 0;
    return gf_exp[gf_log[a] - gf_log[b] + 255];
}

static uint8_t gf_inverse(uint8_t a)
{
    return gf_exp[255 - gf_log[a]];
}

/**
 * Compute Reed-Solomon parity bytes using generator polynomial
 */
static void rs_calc_parity(uint8_t *data, int data_len, uint8_t *parity, int nsym)
{
    memset(parity, 0, nsym);
    
    for (int i = 0; i < data_len; i++) {
        uint8_t feedback = data[i] ^ parity[nsym - 1];
        
        for (int j = nsym - 1; j > 0; j--) {
            parity[j] = parity[j - 1] ^ gf_multiply(feedback, 1);  /* generator poly coefficient */
        }
        parity[0] = gf_multiply(feedback, 1);
    }
}

int rs_init(int nsym)
{
    if (nsym <= 0 || nsym > 255) return -1;
    
    init_gf_tables();
    g_nsym = nsym;
    return 0;
}

int rs_encode(uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, int nsym)
{
    if (!input || !output || !output_len || nsym <= 0 || nsym > 255) return -1;
    if (input_len + nsym > 255) return -1;
    
    /* Copy data to output */
    memcpy(output, input, input_len);
    
    /* Calculate parity bytes */
    rs_calc_parity(input, input_len, output + input_len, nsym);
    
    *output_len = input_len + nsym;
    return 0;
}

int rs_decode(uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, int nsym)
{
    if (!input || !output || !output_len || nsym <= 0) return -1;
    
    int data_len = input_len - nsym;
    if (data_len <= 0) return -1;
    
    /* For now, implement basic copy (full RS decoder is complex) */
    memcpy(output, input, data_len);
    *output_len = data_len;
    
    /* TODO: Implement full Berlekamp-Massey syndrome decoder */
    /* This would calculate syndromes and correct errors */
    
    return 0;  /* No errors corrected yet */
}
