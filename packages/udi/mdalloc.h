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

extern long mdpagesize;

/*
 * MDAlloc initializes the allocation structure and if in write mode
 * allocates the first page (index 0).
 *
 * Memory based allocation: MDInit(0,0); fd and flags will be ignored
 * Disk based allocation: MDInit(fd, flags)
 *   flags: PROT_* from mmap
 *   (must  not  conflict with the open mode of the file)
 *
 * Disk based allocation with PROT_WRITE flag, it is assumed the file 
 * should be open with O_TRUNC, becouse no care is taken to shrink
 * unused space in an existing file.
 *
 * Memory based allocation allways assumes write mode.
 *
 * In Read mode MAP_SHARED is used to allow sharing of resources.
 */
extern mdalloc_t MDInit (int fd, int flags);

/*
 * Destroys MDAlloc, clearing memory and flushing do disk if necessary
 */
extern void MDDestroy (mdalloc_t);

/*
 * Allocates a new page 
 *   returns the index of allocated page 
 *
 *   indexes are allways >= 1
 *   0 is used on errors
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
