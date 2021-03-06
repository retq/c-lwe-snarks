#pragma once
#include "config.h"

#include <unistd.h>
#include <strings.h>

#include <gmp.h>

#include "gmp-impl.h"
#include "aes.h"


/* Export getrandom(2) even for old version of glibc.
 * If this fails to compile, you probably should update your kernel.
*/
#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>

#else
#include <sys/syscall.h>
#include <linux/random.h>

static inline ssize_t
getrandom(void *buffer, size_t length, unsigned int flags)
{
  return syscall(SYS_getrandom, buffer, length, flags);
}

#endif


/** Random number genration **/

/* a rseed_t is 32 bytes for the key + 8 bytes for the key */
typedef uint8_t rseed_t[32 + 8];
typedef aesctr_t rng_t[1];

void rng_init(rng_t rs, uint8_t *rseed);
void rng_clear(rng_t rs);
#define RNG_INIT(rs) do {                               \
    rseed_t rseed;                                      \
    getrandom(&rseed, sizeof(rseed_t), GRND_NONBLOCK);  \
    rng_init(rs, rseed);                                \
    bzero(&rseed, sizeof(rseed_t));                     \
  } while(0)


void rng_seek(rng_t prg, size_t count);


static inline void rng_gen(rng_t prg, void *out, size_t count)
{
  aesctr_prg((aesctr_ptr) prg, out, count);
}

void mpz_entropy_init();


void mpz2_urandomb(mpz_ptr rop, rng_t prg, size_t nbits);
void mpz2_urandomb2(mpz_ptr rop, size_t nbits);

#define mpz2_urandommv(vs, rng, bits, len) do {         \
    for (size_t i = 0; i < len; i++) {                  \
      mpz2_urandomb(vs[i], rng, bits);                  \
    }                                                   \
  } while (0)

#define mpz2_urandombv2(vs, bits, len) do {     \
    for (size_t i = 0; i < len; i++) {          \
      mpz2_urandomb2(vs[i], bits);              \
    }                                           \
  } while (0)
