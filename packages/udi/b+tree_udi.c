#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#include "Yap.h"

#include "udi.h"

#include "b+tree.h"
#include "b+tree_udi.h"
#include "b+tree_udi_private.h"

/* TODO: Check this
 * Used only becouse of:
 * YAP_IsAttVar(t) and YAP_AttsOfVar(t)
 */
#include "YapInterface.h"

void BtreeUdiInit (control_t control){
  control->tree = BTreeNew();
}

void BtreeUdiInsert (YAP_Term term,control_t control,index_t clausule)
{
  assert(control);

  /*TODO: remove*/
  if (!control->tree)
    control->tree = BTreeNew();

  BTreeInsert(control->tree,
              YAP_FloatOfTerm(YAP_ArgOfTerm(control->arg,term)),
              clausule);
}

/*ARGS ARE AVAILABLE*/
int BtreeUdiSearch (control_t control, Yap_UdiCallback callback, void * arg)
{
  int j;
  size_t n;
  const char * att;
  Term t, Constraints;

  t = Deref(XREGS[control->arg]); /*YAP_A(control->arg)*/
  if (YAP_IsAttVar(t))
    {
      /*get the constraits rect*/
      Constraints = YAP_AttsOfVar(t);
      /* Yap_DebugPlWrite(Constraints); */

      att = YAP_AtomName(YAP_NameOfFunctor(YAP_FunctorOfTerm(Constraints)));

      n = sizeof (att_func) / sizeof (struct Att);
      for (j = 0; j < n; j ++)
        if (strcmp(att_func[j].att,att) == 0)
          return att_func[j].proc_att(control->tree,Constraints,callback,arg);
    }
  return 0;
}

/*Needs to test if tree is not null*/
int BTreeMinAtt (btree_t tree, Term constraint,
                  Yap_UdiCallback callback, void * arg)
{
  index_t d;

  /* printf("MIN\n"); */

  d = BTreeMin(tree,NULL,NULL);

  if (d)
    {
      callback((void *) &d, d, arg);
      return 1;
    }
  return 0;
}

int BTreeMaxAtt (btree_t tree, Term constraint,
                 Yap_UdiCallback callback, void * arg)
{
  index_t d;

  /* printf("MAX\n"); */

  d = BTreeMax(tree,NULL,NULL);

  if (d)
    {
      callback((void *) &d, d, arg);
      return 1;
    }
  return 0;
}

int BTreeEqAtt (btree_t tree, Term constraint,
                Yap_UdiCallback callback, void * arg)
{
  double min;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(tree,min,EQ,NULL,NULL);

  if (d)
    {
      callback((void *) &d, d, arg);
      return 1;
    }
  return 0;
}

int BTreeLtAtt (btree_t tree, Term constraint,
                Yap_UdiCallback callback, void * arg)
{
  double max;
  index_t n;
  int i, c = 0;
  index_t d;

  max = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeMin(tree,&n,&i);

  if (d)
    {
      callback((void *) &d, d, arg);
      c ++;

      while ((d = BTreeSearchNext(max,LT,tree,&n,&i)))
        {
          callback((void *) &d, d, arg);
          c ++;
        }
    }
  return c;
}

int BTreeLeAtt (btree_t tree, Term constraint,
                Yap_UdiCallback callback, void * arg)
{
  double max;
  index_t n;
  int i, c = 0;
  index_t d;

  max = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeMin(tree,&n,&i);

  if (d)
    {
      callback((void *) &d, d, arg);
      c ++;

      while ((d = BTreeSearchNext(max,LE,tree,&n,&i)))
        {
          callback((void *) &d, d, arg);
          c ++;
        }
    }
  return c;
}

int BTreeGtAtt (btree_t tree, Term constraint,
                Yap_UdiCallback callback, void * arg)
{
  double min;
  index_t n;
  int i, c = 0;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(tree,min,GT,&n,&i);
  if (d)
    {
      callback((void *) &d, d, arg);
      c ++;

      while ((d = BTreeSearchNext(DBL_MAX,LT,tree,&n,&i)))
        {
          callback((void *) &d, d, arg);
          c ++;
        }
    }
  return c;
}

int BTreeGeAtt (btree_t tree, Term constraint,
                Yap_UdiCallback callback, void * arg)
{
  double min;
  index_t n;
  int i, c = 0;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(tree,min,GE,&n,&i);
  if (d)
    {
      callback((void *) &d, d, arg);
      c ++;

      while ((d = BTreeSearchNext(DBL_MAX,LT,tree,&n,&i)))
        {
          callback((void *) &d, d, arg);
          c ++;
        }
    }
  return c;
}

int BTreeRangeAtt (btree_t tree, Term constraint,
                   Yap_UdiCallback callback, void * arg)
{
  double min,max;
  int minc,maxc;
  index_t n;
  int i, c = 0;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));
  minc = strcmp(YAP_AtomName(YAP_AtomOfTerm(YAP_ArgOfTerm(3,constraint))),
                "true") == 0 ? GE: GT;
  max = YAP_FloatOfTerm(YAP_ArgOfTerm(4,constraint));
  maxc = strcmp(YAP_AtomName(YAP_AtomOfTerm(YAP_ArgOfTerm(5,constraint))),
                "true") == 0 ? LE: LT;

  d = BTreeSearch(tree,min,minc,&n,&i);
  if (d)
    {
      callback((void *) &d, d, arg);
      c ++;

      while ((d = BTreeSearchNext(max,maxc,tree,&n,&i)))
        {
          callback((void *) &d, d, arg);
          c ++;
        }
    }
  return c;
}

void BtreeUdiDestroy(control_t control)
{
  assert(control);

  if (control->tree)
    BTreeDestroy(control->tree);
}
