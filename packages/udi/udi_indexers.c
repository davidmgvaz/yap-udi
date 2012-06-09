#include <assert.h>
#include "Yap.h"
#include "clause.h"
#include "clause_list.h"

#include "udi_indexers_private.h"
#include "gmpis.h"

/* controlext_t linked list maintenance */
static controlext_t add_control(int arg, void *pred, void *idxstr,
                             controlext_t old)
{
  controlext_t control;

  control = (controlext_t) Yap_AllocCodeSpace(sizeof(*control));
  if (!control)
    return NULL;

  control->arg = arg;
  control->pred = pred;
  control->tree = NULL;
  control->controlblock = idxstr;
  control->next = old;

  return control;
}

controlext_t UdiInit (Term spec, void * pred, int arity)
{
  int i, j, n;
  int valid_spec;
  Term arg;
  Atom idxtype;
  controlext_t control;
  mdalloc_t clausulelist;

  n =  sizeof (udi_indexers) / sizeof (struct UDIIndexers);
  if (!udi_indexers[0].atomdecl)
    { /*LookupAtom optimization*/
      for (i = 0; i < n; i++)
        udi_indexers[i].atomdecl = Yap_LookupAtom(udi_indexers[i].decl);
    }

  control=NULL;
  clausulelist=NULL;
  for (i = 1; i <= arity; i++)
    {
      arg = ArgOfTerm(i,spec);
      if (IsAtomTerm(arg))
        {
          idxtype = AtomOfTerm(arg);

          valid_spec = 0;
          for(j = 0; j < n; j++)
            {
              if (idxtype == udi_indexers[j].atomdecl)
                {
                  control = add_control(i, pred,
                                        (void *) &(udi_indexers[j].control),
                                        control);
                  valid_spec = 1;
                  if (!clausulelist)
                    clausulelist = MDInit(0,0);
                  /* TODO: check return value */
                  control->clausules = clausulelist;
                  break;
                }
            }
          if (!valid_spec && idxtype != AtomMinus)
            fprintf(stderr, "Invalid Spec (%s)\n", AtomName(idxtype));

        }
    }

  return control;
}

controlext_t UdiInsert (Term term,controlext_t control,void *clausule)
{
  controlext_t iter;
  /* int i; */
  int *index;
  void **clausules;

  assert(control);

  /*last clausule*/
  index = (int *) (control->clausules->region);
  if (*index >= ((control->clausules->size - sizeof(int)) / sizeof(void *)))
    {
      /* alloc a new page */
      MDAlloc(control->clausules);
      index = (int *) (control->clausules->region);
    }
  clausules = (void **) (control->clausules->region + sizeof(int));
  clausules[*index] = clausule;

  iter = control;
  while (iter)
    {
      iter->controlblock->insert(term, (control_t) iter, *index);
      iter = iter->next;
    }

  (*index) ++;
  return control;
}

static int callback(void *key, size_t data, void *arg)
{
  GMPISAdd((iset_t) arg, data);
  return TRUE;
}

void *UdiSearch (controlext_t control)
{
  controlext_t iter;
  iset_t result;
  int r;

  assert(control);

  iter = control;
  while (iter)
    {
      result = GMPISInit();
      r = control->controlblock->search((control_t) control, callback, (void *) result);
      if (r >= 0)
        {
          size_t last = 0;
          struct ClauseList clauselist;
          void **clausules;

          clausules = (void **) (control->clausules->region + sizeof(int));

          Yap_ClauseListInit(&clauselist);
          last = GMPISNext(result,last);

          while (last != NOMORE)
            {
              Yap_ClauseListExtend(&clauselist,
                                   clausules[last],
                                   control->pred);

              last = GMPISNext(result,++last);
            }

          Yap_ClauseListClose(&clauselist);

          if (Yap_ClauseListCount(&clauselist) == 0)
          {
            Yap_ClauseListDestroy(&clauselist);
            return Yap_FAILCODE();
          }

          if (Yap_ClauseListCount(&clauselist) == 1)
            {
              return Yap_ClauseListToClause(&clauselist);
            }

          return Yap_ClauseListCode(&clauselist);
        }
      GMPISDestroy(result);
      iter = iter->next;
    }
  return NULL;
}

int UdiDestroy (controlext_t control)
{

  assert(control);

  control->controlblock->destroy((control_t) control);

  return TRUE;
}
