#ifndef _ALLOC_
#define _ALLOC_

struct MMap
{
  int fd;
  size_t size;
  void * region;
};
typedef struct MMap * mmap_t;

extern mmap_t MemoryInit (int fd);
/* extern void MemoryDestroy (mmap_t); */
extern size_t MemoryAlloc (mmap_t); /*allocs new page returning index*/

/*TODO FIX THIS*/
/* #define PAGE_SIZE 4096 */

#define MGET(m,index) (m->region + index * PAGE_SIZE)
#define MINDEX(m,p) ((p - m->region)/PAGE_SIZE)

#define VALID(m,index) (index < (m->size / PAGE_SIZE))

#endif /* _ALLOC_ */
