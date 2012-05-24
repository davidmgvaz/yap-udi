#ifndef _RTREE_
#define _RTREE_

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

#define NUMDIMS 2 /* 2d */

struct Rect
{
  double coords[2*NUMDIMS]; /* x1min, y1min, ... , x1max, y1max, ...*/
};
typedef struct Rect rect_t;

/* TODO: CHECK size_t needs to be the same size as void */
typedef size_t index_t;

struct Branch
{
  rect_t mbr;
  index_t cld; /*void * so user can store whatever he needs, in case
   of non-leaf nodes it stores the index of child*/
};
typedef struct Branch branch_t;

/*both pointers and indexs are allways > 0*/
#define EMPTYBRANCH(b) (b.cld == 0)

#include "mdalloc.h"

#define PAGE_SIZE 4096
#define MAXCARD (int)((PAGE_SIZE-(2*sizeof(int)))/ sizeof(struct Branch))
#define MINCARD (MAXCARD / 2)

struct Node
{
  int count;
  int level;
  branch_t branch[MAXCARD];
};
typedef struct Node * node_t;

struct Rtree
{
  /* info on the tree structure */
  size_t maxcard;
  size_t mincard;
  
  /* root node index */
  index_t index;

  /* allocation structure */
  mdalloc_t m;
};
typedef struct Rtree * rtree_t;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* CallBack to search function */
typedef int (*SearchHitCallback)(rect_t r, void *data, void *arg);

extern rtree_t RTreeNew (void);
extern void RTreeInsert (rtree_t, rect_t, void *);
extern int RTreeSearch (rtree_t, rect_t, SearchHitCallback, void *);
extern void RTreeDestroy (rtree_t);
extern void RTreePrint(rtree_t, index_t);

extern rect_t RectInit (void);

struct Partition
{
  branch_t buffer[MAXCARD+1];
  int n;
  rect_t cover_all;
  rect_t cover[2];
};
typedef struct Partition partition_t;

#endif /* _RTREE_ */
