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

rtree_t RTreeNew (void)
{
  rtree_t t;

  /* fprintf(stderr,"sizeof(*node_t)=%zu\n", sizeof(struct Node)); */

  t = (rtree_t) malloc (sizeof(*t));
  assert(t);
  memset((void *) t,0, sizeof(*t));
  
  t->m = MDInit(-1,0);
  t->index = RTreeNewNode(t);

  ROOTNODE(t)->level = 0; /*leaf*/
  return t;
}

void RTreeDestroy (rtree_t t)
{
  if (t)
    RTreeDestroyNode (t,ROOTINDEX(t));
  /*TODO: mmap clean*/
}

static size_t RTreeNewNode (rtree_t t)
{
  index_t index;

  index = MDAlloc(t->m);

  {
    node_t n;
    n = NODE(t,index);
    assert(n);
    fprintf(stderr, "index=%2zu n=%p r(%p,%4zu,%p) \n",
            index, (void *) n,
            t->m->region,
            t->m->size,
            t->m->region + t->m->size -1);
  }

  RTreeNodeInit(t,index);

  return index;
}

static void RTreeDestroyNode (rtree_t t, index_t index)
{
  int i;
  node_t node;

  node = NODE(t,index);
  
  if (node->level == 0) /* leaf level*/
    {
      for (i = 0; i < MAXCARD; i++)
        if (node->branch[i].cld)
          ;/* allow user free data*/
        else
          break;
    }
  else
    {
      for (i = 0; i < MAXCARD; i++)
        if (!EMPTYBRANCH(node->branch[i]))
          RTreeDestroyNode (t, node->branch[i].cld);
        else
          break;
    }
}

static void RTreeNodeInit (rtree_t t, index_t index)
{
  node_t n = NODE(t,index);
  memset((void *) n,0, sizeof(*n));
  n->level = -1;
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
      for (i = 0; i < MAXCARD; i++)
        if (!EMPTYBRANCH(n->branch[i]) &&
            RectOverlap (s,n->branch[i].mbr))
          c += RTreeSearchNode (t, n->branch[i].cld, s, f, arg);
    }
  else
    {
      for (i = 0; i < MAXCARD; i++)
        if (!EMPTYBRANCH(n->branch[i]) &&
            RectOverlap (s,n->branch[i].mbr))
          {
            c ++;
            if (f)
              if ( !f(n->branch[i].mbr,DATA(t,n->branch[i].cld),arg))
                return c;
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

  /* fprintf(stderr, "RS index=%zu\n", ROOTINDEX(t)); */
  if (RTreeInsertNode(t, ROOTINDEX(t), 0, r, data, &n2))
    /* deal with root split */
    {
      /* fprintf(stderr, "New RS\n"); */

      new_root = RTreeNewNode(t);
      NODE(t,new_root)->level = ROOTNODE(t)->level + 1;

      b.mbr = RTreeNodeCover(t, ROOTINDEX(t));
      b.cld = ROOTINDEX(t);
      RTreeAddBranch(t, new_root, b, NULL);

      b.mbr = RTreeNodeCover(t, n2);
      b.cld = n2;
      RTreeAddBranch(t, new_root, b, NULL);

      ROOTINDEX(t) = new_root;
      /* fprintf(stderr, "RS index=%zu\n", ROOTINDEX(t)); */
    }

  /* RTreePrint(t,ROOTINDEX(t)); */
  /* printf("\n\n\n"); */
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

  /* assert(n && new_node); */
  /* assert(level >= 0 && level <= n->level); */
  
  if (NODE(t,index)->level > level)
    {
      i = RTreePickBranch(t, r, index);
      split = RTreeInsertNode(t, NODE(t,index)->branch[i].cld, level,
                              r, data, &n2);
      if (!split)
        {
          NODE(t,index)->branch[i].mbr = RectCombine(r,NODE(t,index)->branch[i].mbr);
          return FALSE;
        }
      else /* node split */
        {
          NODE(t,index)->branch[i].mbr = RTreeNodeCover(t, NODE(t,index)->branch[i].cld);
          b.cld = n2;
          b.mbr = RTreeNodeCover(t, n2);
          return RTreeAddBranch(t, index, b, new_index);
        }
    }
  else /*insert level*/
    {
      b.mbr = r;
      b.cld = (size_t) data;
      return RTreeAddBranch(t, index, b, new_index);
    }
}

static int RTreeAddBranch(rtree_t t, index_t index, branch_t b, 
                          index_t *new_index)
{
  int i;

  if (NODE(t,index)->count < MAXCARD) /*split not necessary*/
    {
      for (i = 0; i < MAXCARD; i++)
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

  for (i = 0; i < MAXCARD; i++)
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

  for (i = 0; i < MAXCARD; i ++)
    PartitionPush(&p,NODE(t,index)->branch[i]);
  PartitionPush(&p,b);

  level = NODE(t,index)->level;
  RTreeNodeInit(t,index);
  NODE(t,index)->level = level;
  *new_index = RTreeNewNode(t);
  NODE(t,(*new_index))->level = level;

  RTreePickSeeds(t, &p, index, *new_index);

  while (p.n) {
    if (NODE(t,index)->count + p.n <= MINCARD)
      /* first group (n) needs all entries */
      RTreeNodeAddBranch(t, &(p.cover[0]), index, PartitionPop(&p));
    else if ( NODE(t,(*new_index))->count + p.n <= MINCARD)
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

  assert(p->n == MAXCARD + 1);

  for (dim = 0; dim < NUMDIMS; dim++)
    {
      high = dim + NUMDIMS;
      highestLow[dim] = lowestHigh[dim] = 0;
      for (i = 1; i < MAXCARD +1; i++)
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

  for (i = 0; i < MAXCARD; i++)
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
  for (i = 0; i < MAXCARD; i++)
    {
      if (!EMPTYBRANCH(n->branch[i]))
        {
          printf("(%zu,",n->branch[i].cld);
                   RectPrint(n->branch[i].mbr);
          printf(")");
        }
      else
        {
          printf("nil");
        }
      if (i < MAXCARD-1)
        printf(",");
    }
  printf("]).\n");

  if (n->level != 0)
    for (i = 0; i < MAXCARD; i++)
      if (!EMPTYBRANCH(n->branch[i]))
        RTreePrint(t, n->branch[i].cld);
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

  for (i = 0; i < MAXCARD; i++)
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
