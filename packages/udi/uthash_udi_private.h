#ifndef _UTHASH_UDI_PRIVATE_
#define _UTHASH_UDI_PRIVATE_

#include "uthash.h"
#include "Yap.h"

typedef size_t index_t;

struct UTHash
{
  Atom atom;
  index_t index;
  UT_hash_handle hh;
};
typedef struct UTHash uthash_t;

/*
  Used to Iterate over equal keys in hash table
*/
#define HASH_FIND_NEXT(hh,last,keyptr,keylen_in)                        \
  do {                                                                  \
    if (last->hh.hh_next)                                               \
      DECLTYPE_ASSIGN(last,ELMT_FROM_HH(last->hh.tbl,last->hh.hh_next)); \
    else last = NULL;                                                   \
    while (last) {                                                      \
      if (last->hh.keylen == keylen_in) {                               \
        if ((HASH_KEYCMP(last->hh.key,keyptr,keylen_in)) == 0) {        \
          break;                                                        \
        }                                                               \
      }                                                                 \
      if (last->hh.hh_next)                                             \
        DECLTYPE_ASSIGN(last,ELMT_FROM_HH(last->hh.tbl,last->hh.hh_next)); \
      else last = NULL;                                                 \
    }                                                                   \
  } while (0)                                                           \

/* to ease code for a Atom hash table*/
#define HASH_FIND_ATOM(head,findatom,out)               \
  HASH_FIND(hh,head,findatom,sizeof(Atom),out)
#define HASH_ADD_ATOM(head,add)                 \
  HASH_ADD(hh,head,atom,sizeof(Atom),add)
#define HASH_FIND_NEXT_ATOM(last,findint)       \
  HASH_FIND_NEXT(hh,last,findint,sizeof(Atom))

#endif /* _UTHASH_UDI_PRIVATE_ */
