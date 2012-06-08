#include <stdlib.h>
#include <assert.h>
#include "gmpis.h"

iset_t GMPISInit(void)
{
  iset_t s;

  s = (iset_t) malloc(sizeof(*s));
  assert(s);
  mpz_init(*s);

  return s;
}

void GMPISAdd(iset_t s, size_t i)
{
  mpz_setbit(*s,(mp_bitcnt_t) i);
}

iset_t GMPISInterset(iset_t a, iset_t b)
{
  iset_t r;

  r = GMPISInit();
  mpz_and(*r,*a,*b);

  return r;
}

size_t GMPISNext(iset_t s, size_t last){
  return (size_t) mpz_scan1(*s,last);
}

void GMPISDestroy(iset_t s)
{
  mpz_clear(*s);
  free(s);
}
