#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <YapInterface.h>

#include "Yap.h"

#include "udi.h"

#include "rtree.h"
#include "clause_list.h"
#include "rtree_udi_i.h"
#include "rtree_udi.h"

/* integer set */
#include "gmpis.h"

static int YAP_IsNumberTermToFloat (Term term, YAP_Float *n)
{
  if (YAP_IsIntTerm (term) != FALSE)
  {
    if (n != NULL)
      *n = (YAP_Float) YAP_IntOfTerm (term);
    return (TRUE);
  }
  if (YAP_IsFloatTerm (term) != FALSE)
  {
    if (n != NULL)
      *n = YAP_FloatOfTerm (term);
    return (TRUE);
  }
  return (FALSE);
}

static rect_t RectOfTerm (Term term)
{
  YAP_Term tmp;
  rect_t rect;
  int i;

  if (!YAP_IsPairTerm(term))
    return (RectInit());

  for (i = 0; YAP_IsPairTerm(term) && i < 4; i++)
    {
      tmp = YAP_HeadOfTerm (term);
      if (!YAP_IsNumberTermToFloat(tmp,&(rect.coords[i])))
        return (RectInit());
      term = YAP_TailOfTerm (term);
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

  r = RectOfTerm(YAP_ArgOfTerm(control->arg,term));
  /*TODO: remove latter*/
  if (!control->tree)
    control->tree = RTreeNew();
  RTreeInsert(control->tree,r,clausule);
}

/*ARGS ARE AVAILABLE*/
int RtreeUdiSearch (control_t control, Yap_UdiCallback callback, void * arg)
{
  rect_t r;
  YAP_Term t, Constraints;

  t = YAP_A(control->arg);
  if (YAP_IsAttVar(t))
    {
      /*get the constraits rect*/
      Constraints = YAP_AttsOfVar(t);
      /* Yap_DebugPlWrite(Constraints); */
      if (YAP_IsApplTerm(Constraints))
        {
          r = RectOfTerm(YAP_ArgOfTerm(2,Constraints));
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
