#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <gmp.h>

#include "entropy.h"
#include "lwe.h"
#include "timeit.h"

void benchmark_encrypt()
{
  rng_t rng;
  RNG_INIT(rng);

  sk_t sk;
  key_gen(sk);

  mpz_t m;
  mpz_init(m);

  ct_t c;
  ct_init(c);

  INIT_TIMEIT();
  for (size_t i = 0; i < 1e4; i++) {
    mpz_set_ui(m, rand_modp());
    START_TIMEIT();
    regev_encrypt(c, rng, sk, m);
    END_TIMEIT();
    printf("encryption\t" TIMEIT_FORMAT "\n", GET_TIMEIT());

    START_TIMEIT();
    regev_decrypt(m, sk, c);
    END_TIMEIT();
    printf("decryption\t" TIMEIT_FORMAT "\n", GET_TIMEIT());
  }

  key_clear(sk);
  mpz_clear(m);
  ct_clear(c);
  rng_clear(rng);

}


int main() {
  benchmark_encrypt();
}
