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

  if (mdpagesize == 0)
    {
      mdpagesize = sysconf(_SC_PAGESIZE);
    }

  if (fd > 2) /* disk based*/
    {
      if (fstat(fd, &buffer) == -1)
        {
          perror("MDInit stat");
          free(m);
          return NULL;
        }
      m->size = buffer.st_size;
      m->region = (void *) mmap(NULL, m->size, flags, 
                                MAP_SHARED, fd, 0);
      if (m->region == MAP_FAILED)
        {
          perror("MDInit mmap");
          free(m);
          return NULL;
        }
    } 
  else /*memory based*/
    {
      m->size = 0;
      /*size 1 is a hack just to initialize a mmap region*/
      m->region = (void *) mmap(NULL, 1, PROT_READ | PROT_WRITE, 
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
}

size_t MDAlloc (mdalloc_t m)
/*allocs new page returning index*/
{
  int r;
  size_t nsize;
  size_t index;

  assert(m && m->region);
  
  nsize = m->size + mdpagesize;

  if (m->fd > 2) /*disk based*/
    {
      /*strech undelying file*/
      r = lseek(m->fd, nsize - 1, SEEK_SET);
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

  /*expand*/
  m->region = mremap(m->region, m->size, nsize, MREMAP_MAYMOVE);
  if (m->region == MAP_FAILED)
    {
      perror("MDAlloc mremap");
      return 0;
    }

  m->size = nsize;
  index = m->size/mdpagesize;

  return index;
}

inline void * MDGET(mdalloc_t m, size_t index)
{
  return (m->region + (index - 1) * mdpagesize);
}

inline size_t MDINDEX(mdalloc_t m, void * address)
{
  return ((address - m->region) / mdpagesize + 1);
}

inline int MDVALID(mdalloc_t m,void * address)
{
  return ((address >= m->region) && (address < m->region + m->size));
}

inline int MDVALIDI(mdalloc_t m,size_t index)
{
  return ((index > 0) && (index <= m->size / mdpagesize));
}
