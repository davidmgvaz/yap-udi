#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "b+tree.h"

static index_t BTreeNewNode (btree_t);
static void BTreeNodeInit (btree_t, index_t);
static int BTreeInsertNode(btree_t, index_t, double *, void **);
static int BTreePickBranch(btree_t, index_t, double);
static int BTreeAddBranch(btree_t, index_t, int, double *, void **);
static int BTreeAddLeaf(btree_t, index_t, double *, void **);
/* static void BTreeDestroyNode (node_t n); */
static void *BTreeMin_(btree_t, index_t, index_t *, int *);
static void *BTreeMax_(btree_t, index_t, index_t *, int *);
static void *BTreeSearch_(btree_t, index_t, double, int, index_t *, int *);

mdalloc_t m;

btree_t BTreeNew (void)
{
  btree_t t;
  index_t n;

  t = MDInit(-1,0);
  /* first page stores tree info */
  MAXCARD_(t) = (mdpagesize - 2*sizeof(int)) / sizeof(branch_t);
  MINCARD_(t) = MAXCARD_(t) / 2;

  n = BTreeNewNode(t);
  /*t->region might change after BTreeNewNode so
    ROOTINDEX(t) = BTreeNewNode(t); would segfault */
  ROOTINDEX(t) = n;

  NODE(t,n)->level = 0; /*leaf*/

  return t;
}

static index_t BTreeNewNode (btree_t t)
{
  index_t ndx;

  ndx = MDAlloc(t);
  BTreeNodeInit(t, ndx);

  return ndx;
}

static void BTreeNodeInit (btree_t t, index_t nidx)
{
  node_t n;

  n = NODE(t,nidx);

  memset((void *) n, 0, SIZEOF_NODE(MAXCARD_(t)));

  n->level = -1;
}

void BTreeDestroy (btree_t t)
{
  /* if (t) */
  /*   BTreeDestroyNode (t); */
  MDDestroy(t);
}

/* static void BTreeDestroyNode (node_t n) */
/* { */
/*   int i; */

/*   if (NODELEVEL(n) == 0) */
/*     { */
/*       for (i = 0; i < NODECOUNT(n); i++) */
/*         ;/\* allow user free data*\/ */
/*     } */
/*   else */
/*     { */
/*       for (i = 0; i < NODECOUNT(n); i++) */
/*         BTreeDestroyNode (NODEBRANCH(n,i)); */
/*     } */
/* } */

void BTreeInsert (btree_t t, double k, void *ptr)
{
  index_t new_root;

  assert(t && ROOTNODE(t));

  if (BTreeInsertNode(t, ROOTINDEX(t), &k, &ptr))
    /* deal with root split */
    {
      new_root = BTreeNewNode(t);
      NODE(t,new_root)->level = ROOTNODE(t)->level + 1;

      NODE(t,new_root)->count = 1;
      NODE(t,new_root)->branch[0].key = k;
      NODE(t,new_root)->branch[0].child = ROOTINDEX(t);
      NODE(t,new_root)->branch[1].child = (index_t) ptr;

      ROOTINDEX(t) = new_root;
    }
}

static int BTreeInsertNode(btree_t t, index_t nidx,
                           double *k, void **ptr)
/*ptr holds data and can return node_t*/
{
  int i;
  node_t n;

  n = NODE(t,nidx);
  assert(n);

  if (n->level > 0)
    {
      i = BTreePickBranch(t, nidx, *k);
      if (!BTreeInsertNode(t, n->branch[i].child, k, ptr))
        /*not split */
        {
          return FALSE;
        }
      else 
        /* node split */
        {
          return BTreeAddBranch(t, nidx, i, k, ptr); /*propagate split*/
        }
    }
  else
    {
      return BTreeAddLeaf(t, nidx, k, ptr);
    }
}

static int BTreeAddBranch(btree_t t, index_t nidx,
                          int idx, 
                          double *k, void **ptr)
{
  int i,j;
  double key[MAXCARD];
  index_t branch[MAXCARD+1];
  int level;
  index_t nidx1;
  node_t n1;
  node_t n;

  n = NODE(t,nidx);

  if (n->count < MAXCARD - 1)
    {      
      i = n->count;
      if (i > 0)
        /*shift to get space*/
        for(; n->branch[i-1].key > *k ; i--)
          {
            n->branch[i].key = n->branch[i-1].key;
            n->branch[i+1].child = n->branch[i].child;
          }
      n->branch[i].key = *k;
      n->branch[i+1].child = (index_t) *ptr;
      n->branch[i].child = n->branch[idx].child;
      n->count ++;
      return FALSE;
    }
  else
    {
      for(i = n->count, j = MAXCARD_(t); 
          n->branch[i-1].key > *k; 
          i--, j--)
        { 
          key[j - 1] = n->branch[i-1].key;
          branch[j] = n->branch[i].child;
        }
      key[j - 1] = *k;
      branch[j - 1] = n->branch[idx].child;
      branch[j] = (index_t) *ptr;
      j--;
      for(; i > 0;i--,j--)
        {
          key[j-1] = n->branch[i-1].key;
          branch[j-1] = n->branch[i-1].child;
        }

      level = n->level;
      BTreeNodeInit(t,nidx);
      n->level = level;

      nidx1 = BTreeNewNode(t);
      /* account for possible node pointer change*/
      n = NODE(t,nidx);
      n1 = NODE(t,nidx1);

      n1->level = level;

      for (i = 0; i < MAXCARD / 2; i ++)
        {
          n->branch[i].key = key[i];
          n->branch[i].child = branch[i];
          n->count ++;
        }
      n->branch[i].child = branch[i];

      *k = key[i];
      *ptr = (void *) nidx1;

      for (j = 0, i++; i < MAXCARD; j ++, i ++)
        {
          n1->branch[j].key = key[i];
          n1->branch[j].child = branch[i];
          n1->count ++;
        }
      n1->branch[j].child = branch[i];

      return TRUE;
    }
}

static int BTreePickBranch(btree_t t, index_t nidx, double k)
{
  node_t n;
  int i;

  n = NODE(t,nidx);
  for (i = 0; i < n->count; i++)
    if (n->branch[i].key > k)
      return i;
  return i;
}

static int BTreeAddLeaf(btree_t t, index_t nidx,
                        double *k, void **ptr)
{
  int i,j;
  double key[MAXCARD];
  index_t branch[MAXCARD];
  node_t n;
  index_t nidx1;
  node_t n1;

  n = NODE(t, nidx);
  assert(n);
  
  if (n->count < MAXCARD_(t) - 1) /*split not necessary*/
    {
      i = n->count;
      if (i > 0)
        for (; n->branch[i-1].key > *k; i--)
          {
            n->branch[i].key = n->branch[i-1].key;
            n->branch[i].child = n->branch[i-1].child;
          }
      n->branch[i].key = *k;
      n->branch[i].child = (index_t) *ptr;
      n->count ++;
      return FALSE;
    }
  else /*needs to split*/
    {
      for(i = n->count - 1, j = MAXCARD - 1; 
          n->branch[i].key > *k; 
          i--, j--)
        {
          key[j] = n->branch[i].key;
          branch[j] = n->branch[i].child;
        }
      key[j] = *k;
      branch[j] = (index_t) *ptr;
      j--;
      for(; i >= 0;i--,j--)
        {
          key[j] = n->branch[i].key;
          branch[j] = n->branch[i].child;
        }
      
      n->count = 0;

      nidx1 = BTreeNewNode(t);
      /* account for possible node pointer change*/
      n = NODE(t,nidx);
      n1 = NODE(t,nidx1);

      n1->level = n->level;

      for (i = 0; i <= MAXCARD_(t) / 2; i ++)
        {
          n->branch[i].key = key[i];
          n->branch[i].child = branch[i];
          n->count ++;
        }
      *k = key[i-1];
      *ptr = (void *) nidx1;
      for (j = 0; i < MAXCARD; j ++, i ++)
        {
          n1->branch[j].key = key[i];
          n1->branch[j].child = branch[i];
          n1->count ++;
        }

      /*linked list*/
      n1->branch[MAXCARD_(t)-1].child = n->branch[MAXCARD_(t)-1].child;
      n->branch[MAXCARD_(t)-1].child = nidx1;

    return TRUE;
    }
}


void *BTreeMin (btree_t t, index_t *f, int *i)
{
  return BTreeMin_(t,ROOTINDEX(t), f, i);
}

void *BTreeMin_ (btree_t t, index_t nidx,
                index_t *f, int *i)
{
  node_t n;

  n = NODE(t, nidx);
  
  if (n->level > 0)
    return BTreeMin_(t, n->branch[0].child, f, i);
  else
    {
      if (n->count > 0)
        {
          if(f)
            *f = nidx;
          if(i)
            *i = 0;
          return (void *) n->branch[0].child;
        }
    }
  if (f)
    *f = 0;
  if (i)
    *i = -1;
  return NULL;
}

void *BTreeMax (btree_t t, index_t *f, int *i)
{
  return BTreeMax_(t, ROOTINDEX(t), f, i);
}

void *BTreeMax_ (btree_t t, index_t nidx,
                index_t *f, int *i)
{
  node_t n;
  
  n = NODE(t,nidx);
  if (n->level > 0)
    return BTreeMax_(t, n->branch[n->count].child, f, i);
  else  
    {
      if (n->count > 0)
        {
          if(f)
            *f = nidx;
          if(i)
            *i = n->count - 1;
          return (void *) n->branch[n->count -1].child;
        }
    }
  if (f)
    *f = 0;
  if (i)
    *i = -1;
  return NULL;
}

void * BTreeSearch (btree_t t, double k, int s, index_t *f, int *i)
{
  return BTreeSearch_(t, ROOTINDEX(t), k, s, f, i);
}

void * BTreeSearch_ (btree_t t, index_t nidx, 
                    double k, int s, index_t *f, int *i)
{
  int j;
  node_t n;

  assert(s == EQ || s == GE || s == GT);

  n = NODE(t,nidx);
  if (n->level > 0)
    {
      for (j = 0; j < n->count; j++)
        if (n->branch[j].key >= k)
          return BTreeSearch_(t, n->branch[j].child, k, s, f, i);
      return BTreeSearch_(t, n->branch[j].child, k, s, f, i);
    }
  else
    {
      if (s == EQ || s == GE) /*== or >=*/
        for (j = 0; j < n->count; j++)
          if (n->branch[j].key == k)
            {
              if (f)
                *f = nidx;
              if (i)
                *i = j;
              return (void *)n->branch[j].child;
            }
      if (s == GE || s == GT) /* >= or > */
        {
          if (f)
            *f = nidx;
          for (j = 0; j < n->count; j++)
            if (n->branch[j].key > k)
              {
                if (i)
                  *i = j;
                return (void *) n->branch[j].child;
              }
        }
    }
  if (f)
    *f = 0;
  if (i)
    *i = -1;
  return NULL;
}

void *BTreeSearchNext (double max, int s, 
                       btree_t t, index_t *nidx, int *i)
{
  node_t n;

  assert(nidx && i);
  assert(s == LT || s == LE);

  n = NODE(t, *nidx);
 
  if (*i == n->count - 1)
    {
      if (!n->branch[MAXCARD_(t) - 1].child) /*terminou*/
        return NULL;
      *nidx = n->branch[MAXCARD_(t) - 1].child;
      *i = 0;
      n = NODE(t, *nidx);
    }
  else
    (*i) ++;

  if (n->branch[*i].key > max ||
      (n->branch[*i].key == max && s == LT))
    return NULL;

  return (void *) n->branch[*i].child;
}

void BTreePrint(btree_t t, index_t nidx)
{
  int j;
  node_t n;

  n = NODE(t, nidx);

  printf("btree(%zu, %d, %d,\n[", nidx, n->level, n->count);
  for (j = 0; j < n->count; j++)
    printf("%lf,",n->branch[j].key);
  for (; j < MAXCARD_(t) - 1; j++)
    printf("nil,");
  printf("],\n[");

  for (j = 0; j < n->count; j++)
    printf("%zu,",n->branch[j].child);
  for (; j < MAXCARD_(t); j++)
    printf("nil,");
  printf("]).\n\n");

  if (n->level > 0)
    for (j = 0; j < n->count; j++)
      BTreePrint(t, n->branch[j].child);
}
