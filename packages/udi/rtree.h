#ifndef __RTREE_H__
#define __RTREE_H__

#ifndef __RTREE_PRIVATE_H__
typedef void * rtree_t;

struct Rect
{
  double coords[4]; /* xmin, ymin, xmax, ymax */
};
typedef struct Rect rect_t;

typedef size_t index_t;
#endif

/*
 * Alocates and initializes a new rtree structure
 */
extern rtree_t RTreeNew (void);

/*
 * Inserts in the rtree the object with MBR (mbr)
 */
extern void RTreeInsert (rtree_t rtree, rect_t mbr, index_t object);


/* CallBack of the search function
 *
 * It is called with the found mbr and object, and auxiliary data
*/
typedef int (*SearchHitCallback)(void *mbr, index_t object, void *data);

/* Searchs the rtree for objects that overlap mbr
 *
 * In each object found callback_f is called with the found
 * object, mbr and auxiliary (void *) data
*/
extern int RTreeSearch (rtree_t t, rect_t mbr,
                        SearchHitCallback callback_f, void *data);

/*
 * Destroys rtree, freeing all the memory allocated to it
 */
extern void RTreeDestroy (rtree_t rtree);

/*
 * Debug function, prints rtree
 */
extern void RTreePrint(rtree_t rtree);

/*
 * Initializes an infinity MBR
 */
extern rect_t RectInit (void);

#endif /*__RTREE_H__*/
