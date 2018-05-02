#pragma once
#include <stdint.h>
#include <gmp.h>

/* Parameter generation */

typedef uint8_t rseed_t[32];

typedef struct gamma {
  mpz_t p;
  mpz_t q;
  uint64_t log_sigma;
  uint64_t n;
  gmp_randstate_t rstate;
  rseed_t rseed;
  uint64_t d;
} gamma_t;

#define GAMMA_N 1470
#define GAMMA_D (1<<15)

gamma_t param_gen();
gamma_t param_gen_from_seed(rseed_t rseed);
void param_clear(gamma_t *g);

/* secret key generation */
typedef mpz_t sk_t[GAMMA_N];

void key_gen(sk_t sk, gamma_t gamma);
void key_clear(sk_t sk, gamma_t gamma);


/* error distributions */
void errdist_uniform(mpz_t e, gamma_t gamma);

/* ciphertext */
typedef mpz_t ct_t[GAMMA_N+1];

void ct_init(ct_t ct);
void ct_clear(ct_t ct);

void decompress_encryption(ct_t c, gamma_t gamma, gmp_randstate_t rs, mpz_t b);
void encrypt1(ct_t c, gamma_t gamma, gmp_randstate_t rs, sk_t sk, mpz_t m, void (*chi)(mpz_t, gamma_t));

static inline
void encrypt(ct_t c, gamma_t gamma, gmp_randstate_t rs, sk_t sk, mpz_t m)
{
  encrypt1(c, gamma, rs, sk, m, errdist_uniform);
}


void decrypt(mpz_t m, gamma_t gamma, sk_t sk, ct_t ct);
void eval(ct_t rop, gamma_t gamma, ct_t c[], mpz_t coeffs[], size_t d);


#define ct_clearv(vs, len) do {                     \
    for (size_t i = 0; i < len; i++) {              \
      ct_clear((vs)[i]);                            \
  }                                                 \
} while (0)


#define mpz_clearv(vs, len) do {                    \
  for (size_t i = 0; i < len; i++) {                \
    mpz_clear((vs)[i]);                             \
  }                                                 \
} while (0)

#define mpz_initv(vs, len) do {                 \
    for (size_t i = 0; i < len; i++) {          \
      mpz_init((vs)[i]);                        \
    }                                           \
  } while (0)
