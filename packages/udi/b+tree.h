#ifndef _BTREE_
#define _BTREE_

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

#define PGSIZE 4096
#define MAXCARD (int) ((PGSIZE-(2*sizeof(int))) / (2 * sizeof(void*)))
#define MINCARD (MAXCARD / 2)

/* TODO: CHECK size_t needs to be the same size as void */
typedef size_t index_t;

struct Branch
{
  double key;
  index_t child;  /*in leaf nodes last is ptr to next in order node*/
};
typedef struct Branch branch_t;

#include <stddef.h>
#define FLEXIBLE_SIZE 1
#define SIZEOF_FLEXIBLE(type, member, length) \
  ( offsetof(type, member) + (length) * sizeof ((type *)0)->member[0] )

struct Node
{
  int count;
  int level;
  /* double key[MAXCARD - 1]; */
  /* void * branch[MAXCARD]; */

  /* one key discarded with this representation*/
  branch_t branch[FLEXIBLE_SIZE];
};
typedef struct Node * node_t;
#define SIZEOF_NODE(maxcard) SIZEOF_FLEXIBLE(struct Node, branch, maxcard)

struct BTreeInfo
{
  /* info on the tree structure */
  size_t maxcard;
  size_t mincard;
  
  /* root node index */
  size_t nidx;
};
typedef struct BTreeInfo * btreeinfo_t;

#include "mdalloc.h"
typedef mdalloc_t btree_t;

/* b+tree info is allways stored in the first page
 * of m
 * (btreeinfo_t) t->region
 */
#define BTREEINFO(t) ((btreeinfo_t) t->region)

#define MAXCARD_(t) (BTREEINFO(t)->maxcard)
#define MINCARD_(t) (BTREEINFO(t)->mincard)

#define NODE(t,index) ((node_t) MDGET(t,index))

#define ROOTINDEX(t) (BTREEINFO(t)->nidx)
#define ROOTNODE(t) (NODE(t, ROOTINDEX(t)))

struct Range
{
  double min;
  int le;
  double max;
  int ge;
};
typedef struct Range range_t;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

extern btree_t BTreeNew (void);
extern void BTreeInsert (btree_t, double, void *);

extern void *BTreeMin(btree_t, index_t *, int *);
extern void *BTreeMax(btree_t, index_t *, int *);

#define EQ 1
#define LE 2
#define LT 3
#define GE 4
#define GT 5

extern void *BTreeSearch(btree_t, double, int, index_t *, int *);

extern void *BTreeSearchNext (double, int, btree_t, index_t *, int *);

extern void BTreeDestroy (btree_t);

extern void BTreePrint(btree_t, index_t);

#endif /* _BTREE_ */
