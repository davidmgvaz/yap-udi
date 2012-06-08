#ifndef __BTREE_PRIVATE_H__
#define __BTREE_PRIVATE_H__ 1

#include "common.h"

#include "mdalloc.h"
/* This B+Tree uses mdalloc to andle space allocations which allows to work both
 * in memory or in disk
 */

/* TODO: REMOVE THIS*/
#define PGSIZE 4096
#define MAXCARD (int) ((PGSIZE-(2*sizeof(int))) / (2 * sizeof(void*)))
#define MINCARD (MAXCARD / 2)

typedef size_t index_t;

struct Branch
{
  double key;
  index_t child;
  /*This B+Tree will allways hold index_t both in branches and leaves*/
};
typedef struct Branch branch_t;

struct Node
{
  int count;
  int level;
  /* double key[MAXCARD - 1]; */
  /* void * branch[MAXCARD]; */

  /* we do not use one key with this representation*/
  branch_t branch[FLEXIBLE_SIZE]; /*in leaf nodes last child is ptr to next node
                                    for fast in order run */
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

#include "b+tree.h"

static index_t BTreeNewNode (btree_t);
static void BTreeNodeInit (btree_t, index_t);

static int BTreeInsertNode(btree_t, index_t, double *, index_t *);
static int BTreePickBranch(btree_t, index_t, double);
static int BTreeAddBranch(btree_t, index_t, int, double *, index_t *);
static int BTreeAddLeaf(btree_t, index_t, double *, index_t *);
/* static void BTreeDestroyNode (node_t n); */
static index_t BTreeMin_(btree_t, index_t, index_t *, int *);
static index_t BTreeMax_(btree_t, index_t, index_t *, int *);
static index_t BTreeSearch_(btree_t, index_t, double, int, index_t *, int *);

void BTreePrint_(btree_t t, index_t nidx);

#endif /* __BTREE_PRIVATE_H__ */
