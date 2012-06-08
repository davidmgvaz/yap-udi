#ifndef __RTREE_PRIVATE_H__
#define __RTREE_PRIVATE_H__ 1

#include "common.h"

#include "mdalloc.h"
/* This RTree uses mdalloc to andle space allocations which allows to work both
 * in memory or in disk
 */

#define NUMDIMS 2 /* we will work in 2d changing this will
                     break some functions */

struct Rect
{
  double coords[2*NUMDIMS]; /* x1min, y1min, ... , x1max, y1max, ...*/
};
typedef struct Rect rect_t;

typedef size_t index_t;

struct Branch
{
  rect_t mbr;
  index_t child;
  /*This RTree will allways hold index_t both in branches and leaves*/
};
typedef struct Branch branch_t;

struct Node
{
  int count;
  int level;
  branch_t branch[FLEXIBLE_SIZE];
};
typedef struct Node * node_t;
#define SIZEOF_NODE(maxcard) SIZEOF_FLEXIBLE(struct Node, branch, maxcard)

struct RTreeInfo
{
  /* info on the tree structure */
  size_t maxcard;
  size_t mincard;

  /* root node index */
  size_t nidx;
};
typedef struct RTreeInfo * rtreeinfo_t;

typedef mdalloc_t rtree_t;
/* rtree info is allways stored in the first page
 * of m
 * (rtreeinfo_t) t->region
 */
#define RTREEINFO(t) ((rtreeinfo_t) t->region)

#define MAXCARD(t) (RTREEINFO(t)->maxcard)
#define MINCARD(t) (RTREEINFO(t)->mincard)

#define NODE(t,index) ((node_t) MDGET(t,index))

#define ROOTINDEX(t) (RTREEINFO(t)->nidx)
#define ROOTNODE(t) (NODE(t, ROOTINDEX(t)))

struct Partition
{
  int n;
  rect_t cover_all;
  rect_t cover[2];
  size_t maxcard;
  branch_t buffer[FLEXIBLE_SIZE];
};
typedef struct Partition * partition_t;
#define SIZEOF_PARTITION(maxcard) \
  SIZEOF_FLEXIBLE(struct Partition, buffer, maxcard + 1)

/* #define ALIGN(addr, size) (((addr)+(size-1))&(~(size-1))) */

#include "rtree.h"

static size_t RTreeNewNode (rtree_t);
static void RTreeNodeInit (rtree_t, index_t);

/* static void RTreeDestroyNode (rtree_t, index_t); */

static int RTreeSearchNode (rtree_t, index_t,
                            rect_t, SearchHitCallback, void *);

static int RTreeInsertNode (rtree_t, index_t,
                            int,
                            rect_t, index_t,
                            index_t *);
static int RTreeAddBranch(rtree_t, index_t,
                          branch_t, index_t *);
static int RTreePickBranch (rtree_t, index_t,
                            rect_t);
static void RTreeSplitNode (rtree_t, index_t,
                            branch_t, index_t *);

static void RTreePickNext(rtree_t, partition_t, index_t, index_t);
static void RTreePickSeeds(rtree_t, partition_t, index_t, index_t);
static void RTreeNodeAddBranch(rtree_t, rect_t *, index_t, branch_t);

static rect_t RTreeNodeCover(rtree_t, index_t);

static double RectArea (rect_t);
static rect_t RectCombine (rect_t, rect_t);
static int RectOverlap (rect_t, rect_t);

static partition_t PartitionNew (size_t);
static void PartitionPush (partition_t, branch_t);
static branch_t PartitionPop (partition_t);
static branch_t PartitionGet (partition_t, int);

void RTreePrint_(rtree_t t, index_t i);
static void RectPrint (rect_t);

/*both pointers and offets are allways > 0
 because offset 0 stores rtree info */
#define EMPTYBRANCH(b) (b.child == 0)


#endif /* __RTREE_PRIVATE_H__ */
