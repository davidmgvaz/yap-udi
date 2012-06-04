#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "Yap.h"
#include "clause.h"
#include "YapInterface.h"
#include "udi.h"
#include "udi_.h"

static control_t add_control(int arg, void *pred, void *idxstr,
                       control_t old)
{
  control_t control;

  control = (control_t) malloc (sizeof(*control));
  assert(control);
  memset((void *) control,0, sizeof(*control));

  /* fprintf(stderr, "ADD:%p\n", idxstr); */

  control->arg = arg;
  control->pred = pred;
  control->controlblock = idxstr;
  control->next = old;

  return control;
}

control_t RtreeUdiInit (Term spec, void * pred, int arity);
control_t RtreeUdiInsert (Term term,control_t control,void *clausule);
void *RtreeUdiSearch (control_t control);
int RtreeUdiDestroy(control_t control);

control_t BtreeUdiInit (Term spec, void * pred, int arity);
control_t BtreeUdiInsert (Term term,control_t control,void *clausule);
void *BtreeUdiSearch (control_t control);
int BtreeUdiDestroy(control_t control);

struct IdxStructs
{
  struct udi_control_block control;
};

static struct IdxStructs idx_structs[] = {
  {
    {RtreeUdiInit, RtreeUdiInsert, RtreeUdiSearch, RtreeUdiDestroy}
  },
  {
  /*   Yap_LookupAtom("b+tree"),  */
    {BtreeUdiInit, BtreeUdiInsert, BtreeUdiSearch, BtreeUdiDestroy}
  }
};

control_t UdiInit (Term spec, void * pred, int arity)
{
  int i, j, n;
  Term arg;
  Atom idxtype;
  control_t control;

  control=NULL;
  for (i = 1; i <= arity; i++)
    {
      arg = YAP_ArgOfTerm(i,spec);
      Yap_DebugPlWrite(arg);
      fprintf(stderr, " %p %d\n", (void *) arg, YAP_IsAtomTerm(arg));
      if (YAP_IsAtomTerm(arg))
        {
          idxtype = YAP_AtomOfTerm(arg);

          fprintf(stderr,"%p(%s) == %p \\/ %p\n",
                  (void *) idxtype, YAP_AtomName(idxtype), 
                  (void *) AtomRTree, (void *) AtomBTree);
          if (idxtype == AtomRTree) /*hardcoded 0*/
            control = add_control(i, pred, 
                                  (void *) &(idx_structs[0].control),
                                  control);
          else if (idxtype == AtomBTree)
            control = add_control(i, pred, 
                                  (void *) &(idx_structs[1].control),
                                  control);
        }
    }

  /* fprintf(stderr,"%p\n", (void *) &(idx_structs[0].control)); */
  /* fprintf(stderr,"%p\n", (void *) control); */
  return control;
}

control_t UdiInsert (Term term,control_t control,void *clausule)
{
  control_t iter;
  assert(control);
  /* fprintf(stderr,"%p\n", (void *) control); */

  iter = control;
  while (iter)
    {
      iter->controlblock->insert(term, iter, clausule);
      iter = iter->next;
    }

  return control;
}

void *UdiSearch (control_t control)
{
  control_t iter;
  void * r;

  assert(control);

  iter = control;
  while (iter)
    {
      r = control->controlblock->search(control);
      if (r)
        return r;
      iter = iter->next;
    }
  return NULL;
}

control_t UdiDestroy (control_t control)
{

  assert(control);

  control->controlblock->destroy(control);

  return control;
}

