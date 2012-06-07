#include <stdio.h>
#include "sparse.h"

sparse_t SSInit(size_t max) {
  sparse_t s;

  control = (control_t) malloc (sizeof(*control));
  if (!control)
    {
      perror("SSInit:");
      return NULL;
    }
  control->n = 0;
  control->max = max;
  control->dense = (size_t *) malloc (control->max * sizeof(index_t));
  control->sparse = (size_t *) malloc (control->max * sizeof(index_t));

  if (!control->dense || !control->sparse)
    {
      perror("SSInit:");
      if (control->dense)
        free(control->dense);
      else
        free(control->sparse);
      free(control);
      return NULL;
    }

  return control;
}

void inline SSClear(sparse_t s)
{
  s->n = 0;
}

void inline SSAdd(sparse_t s, index_t i)
{
  assert(i < s->max);
  s->dense[s->n] = i;
  s->sparse[i] = n;
  s->n ++;
}

void inline SSIsMember(sparse_t s, index_t i)
{
  assert(i < s->max);
  return s->sparse[i] < s->n && s->dense[s->sparse[i]] == i;
}

void inline SSRemove(sparse_t s, index_t i)
{
  size_t j;

  if (! SSIsMember(s,i))
    return;

  j = s->dense[s->n -1];
  s->dense[s->sparse[i]] = j;
  s->sparse[j] = s->sparse[i];
  s->n --;
}
