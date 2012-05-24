#ifndef _MDALLOC_
#define _MDALLOC_ 1

struct MDAlloc
{
  /* 
   * fd (file descriptor) and flags are used to distinguich between:
   *  - memory based allocation using MAP_ANONYMOUS
   *  - disk based allocation
   */
  int fd;
  int flags;
  size_t size;
  void * region;
};
typedef struct MDAlloc * mdalloc_t;

/*
 * Memory based allocation: MDInit(0,0);
 * Disk based allocation: MDInit(fd, flags)
 *   flags: PROT_* from mmap
 *   (must  not  conflict with the open mode of the file)
 */
extern mdalloc_t MDInit (int fd, int flags);

/*
 * Destroys MDAlloc, clearing memory and flushing do disk if necessary
 */
extern void MDDestroy (mdalloc_t);

/*
 * Allocates a new page 
 *   returns the index of allocated page
 */
extern size_t MDAlloc (mdalloc_t);

/*
 * The next functions are volatile. The results are only
 * valid while no allocation is made
 */

/*
 * fetches the address of a given index page in MDAlloc
 */
extern inline void * MDGET(mdalloc_t, size_t index);

/*
 * Returns the index of the address in MDAlloc
 */
extern inline size_t MDINDEX(mdalloc_t, void * address);

/*
 * Next functions test the validity od the address/index
 * within the MDAlloc structure 
 */
extern inline int MDVALID(mdalloc_t,void * address);
extern inline int MDVALIDI(mdalloc_t,size_t index);

#endif /* _MDALLOC_ */
