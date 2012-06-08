#include <assert.h>
#include "Yap.h"
#include "clause.h"
#include "clause_list.h"

#include "udi.h"
#include "udi_indexers.h"
#include "gmpis.h"

/*TODO: remove this*/
/* we can have this stactic because it is written once */
static struct udi_control_block RtreeCmd;

/******
       All the info we need to enter user indexed code:
        predicate
	the user control block
	functions used, in case we have different schema (maybe should part of previous)
	right now, this is just a linked list....
******/
typedef struct udi_info
{
  PredEntry *p;
  void *cb;
  UdiControlBlock functions;
  struct udi_info *next;
} *UdiInfo;

/******
      we now have one extra user indexed predicate. We assume these
      are few, so we can do with a linked list.
******/
static int
add_udi_block(void *info, PredEntry *p, UdiControlBlock cmd)
{
  UdiInfo blk = (UdiInfo)Yap_AllocCodeSpace(sizeof(struct udi_info));
  if (!blk)
    return FALSE;
  blk->next = UdiControlBlocks;
  UdiControlBlocks = blk;
  blk->p = p;
  blk->functions = cmd;
  blk->cb = info;
  return TRUE;
}

/******
      new user indexed predicate;
      the type right now is just rtrees, but in the future we'll have more.
      the second argument is the term.
******/
static Int
p_new_udi( USES_REGS1 )
{
  Term spec = Deref(ARG2), udi_type = Deref(ARG1);
  PredEntry *p;
  UdiControlBlock cmd;
  Atom udi_t;
  void *info;

/*  fprintf(stderr,"new pred babe\n");*/
  /* get the predicate from the spec, copied from cdmgr.c */
  if (IsVarTerm(spec)) {
    Yap_Error(INSTANTIATION_ERROR,spec,"new user index/1");
    return FALSE;
  } else if (!IsApplTerm(spec)) {
    Yap_Error(TYPE_ERROR_COMPOUND,spec,"new user index/1");
    return FALSE;
  } else {
    Functor    fun = FunctorOfTerm(spec);
    Term tmod = CurrentModule;

    while (fun == FunctorModule) {
      tmod = ArgOfTerm(1,spec);
      if (IsVarTerm(tmod) ) {
	Yap_Error(INSTANTIATION_ERROR, spec, "new user index/1");
	return FALSE;
      }
      if (!IsAtomTerm(tmod) ) {
	Yap_Error(TYPE_ERROR_ATOM, spec, "new user index/1");
	return FALSE;
      }
      spec = ArgOfTerm(2, spec);
      fun = FunctorOfTerm(spec);
    }
    p = RepPredProp(PredPropByFunc(fun, tmod));
  }
  if (!p)
    return FALSE;
  /* boring, boring, boring! */
  if ((p->PredFlags & (DynamicPredFlag|LogUpdatePredFlag|UserCPredFlag|CArgsPredFlag|NumberDBPredFlag|AtomDBPredFlag|TestPredFlag|AsmPredFlag|CPredFlag|BinaryPredFlag)) ||
      (p->ModuleOfPred == PROLOG_MODULE)) {
    Yap_Error(PERMISSION_ERROR_MODIFY_STATIC_PROCEDURE, spec, "udi/2");
    return FALSE;
  }
  if (p->PredFlags & (DynamicPredFlag|LogUpdatePredFlag|TabledPredFlag)) {
    Yap_Error(PERMISSION_ERROR_ACCESS_PRIVATE_PROCEDURE, spec, "udi/2");
    return FALSE;
  }
  /* just make sure we're looking at the right user type! */
  if (IsVarTerm(udi_type)) {
    Yap_Error(INSTANTIATION_ERROR,spec,"new user index/1");
    return FALSE;
  } else if (!IsAtomTerm(udi_type)) {
    Yap_Error(TYPE_ERROR_ATOM,spec,"new user index/1");
    return FALSE;
  }
  udi_t = AtomOfTerm(udi_type);
  if (udi_t == AtomRTree) {
    cmd = &RtreeCmd;
  } else {
    Yap_Error(TYPE_ERROR_ATOM,spec,"new user index/1");
    return FALSE;
  }
  /* this is the real work */
  info = cmd->init(spec, (void *)p, p->ArityOfPE);
  if (!info)
    return FALSE;
  /* add to table */
  if (!add_udi_block(info, p, cmd)) {
    Yap_Error(OUT_OF_HEAP_ERROR, spec, "new user index/1");
    return FALSE;
  }
  p->PredFlags |= UDIPredFlag;
  return TRUE;
}

/* just pass info to user, called from cdmgr.c */
int
Yap_new_udi_clause(PredEntry *p, yamop *cl, Term t)
{
  struct udi_info *info = UdiControlBlocks;
  while (info->p != p && info)
    info = info->next;
  if (!info)
    return FALSE;
  info->cb = info->functions->insert(t, info->cb, (void *)cl);
  return TRUE;
}

/* index, called from absmi.c */
yamop *
Yap_udi_search(PredEntry *p)
{
  struct udi_info *info = UdiControlBlocks;
  while (info->p != p && info)
    info = info->next;
  if (!info)
    return NULL;
  return info->functions->search(info->cb,NULL,NULL);
}

/* control_t linked list maintenance */
static control_t add_control(int arg, void *pred, void *idxstr,
                             control_t old)
{
  control_t control;

  control = (control_t) Yap_AllocCodeSpace(sizeof(*control));
  if (!control)
    return NULL;

  control->arg = arg;
  control->pred = pred;
  control->tree = NULL;
  control->controlblock = idxstr;
  control->next = old;

  return control;
}

control_t UdiInit (Term spec, void * pred, int arity)
{
  int i, j, n;
  int valid_spec;
  Term arg;
  Atom idxtype;
  control_t control;
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
      arg = YAP_ArgOfTerm(i,spec);
      if (YAP_IsAtomTerm(arg))
        {
          idxtype = YAP_AtomOfTerm(arg);

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

control_t UdiInsert (Term term,control_t control,void *clausule)
{
  control_t iter;
  int i;
  int *index;
  void **clausules;

  assert(control);
  /* fprintf(stderr,"%p\n", (void *) control); */

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

  /* for (i = 0; i < *index; i++) */
  /*   fprintf(stderr,"%d:%p\n",i,clausules[i]); */
  /* if (*index > 2) */
  /*   { */
  /*     fprintf(stderr,"%d:%p\n",(*index) - 2,clausules[(*index) -2]); */
  /*     fprintf(stderr,"%d:%p\n",(*index) - 1,clausules[(*index) -1]); */
  /*     fprintf(stderr,"%p %d %d\n",clausule, (*index), */
  /*             ((control->clausules->size - sizeof(int)) / sizeof(void *))); */
  /*   } */

  iter = control;
  while (iter)
    {
      iter->controlblock->insert(term, iter, *index);
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

void *UdiSearch (control_t control, YAP_UdiCallback c, void *d)
{
  control_t iter;
  void * r;
  iset_t result;

  assert(control);

  iter = control;
  while (iter)
    {
      result = GMPISInit();
      fprintf(stderr, "%p %p Result\n", callback, (void *) result);
      r = control->controlblock->search(control, callback, (void *) result);
      if (r)
        {
          size_t last = 0;
          struct ClauseList clauselist;
          void **clausules;

          //result = (iset_t) r;
          clausules = (void **) (control->clausules->region + sizeof(int));

          Yap_ClauseListInit(&clauselist);
          last = GMPISNext(result,last);

          while (last != NOMORE)
            {
              Yap_ClauseListExtend(&clauselist,
                                   clausules[last],
                                   control->pred);

              fprintf(stderr, "%ld %p %p\n",
                      last,
                      clausules[last],
                      control->pred );

              last = GMPISNext(result,++last);
            }
          fprintf(stderr, "\n\n");
          GMPISDestroy(result);

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
          /* return r; */
        }
      iter = iter->next;
    }
  return NULL;
}

int UdiDestroy (control_t control)
{

  assert(control);

  control->controlblock->destroy(control);

  return TRUE;
}

void
Yap_udi_init(void)
{
  UdiControlBlocks = NULL;
  /* to be filled in by David */
  /* RtreeCmd.init = RtreeUdiInit; */
  /* RtreeCmd.insert = RtreeUdiInsert; */
  /* RtreeCmd.search = RtreeUdiSearch; */
  /* RtreeCmd.destroy = RtreeUdiDestroy; */
  /* RtreeCmd.init = BtreeUdiInit; */
  /* RtreeCmd.insert = BtreeUdiInsert; */
  /* RtreeCmd.search = BtreeUdiSearch; */
  /* RtreeCmd.destroy = BtreeUdiDestroy; */
  RtreeCmd.init = UdiInit;
  RtreeCmd.insert = UdiInsert;
  RtreeCmd.search = UdiSearch;
  RtreeCmd.destroy = UdiDestroy;
  Yap_InitCPred("$udi_init", 2, p_new_udi, 0);
}

