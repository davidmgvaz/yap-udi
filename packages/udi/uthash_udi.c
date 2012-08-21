#include <assert.h>

#include "Yap.h"

#include "udi.h"

#include "uthash_udi.h"
#include "uthash_udi_private.h"

/* Used only becouse of:
 * YAP_IsAttVar(t) and YAP_AttsOfVar(t)
 */
#include "YapInterface.h"

struct ControlUT
{
  int arg; /* argument index */
  void *pred; /* predicate */
  /* TODO: change this name tree  is only valid for trees */
  uthash_t *tree; /* indexing struture */
};
typedef struct ControlUT * UTcontrol_t;


void UTHashUdiInit (control_t control){
  control->tree = NULL;
}

void UTHashUdiInsert (Term term,control_t control,index_t clausule)
{
  Term arg;
  uthash_t *element;
  UTcontrol_t utcontrol;

  assert(control);

  utcontrol = (UTcontrol_t) control;

  arg = ArgOfTerm(control->arg,term);

  if (YAP_IsAtomTerm(arg))
    {
      element = (uthash_t *) malloc(sizeof(uthash_t));
      element->atom = AtomOfTerm(arg);
      element->index = clausule;
      HASH_ADD_ATOM(utcontrol->tree, element);
      /* HASH_ADD(hh,hashtable,atom,sizeof(Atom),element); */
    }
}

/*ARGS ARE AVAILABLE*/
int UTHashUdiSearch (control_t control,
                     Yap_UdiCallback callback, void * callbackarg)
{
  Term arg;
  Atom atom;
  uthash_t *element;
  UTcontrol_t utcontrol;
  int count = 0;

  arg = Deref(XREGS[control->arg]); /*YAP_A(control->arg)*/
  utcontrol = (UTcontrol_t) control;

  if (YAP_IsAtomTerm(arg))
    {
      atom = AtomOfTerm(arg);


      HASH_FIND_ATOM(utcontrol->tree,&atom,element);
      /* HASH_FIND(hh,utcontrol->tree,&atom,sizeof(Atom),element); */
      while (element)
        {
          callback((void *) element->atom, element->index, callbackarg);
          count ++;
          HASH_FIND_NEXT_ATOM(element,&atom);
        }
      return (count);
    }
  return -1; /*YAP FALLBACK*/
}

void UTHashUdiDestroy(control_t control)
{
  uthash_t *hashtable;

  assert(control);
  hashtable = (uthash_t *) control->tree;

  if (control->tree)
    HASH_CLEAR(hh,hashtable); /* Todo: check if this is enough */
}
