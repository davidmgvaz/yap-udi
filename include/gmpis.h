#ifndef __GMP_IS_H__
#define __GMP_IS_H__ 1

/* in this version we will use gmp bits to represent integer set*/
typedef mpz_t * iset_t;

#define NOMORE (~((mp_bitcnt_t) 0))

extern iset_t GMPISInit(void);

extern void GMPISAdd(iset_t, size_t);

extern iset_t GMPISInterset(iset_t, iset_t);

extern size_t GMPISNext(iset_t, size_t last);

extern void GMPISDestroy(iset_t);

#endif /* __GMP_IS_H__ */
