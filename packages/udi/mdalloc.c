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

#ifndef __APPLE__
#  define MEMFLAGS MAP_PRIVATE | MAP_ANONYMOUS
#else
#  define MEMFLAGS MAP_PRIVATE | MAP_ANON
/* MACOS does not provide mremap so we emulate what we need */
void *mremap_compat(int fd, void *oldaddr, size_t oldlen, size_t newlen,
		    int prot, int flags);
#endif

#define DISKFLAGS MAP_SHARED
#define MEMPROT PROT_READ | PROT_WRITE

long mdpagesize = 0;

mdalloc_t MDInit (int fd, int prot)
{
  mdalloc_t m;
  struct stat buffer;

  m = (mdalloc_t ) malloc (sizeof(*m));
  assert(m);
  memset((void *) m,0, sizeof(*m));
  m->fd = fd;
  m->prot = prot;

  if (mdpagesize == 0)
    {
      mdpagesize = sysconf(_SC_PAGESIZE);
    }

  /*alloc first page*/
  m->size = mdpagesize;

  if (fd > 2) /* disk based*/
    {
      if (!prot)
        {
          perror("MDInit: in Disk mode prot mode is mandatory.");
          free(m);
          return NULL;
        }

      if (!(prot & PROT_WRITE)) /*not in write mode*/
        {
          if (fstat(fd, &buffer) == -1)
            {
              perror("MDInit stat");
              free(m);
              return NULL;
            }
          m->size = buffer.st_size;
        }

      m->region = (void *) mmap(NULL, m->size, prot,
                                DISKFLAGS, fd, 0);
      if (m->region == MAP_FAILED)
        {
          perror("MDInit mmap");
          free(m);
          return NULL;
        }
    }
  else /*memory based allways in Write Mode*/
    {
      m->region = (void *) mmap(NULL, m->size, MEMPROT, MEMFLAGS, -1, 0);
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

  assert(m && m->region);

  oldsize = m->size;
  m->size += mdpagesize;

  /* fprintf(stderr, "Alloc: %zu %zu\n", oldsize, m->size); */

  if (m->fd > 2) /*disk based*/
    {
      if ((m->prot & PROT_WRITE)) /*in write mode*/
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

#ifdef __APPLE__
      /* disk remap*/
      m->region = (void *) mremap_compat(m->fd, m->region, oldsize, m->size,
                                         m->prot, DISKFLAGS);
    }
  else
    {
      /* memory remap */
      m->region = (void *) mremap_compat(m->fd, m->region, oldsize, m->size,
                                         MEMPROT, MEMFLAGS);
    }
#else
    }
  /*in linux just use remap the prot and flags are maitained*/
  m->region = (void *) mremap(m->region, oldsize, m->size, MREMAP_MAYMOVE);
#endif
  if (m->region == MAP_FAILED)
    {
      perror("MDAlloc mremap");
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

#ifdef __APPLE__

//m->region = (void *) mremap(m->region, oldsize, m->size, MREMAP_MAYMOVE);
//void *mremap(void *old_address, size_t old_size, size_t new_size, int flags);
/*
 * Idea found in https://dank.qemfd.net/bugzilla/show_bug.cgi?id=119
 */
// This is suitable really only for use here,
// due to assumptions it makes about the flags to pass to mmap(2). The only
// mremap(2) use case addressed is that of MREMAP_MAYMOVE. oldaddr must be a
// valid previous return from mmap(); NULL is not acceptable (ala Linux's
// mremap(2)), resulting in undefined behavior, despite realloc(3) semantics.
// Similarly, oldlen and newlen must be non-zero (and page-aligned).
void *mremap_compat(int fd, void *oldaddr, size_t oldlen, size_t newlen,
		    int prot, int flags)
{
  void *ret;

  // From mmap(2) on freebsd 6.3: A successful FIXED mmap deletes any
  // previous mapping in the allocated address range. This means:
  // remapping over a current map will blow it away (unless FIXED isn't
  // provided, in which case it can't overlap an old mapping. See bug
  // 733 for extensive discussion of this issue for Linux and FreeBSD).
  if((ret = mmap((char *)oldaddr + oldlen, newlen - oldlen,
		 prot, flags, fd, oldlen)) == MAP_FAILED){
    // We couldn't get the memory whatsoever (or we were a fresh
    // allocation that succeeded). Return the immediate result...
    return ret;
  } // ret != MAP_FAILED. Did we squash?
  if(ret != (char *)oldaddr + oldlen){
    // We got the memory, but not where we wanted it. Copy over the
    // old map, and then free it up...
    //nag("Wanted %p, got %p\n",(char *)oldaddr + oldlen,ret);
    munmap(ret,newlen - oldlen);
    if((ret = mmap(NULL,newlen,prot,flags,fd,0)) == MAP_FAILED){
                        return ret;
    }
    memcpy(ret,oldaddr,oldlen);
    munmap(oldaddr,oldlen); // Free the old mapping
    return ret;
  } // We successfully squashed. Return a pointer to the first buf.
  return oldaddr;
}
#endif
