/*chamada a cada index/2
  controi estrutura de control, para definir a indexação, contem a
  rtree p.e. 
  retorna a estrutura de control
*/
typedef void *
(* Yap_UdiInit)(Term  spec, /* mode spec */
		void *pred, /* pass predicate information */
		int   arity);

/*chamada a cada assert*/
typedef void *
(* Yap_UdiInsert)(Term t, /* termo asserted */
		  void *control, /* estrutura de control*/
		  void *clausule); /* valor a guardar na arvore, para retornar na pesquisa */

/*Callbeck for each value found in search*/
typedef int (* YAP_UdiCallback) (void *key, size_t data, void *arg);

/* chamada cada vez que um predicado indexado aparece no código
   Returns:
       NULL quando não há indexação usavel no predicado (fallback to
yap indexing)
       FALSE
       TRY_RETRY_TRUST quando há resultados positivos
*/
typedef void *
(* Yap_UdiSearch)(void * control, YAP_UdiCallback, void *);

/* chamada para destruir a estrutura de indexação, libertanto os recursos
   utilizados
*/
typedef int
(* Yap_UdiDestroy)(void * control);

typedef struct udi_control_block {
  Yap_UdiInit   init;
  Yap_UdiInsert insert;
  Yap_UdiSearch search;
  Yap_UdiDestroy destroy;
} *UdiControlBlock;

#include "mdalloc.h"
/*this is implemented as linked list 
  one element for each indexed argument
*/
struct Control
{
  int arg; /*argument index*/
  void *pred; /*predicate*/
  void *tree; /*indexing struture*/
  mdalloc_t clausules; /*to store clausule list*/
  UdiControlBlock controlblock; /*access to indexer functions*/
  struct Control *next;
};
typedef struct Control * control_t;

/* MAIN UDI calls
 */
/* initialzes Udi main struture
   depending on specific spec (e.g. a(rtree,btree,-))
   it initializes the necessary udi indexers 

   comforms to Yap_UdiInit
*/
control_t UdiInit (Term spec, void * pred, int arity);

/* calls each initialized indexer insert
   conforms to Yap_UdiInsert
*/
control_t UdiInsert (Term term, control_t control,void *clausule);

/* search call, calls each indexer for results, then it performs
   the intersection on the results and returns 
   conforms to Yap_UdiSearch
*/
void *UdiSearch (control_t control, YAP_UdiCallback callback, void *data);

/* calls each initialized indexer destroy
   conforms to Yap_UdiDestroy
*/
int UdiDestroy(control_t control);
