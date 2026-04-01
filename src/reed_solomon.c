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

static uint8_t gf_inverse(uint8_t a)
{
    return gf_exp[255 - gf_log[a]];
}

/**
 * Compute generator polynomial coefficients for Reed-Solomon
 * g(x) = (x - α^0)(x - α^1)...(x - α^{nsym-1})
 */
static void compute_generator_poly(int nsym, uint8_t *gen_poly)
{
    gen_poly[0] = 1;
    for (int i = 1; i <= nsym; i++) {
        gen_poly[i] = 0;
    }
    
    for (int i = 0; i < nsym; i++) {
        uint8_t root = gf_exp[i];
        for (int j = nsym; j > 0; j--) {
            gen_poly[j] = gf_multiply(gen_poly[j], root) ^ gen_poly[j - 1];
        }
        gen_poly[0] = gf_multiply(gen_poly[0], root);
    }
}

/**
 * Evaluate polynomial at point x in GF(2^8)
 * coeffs[0] is constant term, coeffs[degree] is highest degree
 */
static uint8_t eval_poly(uint8_t *coeffs, int degree, uint8_t x)
{
    uint8_t result = coeffs[degree];
    for (int i = degree - 1; i >= 0; i--) {
        result = gf_multiply(result, x) ^ coeffs[i];
    }
    return result;
}

/**
 * Compute formal derivative of polynomial
 * For error locator polynomial derivative
 */
static void poly_derivative(uint8_t *poly, int degree, uint8_t *result)
{
    memset(result, 0, degree * sizeof(uint8_t));
    
    for (int i = 1; i <= degree; i++) {
        if (i % 2 == 1) {  /* Only odd powers contribute */
            result[i - 1] = poly[i];
        }
    }
}

/**
 * Calculate syndromes for Reed-Solomon decoding
 * syndromes[i] = received_poly(alpha^(i+1)) for i = 0 to 2t-1
 * Returns number of non-zero syndromes (0 = no errors)
 */
static int calc_syndromes(uint8_t *received, int received_len, int nsym, uint8_t *syndromes)
{
    int non_zero_count = 0;
    
    for (int i = 0; i < nsym; i++) {
        syndromes[i] = eval_poly(received, received_len - 1, gf_exp[i]);
        if (syndromes[i] != 0) non_zero_count++;
    }
    
    return non_zero_count;
}

/**
 * Berlekamp-Massey algorithm for finding error locator polynomial
 * Returns degree of error locator polynomial, or -1 on failure
 */
static int berlekamp_massey(uint8_t *syndromes, int nsym, uint8_t *lambda, uint8_t *omega)
{
    int t = nsym / 2;
    uint8_t *b = malloc((t + 1) * sizeof(uint8_t));
    uint8_t *c = malloc((t + 1) * sizeof(uint8_t));
    uint8_t *temp = malloc((t + 1) * sizeof(uint8_t));
    
    if (!b || !c || !temp) return -1;
    
    memset(b, 0, (t + 1) * sizeof(uint8_t));
    memset(c, 0, (t + 1) * sizeof(uint8_t));
    memset(lambda, 0, (t + 1) * sizeof(uint8_t));
    memset(omega, 0, nsym * sizeof(uint8_t));
    
    /* Initialize */
    b[0] = 1;
    c[0] = 1;
    lambda[0] = 1;
    
    int l = 0;  /* Current degree of lambda */
    int m = -1; /* Last update iteration */
    
    for (int k = 0; k < nsym; k++) {
        /* Compute discrepancy */
        uint8_t d = syndromes[k];
        for (int i = 1; i <= l && k - i >= 0; i++) {
            d ^= gf_multiply(lambda[i], syndromes[k - i]);
        }
        
        if (d == 0) {
            /* No update needed */
            continue;
        }
        
        /* Update lambda */
        memcpy(temp, lambda, (t + 1) * sizeof(uint8_t));
        uint8_t scale = gf_multiply(d, gf_inverse(b[0]));
        
        for (int i = 0; i <= k - m && i <= t; i++) {
            lambda[i] ^= gf_multiply(scale, b[i]);
        }
        
        if (2 * l <= k) {
            /* Update b and l */
            memcpy(b, temp, (t + 1) * sizeof(uint8_t));
            for (int i = 0; i <= k - m && i <= t; i++) {
                b[i] = gf_multiply(b[i], gf_inverse(d));
            }
            l = k + 1 - l;
            m = k;
        }
    }
    
    /* Compute omega (error evaluator polynomial) */
    for (int i = 0; i < nsym; i++) {
        omega[i] = syndromes[i];
        for (int j = 1; j <= l && i >= j; j++) {
            omega[i] ^= gf_multiply(lambda[j], syndromes[i - j]);
        }
    }
    
    free(b);
    free(c);
    free(temp);
    
    return l;  /* Degree of error locator polynomial */
}

/**
 * Chien search: find roots of error locator polynomial
 * Returns number of errors found
 */
static int chien_search(uint8_t *lambda, int lambda_degree, int *error_positions, int max_errors)
{
    int num_errors = 0;
    
    for (int i = 0; i < 255 && num_errors < max_errors; i++) {
        uint8_t sum = 0;
        for (int j = 0; j <= lambda_degree; j++) {
            if (lambda[j] != 0) {
                sum ^= gf_multiply(lambda[j], gf_exp[(i * j) % 255]);
            }
        }
        
        if (sum == 0) {
            /* Root found at position i */
            error_positions[num_errors++] = i;
        }
    }
    
    return num_errors;
}

/**
 * Forney algorithm: calculate error magnitudes
 * Returns 0 on success, -1 on failure
 */
static int forney_algorithm(uint8_t *omega, uint8_t *lambda, int lambda_degree, 
                           int *error_positions, uint8_t *error_magnitudes, int num_errors)
{
    uint8_t lambda_prime[32];  /* Derivative of lambda */
    
    poly_derivative(lambda, lambda_degree, lambda_prime);
    
    for (int i = 0; i < num_errors; i++) {
        int pos = error_positions[i];
        uint8_t x = gf_exp[255 - pos];  /* alpha^(-pos) */
        
        /* Evaluate omega at x */
        uint8_t omega_val = eval_poly(omega, lambda_degree - 1, x);
        
        /* Evaluate lambda' at x */
        uint8_t lambda_prime_val = eval_poly(lambda_prime, lambda_degree - 1, x);
        
        if (lambda_prime_val == 0) {
            return -1;  /* Cannot compute error magnitude */
        }
        
        error_magnitudes[i] = gf_multiply(omega_val, gf_inverse(lambda_prime_val));
    }
    
    return 0;
}

/**
 * Compute Reed-Solomon parity bytes using generator polynomial
 */
static void rs_calc_parity(uint8_t *data, int data_len, uint8_t *parity, int nsym)
{
    uint8_t *gen_poly = malloc((nsym + 1) * sizeof(uint8_t));
    if (!gen_poly) return;  /* Error, but void */
    
    compute_generator_poly(nsym, gen_poly);
    
    memset(parity, 0, nsym);
    
    for (int i = 0; i < data_len; i++) {
        uint8_t feedback = data[i] ^ parity[nsym - 1];
        
        for (int j = nsym - 1; j > 0; j--) {
            parity[j] = parity[j - 1] ^ gf_multiply(feedback, gen_poly[j]);
        }
        parity[0] = gf_multiply(feedback, gen_poly[0]);
    }
    
    free(gen_poly);
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
    
    /* For now, just copy data (error correction implementation in progress) */
    memcpy(output, input, data_len);
    *output_len = data_len;
    
    return 0;  /* No errors corrected yet */
}
