#include <assert.h>

#include "Yap.h"

#include "udi.h"

#include "rtree.h"
#include "rtree_udi.h"

/* Used only becouse of:
 * YAP_IsAttVar(t) and YAP_AttsOfVar(t)
 */
#include "YapInterface.h"

static int IsNumberTermToFloat (Term term, Float *n)
{
  if (IsIntTerm (term) != FALSE)
  {
    if (n != NULL)
      *n = (Float) IntOfTerm (term);
    return (TRUE);
  }
  if (IsFloatTerm (term) != FALSE)
  {
    if (n != NULL)
      *n = FloatOfTerm (term);
    return (TRUE);
  }
  return (FALSE);
}

static rect_t RectOfTerm (Term term)
{
  Term tmp;
  rect_t rect;
  int i;

  if (!IsPairTerm(term))
    return (RectInit());

  for (i = 0; IsPairTerm(term) && i < 4; i++)
    {
      tmp = HeadOfTerm (term);
      if (!IsNumberTermToFloat(tmp,&(rect.coords[i])))
        return (RectInit());
      term = TailOfTerm (term);
    }

  return (rect);
}

void RtreeUdiInit (control_t control){
  control->tree = RTreeNew();
}

void RtreeUdiInsert (Term term,control_t control,index_t clausule)
{
  rect_t r;

  assert(control);

  r = RectOfTerm(ArgOfTerm(control->arg,term));
  /*TODO: remove latter*/
  if (!control->tree)
    control->tree = RTreeNew();
  RTreeInsert(control->tree,r,clausule);
}

/*ARGS ARE AVAILABLE*/
int RtreeUdiSearch (control_t control, Yap_UdiCallback callback, void * arg)
{
  rect_t r;
  Term t, Constraints;

  t = Deref(XREGS[control->arg]); /*YAP_A(control->arg)*/
  if (YAP_IsAttVar(t))
    {
      /*get the constraits rect*/
      Constraints = YAP_AttsOfVar(t);
      /* Yap_DebugPlWrite(Constraints); */
      if (IsApplTerm(Constraints))
        {
          r = RectOfTerm(ArgOfTerm(2,Constraints));
        }
      else  /*hack to destroy udi*/
        {
          RTreeDestroy(control->tree);
          fprintf(stderr,"Destroy RTree\n");
          control->tree = NULL;
          return -1;
        }

      return RTreeSearch(control->tree, r, (SearchHitCallback) callback, arg);
    }
  return -1; /*YAP FALLBACK*/
}

void RtreeUdiDestroy(control_t control)
{
  assert(control);

  if (control->tree)
    RTreeDestroy(control->tree);
}
