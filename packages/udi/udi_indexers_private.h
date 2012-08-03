#ifndef __UDI_INDEXERS_PRIVATE_H__
#define __UDI_INDEXERS_PRIVATE_H__

#include "udi.h"

#include "mdalloc.h"

/* this is implemented as linked list
 * one element for each indexed argument
 * WARNING: Should be consistent with control_t defined in udi.h
 */
struct ControlExt
{
  int arg; /* argument index */
  void *pred; /* predicate */
  /* TODO: change this name tree  is only valid for trees */
  void *tree; /* indexing struture */
  mdalloc_t clausules; /* clausule list */
  UdiControlBlock controlblock; /*access to indexer functions*/
  struct ControlExt *next;
};
typedef struct ControlExt * controlext_t;

/* include indexers */
#include "rtree_udi.h"
#include "b+tree_udi.h"

/* structure holding available indexers and access functions */
struct UDIIndexers
{
  char *decl;    /* declaration in udi init to use this indexer */
  Atom atomdecl; /* used in run time to store Yap_LookupAtom 
                    pointer for fast compare tests */
  struct udi_control_block control; /* */
};

/* New Indexers can be added here */
struct UDIIndexers udi_indexers[] = {
  {
    "rtree",
    NULL,
    {&RtreeUdiInit, &RtreeUdiInsert, &RtreeUdiSearch, &RtreeUdiDestroy}
  },
  {
    "btree",
    NULL,
    {&BtreeUdiInit, &BtreeUdiInsert, &BtreeUdiSearch, &BtreeUdiDestroy}
  }
};

#include "udi_indexers.h"

#endif /* __UDI_INDEXERS_PRIVATE_H__ */
