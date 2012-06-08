#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

#include <YapInterface.h>

#include "Yap.h"

#include "udi.h"

#include "b+tree.h"
#include "clause_list.h"
#include "b+tree_udi_i.h"
#include "b+tree_udi.h"

#define NARGS 1

control_t BtreeUdiInit (YAP_Term spec,
                         void * pred,
                         int arity){
  control_t control;
  YAP_Term arg;
  int i, c;
  /*  YAP_Term mod;  */

  /*  spec = Yap_StripModule(spec, &mod); */
  if (! YAP_IsApplTerm(spec))
    return (NULL);

  control = (control_t) malloc (sizeof(*control));
  assert(control);
  memset((void *) control,0, sizeof(*control));

  c = 0;
  for (i = 1; i <= arity; i ++)
    {
      arg = YAP_ArgOfTerm(i,spec);
      if (YAP_IsAtomTerm(arg)
          && strcmp("+",YAP_AtomName(YAP_AtomOfTerm(arg))) == 0)
        {

          control[c].pred = pred;
          control[c++].arg = i;
        }
    }

  /* for (i = 0; i < NARGS; i++) */
  /*   printf("%d,%p\t",(*control)[i].arg,(*control)[i].tree); */
  /* printf("\n"); */

  return control;
}

control_t BtreeUdiInsert (YAP_Term term,control_t control,index_t clausule)
{
  int i;

  assert(control);

  for (i = 0; i < NARGS && control[i].arg != 0 ; i++)
    {
      if (!control[i].tree)
        control[i].tree = BTreeNew();
      BTreeInsert(control[i].tree,
                  YAP_FloatOfTerm(YAP_ArgOfTerm(control[i].arg,term)),
                  clausule);
    }

  /*  printf("insert %p\n", clausule); */

  return control;
}

/*ARGS ARE AVAILABLE*/
void *BtreeUdiSearch (control_t control)
{
  int i, j;
  size_t n;
  struct ClauseList clauselist;
  clause_list_t cl;
  YAP_Term t, Constraints;
  const char * att;


  for (i = 0; i < NARGS && control[i].arg != 0 ; i++) {
    t = YAP_A(control[i].arg);
    if (YAP_IsAttVar(t))
      {

        /*get the constraits rect*/
        Constraints = YAP_AttsOfVar(t);
        /* Yap_DebugPlWrite(Constraints); */
        att = YAP_AtomName(YAP_NameOfFunctor(YAP_FunctorOfTerm(Constraints)));

        cl = Yap_ClauseListInit(&clauselist);
        if (!cl)
          return NULL; /*? or fail*/

        n = sizeof (att_func) / sizeof (struct Att);
        for (j = 0; j < n; j ++)
          if (strcmp(att_func[j].att,att) == 0)
            att_func[j].proc_att(control[i],Constraints,cl);

        Yap_ClauseListClose(cl);

        if (Yap_ClauseListCount(cl) == 0)
          {
            Yap_ClauseListDestroy(cl);
            return Yap_FAILCODE();
          }

        if (Yap_ClauseListCount(cl) == 1)
          {
            return Yap_ClauseListToClause(cl);
          }

        return Yap_ClauseListCode(cl);
      }
  }

  return NULL; /*YAP FALLBACK*/
}

/*Needs to test if tree is not null*/
void BTreeMinAtt (struct Control c,Term constraint, clause_list_t cl)
{
  index_t d;

  /* printf("MIN\n"); */

  d = BTreeMin(c.tree,NULL,NULL);

  if (d)
    Yap_ClauseListExtend(cl,d,c.pred);
}

void BTreeMaxAtt (struct Control c,Term constraint, clause_list_t cl)
{
  index_t d;

  /* printf("MAX\n"); */

  d = BTreeMax(c.tree,NULL,NULL);
  if (d)
    Yap_ClauseListExtend(cl,d,c.pred);
}

void BTreeEqAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double min;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(c.tree,min,EQ,NULL,NULL);
  if (d)
    Yap_ClauseListExtend(cl,d,c.pred);
}

void BTreeLtAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double max;
  index_t n;
  int i;
  index_t d;

  max = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeMin(c.tree,&n,&i);

  if (d)
    {
      Yap_ClauseListExtend(cl,d,c.pred);

      while ((d = BTreeSearchNext(max,LT,c.tree,&n,&i)))
        {
          Yap_ClauseListExtend(cl,d,c.pred);
        }
    }
}

void BTreeLeAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double max;
  index_t n;
  int i;
  index_t d;

  max = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeMin(c.tree,&n,&i);

  if (d)
    {
      Yap_ClauseListExtend(cl,d,c.pred);

      while ((d = BTreeSearchNext(max,LE,c.tree,&n,&i)))
        Yap_ClauseListExtend(cl,d,c.pred);
    }
}

void BTreeGtAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double min;
  index_t n;
  int i;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(c.tree,min,GT,&n,&i);
  if (d)
    {
      Yap_ClauseListExtend(cl,d,c.pred);

      while ((d = BTreeSearchNext(DBL_MAX,LT,c.tree,&n,&i)))
        Yap_ClauseListExtend(cl,d,c.pred);
    }
}

void BTreeGeAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double min;
  index_t n;
  int i;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));

  d = BTreeSearch(c.tree,min,GE,&n,&i);
  if (d)
    {
      Yap_ClauseListExtend(cl,d,c.pred);

      while ((d = BTreeSearchNext(DBL_MAX,LT,c.tree,&n,&i)))
        Yap_ClauseListExtend(cl,d,c.pred);
    }
}

void BTreeRangeAtt (struct Control c,Term constraint, clause_list_t cl)
{
  double min,max;
  int minc,maxc;
  index_t n;
  int i;
  index_t d;

  min = YAP_FloatOfTerm(YAP_ArgOfTerm(2,constraint));
  minc = strcmp(YAP_AtomName(YAP_AtomOfTerm(YAP_ArgOfTerm(3,constraint))),
                "true") == 0 ? GE: GT;
  max = YAP_FloatOfTerm(YAP_ArgOfTerm(4,constraint));
  maxc = strcmp(YAP_AtomName(YAP_AtomOfTerm(YAP_ArgOfTerm(5,constraint))),
                "true") == 0 ? LE: LT;

  d = BTreeSearch(c.tree,min,minc,&n,&i);
  if (d)
    {
      Yap_ClauseListExtend(cl,d,c.pred);
      while ((d = BTreeSearchNext(max,maxc,c.tree,&n,&i)))
        Yap_ClauseListExtend(cl,d,c.pred);
    }
}

int BtreeUdiDestroy(control_t control)
{
  int i;

  assert(control);

  for (i = 0; i < NARGS && control[i].arg != 0; i++)
    {
      if (control[i].tree)
        BTreeDestroy(control[i].tree);
    }

  free(control);
  control = NULL;

  return TRUE;
}
