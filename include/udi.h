#ifndef __UDI_H__
#define __UDI_H__ 1

struct Control
{
  int arg; /* argument index */
  void *pred; /* predicate */
  /* TODO: change this name tree  is only valid for trees */
  void *tree; /* indexing struture */
};
typedef struct Control * control_t;

/* Each udi indexer should provide the following functions */

/* Used to Initialize indexing structure */
typedef void (* Yap_UdiInit) (control_t);

/* Insert of each asserted term */
typedef void
(* Yap_UdiInsert) (Term t, /* asserted term*/
                   /*TODO: change this order*/
                   control_t control, /* control structure */
                   size_t index); /* clause index to store */

/* Callback for each value found in a search */
typedef int /* with a FALSE return should abort the search */
(* Yap_UdiCallback) (void *key, /*index key*/
                     size_t index, /*indexed clause*/
                     void *arg); /* auxiliary to callback */

/* This will actually perform the searh
 * Very Important: YAP ARGS are available
 */
typedef int /* number of found entries, return -1 when no search is possible */
(* Yap_UdiSearch) (control_t control, /* control structure */
                   Yap_UdiCallback f, /* callback on each found value*/
                   void *arg); /* auxiliary to callback */

/* To clear any memory used by this structure */
typedef void
(* Yap_UdiDestroy) (control_t control);

typedef struct udi_control_block {
  Yap_UdiInit   init;
  Yap_UdiInsert insert;
  Yap_UdiSearch search;
  Yap_UdiDestroy destroy;
} *UdiControlBlock;

#endif /* __UDI_H__ */
