#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mdalloc.h"

long mdpagesize = 0;

mdalloc_t MDInit (int fd, int flags)
{
  mdalloc_t m;
  struct stat buffer;

  m = (mdalloc_t ) malloc (sizeof(*m));
  assert(m);
  memset((void *) m,0, sizeof(*m));
  m->fd = fd;
  m->flags = flags;

  if (mdpagesize == 0)
    {
      mdpagesize = sysconf(_SC_PAGESIZE);
    }

  /*alloc first page*/
  m->size = mdpagesize;

  if (fd > 2) /* disk based*/
    {
      if (!flags)
        {
          perror("MDInit: in Disk mode flags are mandatory.");
          free(m);
          return NULL;
        }

      if (!(flags & PROT_WRITE)) /*not in write mode*/
        {
          if (fstat(fd, &buffer) == -1)
            {
              perror("MDInit stat");
              free(m);
              return NULL;
            }
          m->size = buffer.st_size;
        }

      m->region = (void *) mmap(NULL, m->size, flags, 
                                MAP_SHARED, fd, 0);
      if (m->region == MAP_FAILED)
        {
          perror("MDInit mmap");
          free(m);
          return NULL;
        }
    } 
  else /*memory based allways in Write Mode*/
    {
      m->region = (void *) mmap(NULL, m->size, PROT_READ | PROT_WRITE, 
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (m->region == MAP_FAILED)
        {
          perror("MDInit mmap");
          free(m);
          return NULL;
        }
    }

  return m;
}

void MDDestroy (mdalloc_t m)
{
  int r;

  assert(m && m->region);
  
  r = munmap(m->region, m->size);
  if (r == -1)
    {
      perror("MDDestroy munmap");
    }

  free(m);
  m = NULL;
}

size_t MDAlloc (mdalloc_t m)
/*allocs new page returning index*/
{
  int r;
  size_t oldsize;
  void * address;

  assert(m && m->region);
  
  oldsize = m->size;
  m->size += mdpagesize;

  if (m->fd > 2) /*disk based*/
    {
      if ((m->flags & PROT_WRITE)) /*not in write mode*/
        {
          /*strech undelying file*/
          r = lseek(m->fd, m->size - 1, SEEK_SET);
          if (r == -1)
            {
              perror("MDAlloc fssek");
              return 0;
            }
          r = write(m->fd, "", 1);
          if (r == -1)
            {
              perror("MDAlloc write");
              return 0;
            }
        }
      else
        {
          perror("MDAlloc: in READ mode no allocation is allowed");
          return 0;
        }
    }

  /*expand*/
  address = m->region;
    m->region = (void *) mremap(m->region, oldsize, m->size, MREMAP_MAYMOVE);
  if (m->region == MAP_FAILED)
    {
      perror("MDAlloc mremap");
      fprintf(stderr, "%p, %zu, %zu, %d\n", 
              address, oldsize, m->size, MREMAP_MAYMOVE);
      return 0;
    }

  return oldsize;
}

inline void * MDGET(mdalloc_t m, size_t index)
{
  return (m->region + index);
}

inline size_t MDINDEX(mdalloc_t m, void * address)
{
  return (address - m->region);
}

inline int MDVALID(mdalloc_t m,void * address)
{
  return ((address >= m->region) && (address < m->region + m->size));
}

inline int MDVALIDI(mdalloc_t m,size_t index)
{
  return ((index > 0) && (index <= m->size));
}
