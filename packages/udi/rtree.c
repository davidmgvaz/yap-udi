#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#include "rtree.h"

static size_t RTreeNewNode (rtree_t);
static void RTreeDestroyNode (rtree_t, index_t);
static void RTreeNodeInit (rtree_t, index_t);

static int RTreeSearchNode (rtree_t, index_t, 
                            rect_t, SearchHitCallback, void *);
static int RTreeInsertNode (rtree_t, index_t, 
                            int, 
                            rect_t,void *,
                            index_t *);

static int RTreePickBranch (rtree_t, rect_t, index_t);
static int RTreeAddBranch(rtree_t, index_t,
                          branch_t, index_t *);
static void RTreeSplitNode (rtree_t, index_t, 
                            branch_t, index_t *);

static void RTreePickSeeds(rtree_t, partition_t *, index_t, index_t);
static void RTreeNodeAddBranch(rtree_t, rect_t *, index_t, branch_t);
static void RTreePickNext(rtree_t, partition_t *, index_t, index_t);

static rect_t RTreeNodeCover(rtree_t, index_t);

static double RectArea (rect_t);
static rect_t RectCombine (rect_t, rect_t);
static int RectOverlap (rect_t, rect_t);
static void RectPrint (rect_t);

static partition_t PartitionNew (void);
static void PartitionPush (partition_t *, branch_t);
static branch_t PartitionPop (partition_t *);
static branch_t PartitionGet (partition_t *, int);

static inline node_t NODE(rtree_t t, size_t ndx)
{
  return ((node_t) MDGET(t, ndx));
}

static inline index_t INDEX(rtree_t t, node_t n)
{
  return MDINDEX(t, (void *) n);
}

static inline void * DATA(index_t ndx)
{
  return ((void *) ndx);
}

static inline branch_t * NODEBRANCH(rtree_t t, index_t ndx, size_t bdx)
{
  return ((branch_t *) (MDGET(t, ndx) + 
                        2 * sizeof(int) + 
                        bdx * sizeof(branch_t)));
}

static inline int EMPTYNODEBRANCH(rtree_t t, index_t ndx, size_t bdx)
{
  return (NODEBRANCH(t, ndx, bdx)->child == 0);
}

#define ROOTNODE(t) (NODE(t, ROOTINDEX(t)))
/*both pointers and offets are allways > 0
 because offset 0 stores rtree info */
#define EMPTYBRANCH(b) (b.child == 0)

rtree_t RTreeNew (void)
{
  rtree_t t;
  size_t n;
  
  t = MDInit(-1,0);
  /* first page to store tree info */
  MDAlloc (t);
  MAXCARD_(t) = (mdpagesize - 2*sizeof(int)) / sizeof(branch_t);
  MINCARD_(t) = MAXCARD_(t) / 2;

  n = RTreeNewNode(t);
  /*t->region might change after RTreeNewNode so
    ROOTINDEX(t) = RTreeNewNode(t); would segfault */
  ROOTINDEX(t) = n;

  ROOTNODE(t)->level = 0; /*leaf*/

  { int bdx;

    fprintf(stderr, "sizeof(int)=%zu, sizeof(branch_t)=%zu\n\n",
            sizeof(int), sizeof(branch_t));

    fprintf(stderr, "node: %p\n",
            ROOTNODE(t));
    fprintf(stderr, "count: %p\n",
            &(ROOTNODE(t)->count));
    fprintf(stderr, "level: %p\n",
            &(ROOTNODE(t)->level));
            
    for(bdx = 0; bdx < MAXCARD_(t); bdx++)
      fprintf(stderr, "%d: %p %p\n", bdx,
              &(ROOTNODE(t)->branch[bdx]),
              NODEBRANCH(t,ROOTINDEX(t), bdx));
    fprintf(stderr, "sizeof node_t=%zu, %zu\n", SIZEOF_FLEXIBLE(struct Node, branch, MAXCARD_(t)), sizeof(struct Node));
    fprintf(stderr, "sizeof rect_t=%zu, index_t=%zu\n", sizeof(rect_t), sizeof(index_t));
    fprintf(stderr, "node0=%p, node1=%p\n", 
            (node_t) t->region, (node_t) (t->region + mdpagesize));
    fprintf(stderr, "align\nnode0=%p, node1=%p\n", 
            (node_t) t->region, (node_t) ALIGN((long int) t->region + SIZEOF_FLEXIBLE(struct Node, branch, MAXCARD_(t)),mdpagesize));
    fprintf(stderr, "align\nnode0=%p, node1=%p\n", 
            (node_t) ALIGN((long int) t->region + 0,mdpagesize),
            (node_t) ALIGN((long int) t->region + 1,mdpagesize));
  }

  return t;
}

static size_t RTreeNewNode (rtree_t t)
{
  index_t ndx;

  ndx = MDAlloc(t);
  RTreeNodeInit(t,ndx);

  return ndx;
}

static void RTreeNodeInit (rtree_t t, index_t index)
{
  node_t n;

  n = NODE(t,index);
  memset((void *) n,0, SIZEOF_NODE(MAXCARD_(t)));

  n->level = -1;
}

void RTreeDestroy (rtree_t t)
{
  if (t)
    RTreeDestroyNode (t,ROOTINDEX(t));
  /*TODO: mmap clean*/
}

static void RTreeDestroyNode (rtree_t t, index_t index)
{
  int i;
  node_t node;

  node = NODE(t,index);
  
  if (node->level == 0) /* leaf level*/
    {
      for (i = 0; i < MAXCARD_(t); i++)
        if (node->branch[i].child)
          ;/* allow user free data*/
        else
          break;
    }
  else
    {
      for (i = 0; i < MAXCARD_(t); i++)
        if (!EMPTYBRANCH(node->branch[i]))
          RTreeDestroyNode (t, node->branch[i].child);
        else
          break;
    }
}

int RTreeSearch (rtree_t t, rect_t s, SearchHitCallback f, void *arg)
{
  assert(t);
  assert(ROOTNODE(t));
  return RTreeSearchNode(t,ROOTINDEX(t),s,f,arg);
}

static int RTreeSearchNode (rtree_t t, index_t index, 
                            rect_t s, SearchHitCallback f, void *arg)
{
  int i;
  int c = 0;
  node_t n;

  n = NODE(t,index);

  if (n->level > 0)
    {
      for (i = 0; i < MAXCARD_(t); i++) {
        assert(&(n->branch[i]) == NODEBRANCH(t,index,i));
        if (!EMPTYBRANCH(n->branch[i]) &&
            RectOverlap (s,n->branch[i].mbr))
          c += RTreeSearchNode (t, n->branch[i].child, s, f, arg);
      }
    }
  else
    {
      for (i = 0; i < MAXCARD_(t); i++) {
        assert(&(n->branch[i]) == NODEBRANCH(t,index,i));
        if (!EMPTYBRANCH(n->branch[i]) &&
            RectOverlap (s,n->branch[i].mbr))
          {
            c ++;
            if (f)
              if ( !f(n->branch[i].mbr,DATA(n->branch[i].child),arg))
                return c;
          }
      }
    }
  return c;
}

void RTreeInsert (rtree_t t, rect_t r, void *data)
{
  index_t n2;
  index_t new_root;
  branch_t b;
  assert(t && ROOTNODE(t));

  if (RTreeInsertNode(t, ROOTINDEX(t), 0, r, data, &n2))
    /* deal with root split */
    {
      new_root = RTreeNewNode(t);
      NODE(t,new_root)->level = ROOTNODE(t)->level + 1;

      b.mbr = RTreeNodeCover(t, ROOTINDEX(t));
      b.child = ROOTINDEX(t);
      RTreeAddBranch(t, new_root, b, NULL);

      b.mbr = RTreeNodeCover(t, n2);
      b.child = n2;
      RTreeAddBranch(t, new_root, b, NULL);

      ROOTINDEX(t) = new_root;
    }
}

static int RTreeInsertNode (rtree_t t, index_t index, 
                            int level,
                            rect_t r, void *data,
                            index_t *new_index)
{
  int i;
  index_t n2;
  branch_t b;
  int split;

  assert(NODE(t,index) && new_index);
  assert(level >= 0 && level <= NODE(t,index)->level);
  
  if (NODE(t,index)->level > level)
    {
      i = RTreePickBranch(t, r, index);
      split = RTreeInsertNode(t, NODE(t,index)->branch[i].child, level,
                              r, data, &n2);
      if (!split)
        {
          NODE(t,index)->branch[i].mbr = RectCombine(r,NODE(t,index)->branch[i].mbr);
          return FALSE;
        }
      else /* node split */
        {
          NODE(t,index)->branch[i].mbr = RTreeNodeCover(t, NODE(t,index)->branch[i].child);
          b.child = n2;
          b.mbr = RTreeNodeCover(t, n2);
          return RTreeAddBranch(t, index, b, new_index);
        }
    }
  else /*insert level*/
    {
      b.mbr = r;
      b.child = (size_t) data;
      return RTreeAddBranch(t, index, b, new_index);
    }
}

static int RTreeAddBranch(rtree_t t, index_t index, branch_t b, 
                          index_t *new_index)
{
  int i;

  if (NODE(t,index)->count < MAXCARD_(t))
    /*split not necessary*/
    {
      for (i = 0; i < MAXCARD_(t); i++)
        if (EMPTYBRANCH(NODE(t,index)->branch[i]))
          {
            NODE(t,index)->branch[i] = b;
            NODE(t,index)->count ++;
            break;
          }
      return FALSE;
    }
  else /*needs to split*/
    {
      assert(new_index);
      RTreeSplitNode (t, index, b, new_index);
      return TRUE;
    }
}

static int RTreePickBranch (rtree_t t, rect_t r, index_t index)
{
  int i;
  double area;
  double inc_area;
  rect_t tmp;
  int best_i;
  double best_inc;
  double best_i_area;

  best_i = 0;
  best_inc = DBL_MAX; /* double Max value */
  best_i_area = DBL_MAX;

  for (i = 0; i < MAXCARD_(t); i++)
    if (!EMPTYBRANCH(NODE(t,index)->branch[i]))
      {
        area = RectArea (NODE(t,index)->branch[i].mbr);
        tmp = RectCombine (r, NODE(t,index)->branch[i].mbr);
        inc_area = RectArea (tmp) - area; 

        if (inc_area < best_inc)
          {
            best_inc = inc_area;
            best_i = i;
            best_i_area = area;
          }
        else if (inc_area == best_inc && best_i_area > area)
          {
            best_inc = inc_area;
            best_i = i;
            best_i_area = area;
          }
      }
    else
      break;
  return best_i;
}

static void RTreeSplitNode (rtree_t t, index_t index, 
                            branch_t b, index_t *new_index)
{
  partition_t p;
  int level;
  int i;

  assert(new_index);

  p = PartitionNew();

  for (i = 0; i < MAXCARD_(t); i ++)
    PartitionPush(&p,NODE(t,index)->branch[i]);
  PartitionPush(&p,b);

  level = NODE(t,index)->level;
  RTreeNodeInit(t,index);
  NODE(t,index)->level = level;
  *new_index = RTreeNewNode(t);
  NODE(t,(*new_index))->level = level;

  RTreePickSeeds(t, &p, index, *new_index);

  while (p.n) {
    if (NODE(t,index)->count + p.n <= MINCARD_(t))
      /* first group (n) needs all entries */
      RTreeNodeAddBranch(t, &(p.cover[0]), index, PartitionPop(&p));
    else if ( NODE(t,(*new_index))->count + p.n <= MINCARD_(t))
      /* second group (new_node) needs all entries */
      RTreeNodeAddBranch(t, &(p.cover[1]), *new_index, PartitionPop(&p));
    else 
      RTreePickNext(t, &p, index, *new_index);
  }
}

static void RTreePickNext(rtree_t t, partition_t *p,
                          index_t index1, index_t index2)
/* linear version */
{
  branch_t b;
  double area[2], inc_area[2];
  rect_t tmp;

  b = PartitionPop(p);

  area[0] = RectArea (p->cover[0]);
  tmp = RectCombine (p->cover[0], b.mbr);
  inc_area[0] = RectArea (tmp) - area[0];

  area[1] = RectArea (p->cover[1]);
  tmp = RectCombine (p->cover[1], b.mbr);
  inc_area[1] = RectArea (tmp) - area[1]; 

  if (inc_area[0] < inc_area[1] ||
      (inc_area[0] == inc_area[1] && area[0] < area[1]))
    RTreeNodeAddBranch(t, &(p->cover[0]),index1,b);
  else
    RTreeNodeAddBranch(t, &(p->cover[1]),index2,b);
}

static void RTreePickSeeds(rtree_t t,partition_t *p, index_t index1, index_t index2)
/* puts in index 0 of each node the resulting entry, forming the two
   groups
   This is the linear version
*/
{
  int dim,high, i;
  int highestLow[NUMDIMS], lowestHigh[NUMDIMS];
  double width[NUMDIMS];
  int seed0, seed1;
  double sep, best_sep;

  assert(p->n == MAXCARD_(t) + 1);

  for (dim = 0; dim < NUMDIMS; dim++)
    {
      high = dim + NUMDIMS;
      highestLow[dim] = lowestHigh[dim] = 0;
      for (i = 1; i < MAXCARD_(t) +1; i++)
        {
          if (p->buffer[i].mbr.coords[dim] >
              p->buffer[highestLow[dim]].mbr.coords[dim])
            highestLow[dim] = i;
          if (p->buffer[i].mbr.coords[high] < 
              p->buffer[lowestHigh[dim]].mbr.coords[high])
            lowestHigh[dim] = i;
        }
      width[dim] = p->cover_all.coords[high] - p->cover_all.coords[dim];
      assert(width[dim] >= 0);
    }

  seed0 = lowestHigh[0];
  seed1 = highestLow[0];
  best_sep = 0;
  for (dim = 0; dim < NUMDIMS; dim ++)
    {
      high = dim + NUMDIMS;
      
      sep = (p->buffer[highestLow[dim]].mbr.coords[dim] -
             p->buffer[lowestHigh[dim]].mbr.coords[high]) / width[dim];
      if (sep > best_sep)
        {
          seed0 = lowestHigh[dim];
          seed1 = highestLow[dim];
          best_sep = sep;
        }
    }
/*   assert (seed0 != seed1); */
  if (seed0 > seed1)
    {
      RTreeNodeAddBranch(t, &(p->cover[0]),index1,PartitionGet(p,seed0));
      RTreeNodeAddBranch(t, &(p->cover[1]),index2,PartitionGet(p,seed1));
    }
  else if (seed0 < seed1)
    {
      RTreeNodeAddBranch(t, &(p->cover[0]),index1,PartitionGet(p,seed1));
      RTreeNodeAddBranch(t, &(p->cover[1]),index2,PartitionGet(p,seed0));
    }
}

static void RTreeNodeAddBranch(rtree_t t, rect_t *r, index_t index, branch_t b)
{
  int i;

  for (i = 0; i < MAXCARD_(t); i++)
    if (EMPTYBRANCH(NODE(t,index)->branch[i]))
      {
        NODE(t,index)->branch[i] = b;
        NODE(t,index)->count ++;
        break;
      }
  *r = RectCombine(*r,b.mbr);
}


void RTreePrint(rtree_t t, index_t index)
{
  int i;
  node_t n;

  n = NODE(t,index);
  /*  printf("rtree([_,_,_,_,_]).\n"); */
  printf("rtree(%zu,%d,%d,[",index,n->level,n->count);
  for (i = 0; i < MAXCARD_(t); i++)
    {
      if (!EMPTYBRANCH(n->branch[i]))
        {
          printf("(%zu,",n->branch[i].child);
                   RectPrint(n->branch[i].mbr);
          printf(")");
        }
      else
        {
          printf("nil");
        }
      if (i < MAXCARD_(t)-1)
        printf(",");
    }
  printf("]).\n");

  if (n->level != 0)
    for (i = 0; i < MAXCARD_(t); i++)
      if (!EMPTYBRANCH(n->branch[i]))
        RTreePrint(t, n->branch[i].child);
      else
        break;
}

/*
 * Partition related
 */

static partition_t PartitionNew (void)
{
  partition_t p;
  memset((void *) &p,0, sizeof(p));
  p.cover[0] = p.cover[1] = p.cover_all = RectInit();
  return p;
}

static void PartitionPush (partition_t *p, branch_t b)
{
  assert(p->n < MAXCARD + 1);
  p->buffer[p->n] = b;
  p->n ++;
  p->cover_all = RectCombine(p->cover_all,b.mbr);
}

static branch_t PartitionPop (partition_t *p)
{
  assert(p->n > 0);
  p->n --;
  return p->buffer[p->n];
}

static branch_t PartitionGet (partition_t *p, int n)
{
  branch_t b;
  assert (p->n > n);
  b = p->buffer[n];
  p->buffer[n] = PartitionPop(p);
  return b;
}

/*
 * Rect related
 */

rect_t RectInit (void)
{
  rect_t r = {{DBL_MAX, DBL_MAX, DBL_MIN, DBL_MIN}};
  return (r);
}

static double RectArea (rect_t r)
{
  int i;
  double area;

  for (i = 0,area = 1; i < NUMDIMS; i++)
    area *= r.coords[i+NUMDIMS] - r.coords[i];

/*   area = (r.coords[1] - r.coords[0]) *  */
/*     (r.coords[3] - r.coords[2]); */

  return area;
}

static rect_t RectCombine (rect_t r, rect_t s)
{
  int i;
  rect_t new_rect;

  for (i = 0; i < NUMDIMS; i++)
    {
      new_rect.coords[i] = MIN(r.coords[i],s.coords[i]);
      new_rect.coords[i+NUMDIMS] = MAX(r.coords[i+NUMDIMS],s.coords[i+NUMDIMS]);
    }
  
  return new_rect;
}

static int RectOverlap (rect_t r, rect_t s)
{
  int i;
  
  for (i = 0; i < NUMDIMS; i++)
    if (r.coords[i] > s.coords[i + NUMDIMS] ||
        s.coords[i] > r.coords[i + NUMDIMS])
      return FALSE;
  return TRUE;
}

static rect_t RTreeNodeCover(rtree_t t, index_t index)
{
  int i;
  rect_t r = RectInit();

  for (i = 0; i < MAXCARD_(t); i++)
    if (!EMPTYBRANCH(NODE(t,index)->branch[i]))
      {
        r = RectCombine (r, NODE(t,index)->branch[i].mbr);
      }
    else
      break;

  return r;
}

static void RectPrint (rect_t r)
{
  int i;

  printf("[");
  for (i = 0; i < 2*NUMDIMS; i++)
    {
      printf("%f",r.coords[i]);
      if ( i < 2*NUMDIMS - 1)
        printf(",");
    }
  printf("]");
}
