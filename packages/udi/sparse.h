/* WARNING: This code was not tested, in some aspects is pseudo-code */

struct SparseSet
{
  size_t n;
  size_t max; /*to keep track of allocated space*/
  size_t *dense;
  size_t *sparce;
};

typedef struct SparseSet sparse_t;

extern sparse_t SSInit(size_t max);

/*
  Very CPU efficient when compared with bitfields
  m bits and n elements
  Memorywise it needs 2 integers instead of o bit to
  represent the set elements.

  Form "An efficient representation for sparse sets"
  Preston Briggs and Linda Torczon 1993

  found in http://research.swtch.com/sparse

  The main trick consists is we do not need to initialize any
  of the arrays (sparse, dense) and continue to have O(1) correct 
  operations (see site for further examples)
*/

/*
  O(1) instead of usual O(m) clear
 */
extern void inline SSClear(sparse_t s);

/*
  O(1)
*/
extern void inline SSAdd(sparse_t s, index_t i);
extern void inline SSIsMember(sparse_t s, index_t i);
extern void inline SSRemove(sparse_t s, index_t i);

/*
  To iterate over elements O(n) instead of O(m)

  for (i = 0, i < s->n; i++)
     yield s->dense[i]
*/
