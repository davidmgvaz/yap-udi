#ifndef __UDI_INDEXERS_H__
#define __UDI_INDEXERS_H__ 1

#ifndef __UDI_INDEXERS_PRIVATE_H__
typedef void * controlext_t;
#endif

/* initialzes Udi main struture
   depending on specific spec (e.g. a(rtree,btree,-))
   it initializes the necessary udi indexers

   comforms to Yap_UdiInit
*/
controlext_t UdiInit (Term spec, void * pred, int arity);

/* calls each initialized indexer insert
   conforms to Yap_UdiInsert
*/
controlext_t UdiInsert (Term term, controlext_t control,void *clausule);

/* search call, calls each indexer for results, then it performs
   the intersection on the results and returns
   conforms to Yap_UdiSearch
*/
void *UdiSearch (controlext_t control);

/* calls each initialized indexer destroy
   conforms to Yap_UdiDestroy
*/
int UdiDestroy(controlext_t control);

#endif /* __UDI_INDEXERS_H__ */
