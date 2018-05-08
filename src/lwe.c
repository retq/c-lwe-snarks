#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/random.h>

#include <flint/nmod_poly.h>
#include <gmp.h>

#include "lwe.h"

void mpz_add_dotp(mpz_t rop,
                  mpz_t modulus,
                  mpz_t a[static 1], mpz_t b[static 1],
                  size_t len)
{
  for (size_t i = 0; i < len; i++) {
    mpz_addmul(rop, a[i], b[i]);
  }
  mpz_mod(rop, rop, modulus);
}

gamma_t param_gen_from_seed(rseed_t rseed)
{
  gamma_t gamma;
  mpz_init(gamma.q);
  mpz_ui_pow_ui(gamma.q, 2, GAMMA_LOGQ);

  gmp_randinit_default(gamma.rstate);

  mpz_t mpz_rseed;
  mpz_init(mpz_rseed);
  mpz_import(mpz_rseed, 32, 1, sizeof(rseed[0]), 0, 0, rseed);
  gmp_randseed(gamma.rstate, mpz_rseed);
  mpz_clear(mpz_rseed);

  memmove(gamma.rseed, rseed, sizeof(rseed_t));
  return gamma;
}

gamma_t param_gen()
{
  rseed_t rseed;
  getrandom(&rseed, sizeof(rseed_t), GRND_NONBLOCK);
  return param_gen_from_seed(rseed);
}

void param_clear(gamma_t *g)
{
  mpz_clear(g->q);
  gmp_randclear(g->rstate);
}


void key_gen(sk_t sk, gamma_t gamma)
{
  mpz_initv(sk, GAMMA_N);
  mpz_urandommv(sk, gamma.rstate, GAMMA_LOGQ, GAMMA_N);
}

void key_clear(sk_t sk)
{
  mpz_clearv(sk, GAMMA_N);
}

void ct_init(ct_t ct)
{
  mpz_initv(ct, GAMMA_N+1);
}

void ct_clear(ct_t ct)
{
  mpz_clearv(ct, GAMMA_N+1);
}

static inline void mpz_randomsgn(mpz_t dst, gamma_t gamma, const mpz_t src)
{
  uint8_t sign;
  getrandom(&sign, 1, GRND_NONBLOCK);
  if (sign & 0x01) {
    // mpz_sub(dst, gamma.q, src);
    mpz_neg(dst, src);
  }
}

void errdist_uniform(mpz_t e, gamma_t gamma)
{
  mpz_urandomb(e, gamma.rstate, GAMMA_LOG_SIGMA+3);
}

void ct_smudge(ct_t ct, gamma_t gamma) {
  mpz_t smudging;
  mpz_init(smudging);

  mpz_urandomb(smudging, gamma.rstate, GAMMA_LOG_SMUDGING);
  mpz_randomsgn(smudging, gamma, smudging);
  mpz_mul_ui(smudging, smudging, GAMMA_P);

  mpz_add(ct[GAMMA_N], ct[GAMMA_N], smudging);
  modq(ct[GAMMA_N]);
  mpz_clear(smudging);
}

void regev_encrypt1(ct_t c, gamma_t gamma, gmp_randstate_t rs, sk_t sk, mpz_t m, void (*chi)(mpz_t, gamma_t))
{
  assert(mpz_cmp_ui(m, GAMMA_P) < 0);

  // sample the error
  mpz_t e;
  mpz_init(e);
  (*chi)(e, gamma);
  mpz_mul_ui(c[GAMMA_N], e, GAMMA_P);
  mpz_randomsgn(e, gamma, e);

  // sample a
  mpz_urandommv(c, rs, GAMMA_LOGQ, GAMMA_N);

  mpz_add_dotp(c[GAMMA_N], gamma.q, sk, c, GAMMA_N);
  mpz_add(c[GAMMA_N], c[GAMMA_N], m);
  mpz_mod(c[GAMMA_N], c[GAMMA_N], gamma.q);

  mpz_clear(e);
}

void decompress_encryption(ct_t c, gamma_t gamma, gmp_randstate_t rs, mpz_t b)
{
  mpz_urandommv(c, rs, GAMMA_LOGQ, GAMMA_N);
  mpz_set(c[GAMMA_N], b);
}

void regev_decrypt(mpz_t m, gamma_t gamma, sk_t sk, ct_t ct)
{
  mpz_dotp(m, gamma.q, ct, sk, GAMMA_N);
  mpz_neg(m, m);
  mpz_add(m, ct[GAMMA_N], m);
  mpz_mod_ui(m, m, GAMMA_P);
}



void ct_export(uint8_t *buf, ct_t ct)
{
  bzero(buf, CT_BLOCK);
  for (size_t i = 0; i < GAMMA_N+1; i++) {
    mpz_export(&buf[i*LOGQ_BYTES], NULL, -1, sizeof(uint8_t), -1, 0, ct[i]);
  }
}


void ct_import(ct_t ct, uint8_t *buf)
{
  for (size_t i = 0; i < GAMMA_N+1; i++) {
    mpz_import(ct[i], LOGQ_BYTES, -1, sizeof(uint8_t), -1, 0, &buf[i*LOGQ_BYTES]);
  }
}

/**
 * Compute the scalar product of a ciphertext (mod q) times a plaintext (mod p).
 */
void ct_mul(ct_t rop, gamma_t gamma, ct_t a, mpz_t b)
{
  assert(mpz_cmp_ui(b, GAMMA_P) < 0);

  for (size_t i = 0; i != GAMMA_N+1; i++) {
    mpz_mul(rop[i], a[i], b);
    modq(rop[i]);
  }
}

void ct_mul_ui(ct_t rop, gamma_t gamma, ct_t a, uint64_t b)
{
  assert(b < GAMMA_P);

  for (size_t i = 0; i < GAMMA_N+1; i++) {
    mpz_mul_ui(rop[i], a[i], b);
    modq(rop[i]);
  }
}

void ct_add(ct_t rop, gamma_t gamma, ct_t a, ct_t b)
{
  for (size_t i = 0; i < GAMMA_N+1; i++) {
    mpz_add(rop[i], a[i], b[i]);
    mpz_mod(rop[i], rop[i], gamma.q);
  }
}


#define fail_if_error() do {                    \
    if (errno > 0) {                            \
      perror("Failed" __FILE__ );               \
      exit(EXIT_FAILURE);                       \
    }                                           \
  } while(0)


void eval_fd(ct_t rop, gamma_t gamma, int cfd, mpz_t coeff[static 1], size_t d)
{
  ct_t ct;
  ct_init(ct);

  const size_t length = d * CT_BLOCK;
  uint8_t *c8 = mmap(NULL, length, PROT_READ, MAP_PRIVATE, cfd, 0);
  madvise(c8, length, MADV_SEQUENTIAL);

  for (size_t i = 0; i < d; i++) {
    ct_import(ct, &c8[i * CT_BLOCK]);
    ct_mul(ct, gamma, ct, coeff[i]);
    ct_add(rop, gamma, rop, ct);
  }

  munmap(c8, length);
  ct_clear(ct);
}


void eval_poly(ct_t rop, gamma_t gamma, uint8_t *c8, nmod_poly_t p, size_t d)
{
  ct_t ct;
  ct_init(ct);

  for (size_t i = 0; i < d; i++) {
    ct_import(ct, &c8[i * CT_BLOCK]);
    ct_mul_ui(ct, gamma, ct, nmod_poly_get_coeff_ui(p, i));
    ct_add(rop, gamma, rop, ct);
  }

  ct_clear(ct);
}
