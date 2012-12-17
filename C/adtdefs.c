/*************************************************************************
*									 *
*	 YAP Prolog 							 *
*									 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-1997	 *
*									 *
**************************************************************************
*									 *
* File:		adtdefs.c						 *
* Last rev:								 *
* mods:									 *
* comments:	abstract machine definitions				 *
*									 *
*************************************************************************/
#ifdef SCCS
static char SccsId[] = "%W% %G%";

#endif

#define ADTDEFS_C

#ifdef __SUNPRO_CC
#define inline
#endif

#include "Yap.h"
ADDR    STD_PROTO(Yap_PreAllocCodeSpace, (void));
Prop	STD_PROTO(PredPropByFunc,(Functor, Term));
Prop	STD_PROTO(PredPropByAtom,(Atom, Term));
#include "Yatom.h"
#include "yapio.h"
#include <stdio.h>
#include <wchar.h>
#if HAVE_STRING_H
#include <string.h>
#endif

/* this routine must be run at least having a read lock on ae */
static Prop
GetFunctorProp(AtomEntry *ae, unsigned int arity)
{				/* look property list of atom a for kind  */
  FunctorEntry *pp;

  pp = RepFunctorProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp) &&
	 (!IsFunctorProperty(pp->KindOfPE) ||
	  pp->ArityOfFE != arity))
    pp = RepFunctorProp(pp->NextOfPE);
  return (AbsFunctorProp(pp));
}

/* vsc: We must guarantee that IsVarTerm(functor) returns true! */
static inline Functor
InlinedUnlockedMkFunctor(AtomEntry *ae, unsigned int arity)
{
  FunctorEntry *p;
  Prop p0;

  p0 = GetFunctorProp(ae, arity);
  if (p0 != NIL) {
    return ((Functor) RepProp(p0));
  }
  p = (FunctorEntry *) Yap_AllocAtomSpace(sizeof(*p));
  if (!p)
    return NULL;
  p->KindOfPE = FunctorProperty;
  p->NameOfFE = AbsAtom(ae);
  p->ArityOfFE = arity;
  p->PropsOfFE = NIL;
  INIT_RWLOCK(p->FRWLock);
  /* respect the first property, in case this is a wide atom */
  AddPropToAtom(ae, (PropEntry *)p);
  return ((Functor) p);
}

Functor
Yap_UnlockedMkFunctor(AtomEntry *ae, unsigned int arity)
{
  return(InlinedUnlockedMkFunctor(ae, arity));
}

/* vsc: We must guarantee that IsVarTerm(functor) returns true! */
Functor
Yap_MkFunctor(Atom ap, unsigned int arity)
{
  AtomEntry *ae = RepAtom(ap);
  Functor f;

  WRITE_LOCK(ae->ARWLock);
  f = InlinedUnlockedMkFunctor(ae, arity);
  WRITE_UNLOCK(ae->ARWLock);
  return (f);
}

/* vsc: We must guarantee that IsVarTerm(functor) returns true! */
void
Yap_MkFunctorWithAddress(Atom ap, unsigned int arity, FunctorEntry *p)
{
  AtomEntry *ae = RepAtom(ap);
  
  WRITE_LOCK(ae->ARWLock);
  p->KindOfPE = FunctorProperty;
  p->NameOfFE = ap;
  p->ArityOfFE = arity;
  AddPropToAtom(ae, (PropEntry *)p);
  WRITE_UNLOCK(ae->ARWLock);
}

inline static Atom
SearchInInvisible(char *atom)
{
  AtomEntry *chain;

  READ_LOCK(INVISIBLECHAIN.AERWLock);
  chain = RepAtom(INVISIBLECHAIN.Entry);
  while (!EndOfPAEntr(chain) && strcmp(chain->StrOfAE, atom)) {
    chain = RepAtom(chain->NextOfAE);
  }
  READ_UNLOCK(INVISIBLECHAIN.AERWLock);
  if (EndOfPAEntr(chain))
    return (NIL);
  else
    return(AbsAtom(chain));
}

static inline Atom
SearchAtom(unsigned char *p, Atom a) {
  AtomEntry *ae;

  /* search atom in chain */
  while (a != NIL) {
    ae = RepAtom(a);
    if (strcmp(ae->StrOfAE, (const char *)p) == 0) {
      return(a);
    }
    a = ae->NextOfAE;
  }
  return(NIL);
}

static inline Atom
SearchWideAtom(wchar_t *p, Atom a) {
  AtomEntry *ae;

  /* search atom in chain */
  while (a != NIL) {
    ae = RepAtom(a);
    if (wcscmp((wchar_t *)ae->StrOfAE, p) == 0) {
      return a;
    }
    a = ae->NextOfAE;
  }
  return(NIL);
}

static Atom
LookupAtom(char *atom)
{				/* lookup atom in atom table            */
  register CELL hash;
  register unsigned char *p;
  Atom a, na;
  AtomEntry *ae;

  /* compute hash */
  p = (unsigned char *)atom;
  hash = HashFunction(p) % AtomHashTableSize;
  /* we'll start by holding a read lock in order to avoid contention */
  READ_LOCK(HashChain[hash].AERWLock);
  a = HashChain[hash].Entry;
  /* search atom in chain */
  na = SearchAtom((unsigned char *)atom, a);
  if (na != NIL) {
    READ_UNLOCK(HashChain[hash].AERWLock);
    return(na);
  }
  READ_UNLOCK(HashChain[hash].AERWLock);
  /* we need a write lock */
  WRITE_LOCK(HashChain[hash].AERWLock);
  /* concurrent version of Yap, need to take care */
#if defined(YAPOR) || defined(THREADS)
  if (a != HashChain[hash].Entry) {
    a = HashChain[hash].Entry;
    na = SearchAtom((unsigned char *)atom, a);
    if (na != NIL) {
      WRITE_UNLOCK(HashChain[hash].AERWLock);
      return(na);
    }
  }
#endif  
  /* add new atom to start of chain */
  ae = (AtomEntry *) Yap_AllocAtomSpace((sizeof *ae) + strlen(atom) + 1);
  if (ae == NULL) {
    WRITE_UNLOCK(HashChain[hash].AERWLock);
    return NIL;
  }
  NOfAtoms++;
  na = AbsAtom(ae);
  ae->PropsOfAE = NIL;
  if (ae->StrOfAE != atom)
    strcpy(ae->StrOfAE, atom);
  ae->NextOfAE = a;
  HashChain[hash].Entry = na;
  INIT_RWLOCK(ae->ARWLock);
  WRITE_UNLOCK(HashChain[hash].AERWLock);
  if (NOfAtoms > 2*AtomHashTableSize) {
    Yap_signal(YAP_CDOVF_SIGNAL);
  }
  return na;
}


static Atom
LookupWideAtom(wchar_t *atom)
{				/* lookup atom in atom table            */
  CELL hash;
  wchar_t *p;
  Atom a, na;
  AtomEntry *ae;
  UInt sz;
  WideAtomEntry *wae;

  /* compute hash */
  p = atom;
  hash = WideHashFunction(p) % WideAtomHashTableSize;
  /* we'll start by holding a read lock in order to avoid contention */
  READ_LOCK(WideHashChain[hash].AERWLock);
  a = WideHashChain[hash].Entry;
  /* search atom in chain */
  na = SearchWideAtom(atom, a);
  if (na != NIL) {
    READ_UNLOCK(WideHashChain[hash].AERWLock);
    return(na);
  }
  READ_UNLOCK(WideHashChain[hash].AERWLock);
  /* we need a write lock */
  WRITE_LOCK(WideHashChain[hash].AERWLock);
  /* concurrent version of Yap, need to take care */
#if defined(YAPOR) || defined(THREADS)
  if (a != WideHashChain[hash].Entry) {
    a = WideHashChain[hash].Entry;
    na = SearchWideAtom(atom, a);
    if (na != NIL) {
      WRITE_UNLOCK(WideHashChain[hash].AERWLock);
      return na;
    }
  }
#endif  
  /* add new atom to start of chain */
  sz = wcslen(atom);
  ae = (AtomEntry *) Yap_AllocAtomSpace((size_t)(((AtomEntry *)NULL)+1) + sizeof(wchar_t)*(sz + 1));
  if (ae == NULL) {
    WRITE_UNLOCK(WideHashChain[hash].AERWLock);
    return NIL;
  }
  wae = (WideAtomEntry *) Yap_AllocAtomSpace(sizeof(WideAtomEntry));
  if (wae == NULL) {
    WRITE_UNLOCK(WideHashChain[hash].AERWLock);
    return NIL;
  }
  na = AbsAtom(ae);
  ae->PropsOfAE = AbsWideAtomProp(wae);
  wae->NextOfPE = NIL;
  wae->KindOfPE = WideAtomProperty;
  wae->SizeOfAtom = sz;
  if (ae->StrOfAE != (char *)atom)
    wcscpy((wchar_t *)(ae->StrOfAE), atom);
  NOfAtoms++;
  ae->NextOfAE = a;
  WideHashChain[hash].Entry = na;
  INIT_RWLOCK(ae->ARWLock);
  WRITE_UNLOCK(WideHashChain[hash].AERWLock);
  if (NOfWideAtoms > 2*WideAtomHashTableSize) {
    Yap_signal(YAP_CDOVF_SIGNAL);
  }
  return na;
}

Atom
Yap_LookupMaybeWideAtom(wchar_t *atom)
{				/* lookup atom in atom table            */
  wchar_t *p = atom, c;
  size_t len = 0;
  char *ptr, *ptr0;
  Atom at;

  while ((c = *p++)) { 
    if (c > 255) return LookupWideAtom(atom);
    len++;
  }
  /* not really a wide atom */
  p = atom;
  ptr0 = ptr = Yap_AllocCodeSpace(len+1);
  if (!ptr)
    return NIL;
  while ((*ptr++ = *p++));
  at = LookupAtom(ptr0);
  Yap_FreeCodeSpace(ptr0);
  return at;
}

Atom
Yap_LookupMaybeWideAtomWithLength(wchar_t *atom, size_t len)
{				/* lookup atom in atom table            */
  wchar_t *p = atom, c;
  size_t len0 = 0;
  Atom at;
  int wide = FALSE;

  while ((c = *p++)) { 
    if (c > 255) wide = TRUE;
    len0++;
    if (len0 == len) break;
  }
  if (p[0] == '\0' && wide) return LookupWideAtom(atom);
  else if (wide) {
    wchar_t *ptr, *ptr0;
    p = atom;
    ptr0 = ptr = (wchar_t *)Yap_AllocCodeSpace(sizeof(wchar_t)*(len+1));
    if (!ptr)
      return NIL;
    while (len--) {*ptr++ = *p++;}
    ptr[0] = '\0';
    at = LookupWideAtom(ptr0);
    Yap_FreeCodeSpace((char *)ptr0);
    return at;
  } else {
    char *ptr, *ptr0;
    /* not really a wide atom */
    p = atom;
    ptr0 = ptr = Yap_AllocCodeSpace(len+1);
    if (!ptr)
      return NIL;
    while (len--) {*ptr++ = *p++;}
    ptr[0] = '\0';
    at = LookupAtom(ptr0);
    Yap_FreeCodeSpace(ptr0);
    return at;
  }
}

Atom
Yap_LookupAtom(char *atom)
{				/* lookup atom in atom table            */
  return LookupAtom(atom);
}

Atom
Yap_LookupWideAtom(wchar_t *atom)
{				/* lookup atom in atom table            */
  return LookupWideAtom(atom);
}

Atom
Yap_FullLookupAtom(char *atom)
{				/* lookup atom in atom table            */
  Atom t;

  if ((t = SearchInInvisible(atom)) != NIL) {
    return (t);
  }
  return(LookupAtom(atom));
}

void
Yap_LookupAtomWithAddress(char *atom, AtomEntry *ae)
{				/* lookup atom in atom table            */
  register CELL hash;
  register unsigned char *p;
  Atom a;

  /* compute hash */
  p = (unsigned char *)atom;
  hash = HashFunction(p) % AtomHashTableSize;
  /* ask for a WRITE lock because it is highly unlikely we shall find anything */
  WRITE_LOCK(HashChain[hash].AERWLock);
  a = HashChain[hash].Entry;
  /* search atom in chain */
  if (SearchAtom(p, a) != NIL) {
    Yap_Error(INTERNAL_ERROR,TermNil,"repeated initialisation for atom %s", ae);
    WRITE_UNLOCK(HashChain[hash].AERWLock);
    return;
  }
  /* add new atom to start of chain */
  NOfAtoms++;
  ae->NextOfAE = a;
  HashChain[hash].Entry = AbsAtom(ae);
  ae->PropsOfAE = NIL;
  strcpy(ae->StrOfAE, atom);
  INIT_RWLOCK(ae->ARWLock);
  WRITE_UNLOCK(HashChain[hash].AERWLock);
}

void
Yap_ReleaseAtom(Atom atom)
{				/* Releases an atom from the hash chain */
  register Int hash;
  register unsigned char *p;
  AtomEntry *inChain;
  AtomEntry *ap = RepAtom(atom);
  char *name = ap->StrOfAE;

  /* compute hash */
  p = (unsigned char *)name;
  hash = HashFunction(p) % AtomHashTableSize;
  WRITE_LOCK(HashChain[hash].AERWLock);
  if (HashChain[hash].Entry == atom) {
    NOfAtoms--;
    HashChain[hash].Entry = ap->NextOfAE;
    WRITE_UNLOCK(HashChain[hash].AERWLock);
    return;
  }
  /* else */
  inChain = RepAtom(HashChain[hash].Entry);
  while (inChain->NextOfAE != atom)
    inChain = RepAtom(inChain->NextOfAE);
  WRITE_LOCK(inChain->ARWLock);
  inChain->NextOfAE = ap->NextOfAE;
  WRITE_UNLOCK(inChain->ARWLock);
  WRITE_UNLOCK(HashChain[hash].AERWLock);
}

static Prop
GetAPropHavingLock(AtomEntry *ae, PropFlags kind)
{				/* look property list of atom a for kind  */
  PropEntry *pp;

  pp = RepProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp) && pp->KindOfPE != kind)
    pp = RepProp(pp->NextOfPE);
  return (AbsProp(pp));
}

Prop
Yap_GetAPropHavingLock(AtomEntry *ae, PropFlags kind)
{				/* look property list of atom a for kind  */
  return GetAPropHavingLock(ae,kind);
}

static Prop
GetAProp(Atom a, PropFlags kind)
{				/* look property list of atom a for kind  */
  AtomEntry *ae = RepAtom(a);
  Prop out;

  READ_LOCK(ae->ARWLock);
  out = GetAPropHavingLock(ae, kind);
  READ_UNLOCK(ae->ARWLock);
  return (out);
}

Prop
Yap_GetAProp(Atom a, PropFlags kind)
{				/* look property list of atom a for kind  */
  return GetAProp(a,kind);
}

OpEntry *
Yap_GetOpPropForAModuleHavingALock(Atom a, Term mod)
{				/* look property list of atom a for kind  */
  AtomEntry *ae = RepAtom(a);
  PropEntry *pp;

  pp = RepProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp) &&
	 (pp->KindOfPE != OpProperty ||
	  ((OpEntry *)pp)->OpModule != mod))
    pp = RepProp(pp->NextOfPE);
  if (EndOfPAEntr(pp)) {
    return NULL;
  }
  return (OpEntry *)pp;
}

int
Yap_HasOp(Atom a)
{				/* look property list of atom a for kind  */
  AtomEntry *ae = RepAtom(a);
  PropEntry *pp;

  READ_LOCK(ae->ARWLock);
  pp = RepProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp) &&
	 ( pp->KindOfPE != OpProperty))
    pp = RepProp(pp->NextOfPE);
  READ_UNLOCK(ae->ARWLock);
  if (EndOfPAEntr(pp)) {
    return FALSE;
  } else {
    return TRUE;
  }
}

OpEntry *
Yap_OpPropForModule(Atom a, Term mod)
{				/* look property list of atom a for kind  */
  AtomEntry *ae = RepAtom(a);
  PropEntry *pp;
  OpEntry *info = NULL;

  if (mod == TermProlog)
    mod = PROLOG_MODULE;
  WRITE_LOCK(ae->ARWLock);
  pp = RepProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp)) {
    if ( pp->KindOfPE == OpProperty) {
      info = (OpEntry *)pp;
      if (info->OpModule == mod) {
	WRITE_LOCK(info->OpRWLock);
	WRITE_UNLOCK(ae->ARWLock);
	return info;
      }
    }
    pp = pp->NextOfPE;
  }
  info = (OpEntry *) Yap_AllocAtomSpace(sizeof(OpEntry));
  info->KindOfPE = Ord(OpProperty);
  info->OpModule = mod;
  info->OpName = a;
  LOCK(OpListLock);
  info->OpNext = OpList;
  OpList = info;
  UNLOCK(OpListLock);
  AddPropToAtom(ae, (PropEntry *)info);
  INIT_RWLOCK(info->OpRWLock);
  WRITE_LOCK(info->OpRWLock);
  WRITE_UNLOCK(ae->ARWLock);
  info->Prefix = info->Infix = info->Posfix = 0;
  return info;
}

OpEntry *
Yap_GetOpProp(Atom a, op_type type USES_REGS)
{				/* look property list of atom a for kind  */
  AtomEntry *ae = RepAtom(a);
  PropEntry *pp;
  OpEntry *oinfo = NULL;

  READ_LOCK(ae->ARWLock);
  pp = RepProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp)) {
    OpEntry *info = NULL;
    if ( pp->KindOfPE != OpProperty) {
      pp = RepProp(pp->NextOfPE);
      continue;
    }
    info = (OpEntry *)pp;
    if (info->OpModule != CurrentModule &&
	info->OpModule != PROLOG_MODULE) {
      pp = RepProp(pp->NextOfPE);
      continue;
    }
    if (type == INFIX_OP) {
      if (!info->Infix) {
	pp = RepProp(pp->NextOfPE);
	continue;
      }
    } else if (type == POSFIX_OP) {
      if (!info->Posfix) {
	pp = RepProp(pp->NextOfPE);
	continue;
      }
    } else {
      if (!info->Prefix) {
	pp = RepProp(pp->NextOfPE);
	continue;
      }
    }
    /* if it is not the latest module */
    if (info->OpModule == PROLOG_MODULE) {
      /* cannot commit now */
      oinfo = info;
      pp = RepProp(pp->NextOfPE);
    } else {
      READ_LOCK(info->OpRWLock);
      READ_UNLOCK(ae->ARWLock);
      return info;
    }
  }
  if (oinfo) {
    READ_LOCK(oinfo->OpRWLock);
    READ_UNLOCK(ae->ARWLock);
    return oinfo;
  }
  READ_UNLOCK(ae->ARWLock);
  return NULL;
}


inline static Prop
GetPredPropByAtomHavingLock(AtomEntry* ae, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;

  p0 = ae->PropsOfAE;
  while (p0) {
    PredEntry *pe = RepPredProp(p0);
    if ( pe->KindOfPE == PEProp && 
	 (pe->ModuleOfPred == cur_mod || !pe->ModuleOfPred)) {
      return(p0);
#if THREADS
      /* Thread Local Predicates */
      if (pe->PredFlags & ThreadLocalPredFlag) {
	return AbsPredProp(Yap_GetThreadPred(pe INIT_REGS));
      }
#endif
    }
    p0 = pe->NextOfPE;
  }
  return(NIL);
}

Prop
Yap_GetPredPropByAtom(Atom at, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);

  READ_LOCK(ae->ARWLock);
  p0 = GetPredPropByAtomHavingLock(ae, cur_mod);
  READ_UNLOCK(ae->ARWLock);
  return(p0);
}


inline static Prop
GetPredPropByAtomHavingLockInThisModule(AtomEntry* ae, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;

  p0 = ae->PropsOfAE;
  while (p0) {
    PredEntry *pe = RepPredProp(p0);
    if ( pe->KindOfPE == PEProp && pe->ModuleOfPred == cur_mod ) {
#if THREADS
      /* Thread Local Predicates */
      if (pe->PredFlags & ThreadLocalPredFlag) {
	return AbsPredProp(Yap_GetThreadPred(pe INIT_REGS));
      }
#endif
      return(p0);
    }
    p0 = pe->NextOfPE;
  }
  return(NIL);
}

Prop
Yap_GetPredPropByAtomInThisModule(Atom at, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);

  READ_LOCK(ae->ARWLock);
  p0 = GetPredPropByAtomHavingLockInThisModule(ae, cur_mod);
  READ_UNLOCK(ae->ARWLock);
  return(p0);
}


Prop
Yap_GetPredPropByFunc(Functor f, Term cur_mod)
     /* get predicate entry for ap/arity;               */
{
  Prop p0;

  READ_LOCK(f->FRWLock);

  p0 = GetPredPropByFuncHavingLock(f, cur_mod);
  READ_UNLOCK(f->FRWLock);
  return (p0);
}

Prop
Yap_GetPredPropByFuncInThisModule(Functor f, Term cur_mod)
     /* get predicate entry for ap/arity;               */
{
  Prop p0;

  READ_LOCK(f->FRWLock);
  p0 = GetPredPropByFuncHavingLock(f, cur_mod);
  READ_UNLOCK(f->FRWLock);
  return (p0);
}

Prop
Yap_GetPredPropHavingLock(Atom ap, unsigned int arity, Term mod)
     /* get predicate entry for ap/arity;               */
{
  Prop p0;
  AtomEntry *ae = RepAtom(ap);
  Functor f;

  if (arity == 0) {
    GetPredPropByAtomHavingLock(ae, mod);
  }
  f = InlinedUnlockedMkFunctor(ae, arity);
  READ_LOCK(f->FRWLock);
  p0 = GetPredPropByFuncHavingLock(f, mod);
  READ_UNLOCK(f->FRWLock);
  return (p0);
}

/* get expression entry for at/arity;               */
Prop
Yap_GetExpProp(Atom at, unsigned int arity)
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);
  ExpEntry *p;

  READ_LOCK(ae->ARWLock);
  p = RepExpProp(p0 = ae->PropsOfAE);
  while (p0 && (p->KindOfPE != ExpProperty || p->ArityOfEE != arity))
    p = RepExpProp(p0 = p->NextOfPE);
  READ_UNLOCK(ae->ARWLock);
  return (p0);
}

/* get expression entry for at/arity, at is already locked;         */
Prop
Yap_GetExpPropHavingLock(AtomEntry *ae, unsigned int arity)
{
  Prop p0;
  ExpEntry *p;

  p = RepExpProp(p0 = ae->PropsOfAE);
  while (p0 && (p->KindOfPE != ExpProperty || p->ArityOfEE != arity))
    p = RepExpProp(p0 = p->NextOfPE);

  return (p0);
}

static int
ExpandPredHash(void)
{
  UInt new_size = PredHashTableSize+PredHashIncrement;
  PredEntry **oldp = PredHash;
  PredEntry **np = (PredEntry **) Yap_AllocAtomSpace(sizeof(PredEntry **)*new_size);
  UInt i;

  if (!np) {
    return FALSE;
  }
  for (i = 0; i < new_size; i++) {
    np[i] = NULL;
  }
  for (i = 0; i < PredHashTableSize; i++) {
    PredEntry *p = PredHash[i];

    while (p) {
      Prop nextp = p->NextOfPE;
      UInt hsh = PRED_HASH(p->FunctorOfPred, p->ModuleOfPred, new_size);
      p->NextOfPE = AbsPredProp(np[hsh]);
      np[hsh] = p;
      p = RepPredProp(nextp);
    }
  }
  PredHashTableSize = new_size;
  PredHash = np;
  Yap_FreeAtomSpace((ADDR)oldp);
  return TRUE;
}

/* fe is supposed to be locked */
Prop
Yap_NewPredPropByFunctor(FunctorEntry *fe, Term cur_mod)
{
  CACHE_REGS
  PredEntry *p = (PredEntry *) Yap_AllocAtomSpace(sizeof(*p));

  if (p == NULL) {
    WRITE_UNLOCK(fe->FRWLock);
    return NULL;
  }
  if (cur_mod == TermProlog)
    p->ModuleOfPred = 0L;
  else
    p->ModuleOfPred = cur_mod;
  if (fe->PropsOfFE) {
    UInt hsh = PRED_HASH(fe, cur_mod, PredHashTableSize);

    WRITE_LOCK(PredHashRWLock);
    if (10*(PredsInHashTable+1) > 6*PredHashTableSize) {
      if (!ExpandPredHash()) {
	Yap_FreeCodeSpace((ADDR)p);
	WRITE_UNLOCK(PredHashRWLock);
	WRITE_UNLOCK(fe->FRWLock);
	return NULL;
      }
      /* retry hashing */
      hsh = PRED_HASH(fe, cur_mod, PredHashTableSize);
    }
    PredsInHashTable++;
    if (p->ModuleOfPred == 0L) {
      PredEntry *pe = RepPredProp(fe->PropsOfFE);

      hsh = PRED_HASH(fe, pe->ModuleOfPred, PredHashTableSize);
      /* should be the first one */
      pe->NextOfPE = AbsPredProp(PredHash[hsh]);
      PredHash[hsh] = pe;
      fe->PropsOfFE = AbsPredProp(p);
    } else {
      p->NextOfPE = AbsPredProp(PredHash[hsh]);
      PredHash[hsh] = p;
    }
    WRITE_UNLOCK(PredHashRWLock);
    /* make sure that we have something here: note that this is not a valid pointer!! */
    RepPredProp(fe->PropsOfFE)->NextOfPE = fe->PropsOfFE;
  } else {
    fe->PropsOfFE = AbsPredProp(p);
    p->NextOfPE = NIL;
  }
  INIT_LOCK(p->PELock);
  p->KindOfPE = PEProp;
  p->ArityOfPE = fe->ArityOfFE;
  p->cs.p_code.FirstClause = p->cs.p_code.LastClause = NULL;
  p->cs.p_code.NOfClauses = 0;
  p->PredFlags = 0L;
  p->src.OwnerFile = AtomNil;
  p->OpcodeOfPred = UNDEF_OPCODE;
  p->CodeOfPred = p->cs.p_code.TrueCodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  p->cs.p_code.ExpandCode = EXPAND_OP_CODE; 
  p->TimeStampOfPred = 0L; 
  p->LastCallOfPred = LUCALL_ASSERT; 
  if (cur_mod == TermProlog)
    p->ModuleOfPred = 0L;
  else
    p->ModuleOfPred = cur_mod;
  Yap_NewModulePred(cur_mod, p);
  INIT_LOCK(p->StatisticsForPred.lock);
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
#ifdef TABLING
  p->TableOfPred = NULL;
#endif /* TABLING */
#ifdef BEAM
  p->beamTable = NULL;
#endif  /* BEAM */
  /* careful that they don't cross MkFunctor */
  if (PRED_GOAL_EXPANSION_FUNC) {
    if (fe->PropsOfFE &&
	(RepPredProp(fe->PropsOfFE)->PredFlags & GoalExPredFlag)) {
      p->PredFlags |= GoalExPredFlag;
    }
  }
  p->FunctorOfPred = fe;
  WRITE_UNLOCK(fe->FRWLock);
  Yap_inform_profiler_of_clause(&(p->OpcodeOfPred), &(p->OpcodeOfPred)+1, p, GPROF_NEW_PRED_FUNC);
  if (!(p->PredFlags & (CPredFlag|AsmPredFlag))) {
    Yap_inform_profiler_of_clause(&(p->cs.p_code.ExpandCode), &(p->cs.p_code.ExpandCode)+1, p, GPROF_NEW_PRED_FUNC);
  }
  return AbsPredProp(p);
}

#if THREADS
Prop
Yap_NewThreadPred(PredEntry *ap USES_REGS)
{
  PredEntry *p = (PredEntry *) Yap_AllocAtomSpace(sizeof(*p));

  if (p == NULL) {
    return NIL;
  }
  INIT_LOCK(p->PELock);
  p->KindOfPE = PEProp;
  p->ArityOfPE = ap->ArityOfPE;
  p->cs.p_code.FirstClause = p->cs.p_code.LastClause = NULL;
  p->cs.p_code.NOfClauses = 0;
  p->PredFlags = ap->PredFlags & ~(IndexedPredFlag|SpiedPredFlag);
  p->src.OwnerFile = ap->src.OwnerFile;
  p->OpcodeOfPred = UNDEF_OPCODE;
  p->CodeOfPred = p->cs.p_code.TrueCodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  p->cs.p_code.ExpandCode = EXPAND_OP_CODE; 
  p->ModuleOfPred = ap->ModuleOfPred;
  p->NextPredOfModule = NULL;
  p->TimeStampOfPred = 0L; 
  p->LastCallOfPred = LUCALL_ASSERT; 
  INIT_LOCK(p->StatisticsForPred.lock);
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
#ifdef TABLING
  p->TableOfPred = NULL;
#endif /* TABLING */
#ifdef BEAM
  p->beamTable = NULL;
#endif 
  /* careful that they don't cross MkFunctor */
  p->NextOfPE = AbsPredProp(LOCAL_ThreadHandle.local_preds);
  LOCAL_ThreadHandle.local_preds = p;
  p->FunctorOfPred = ap->FunctorOfPred;
  Yap_inform_profiler_of_clause(&(p->OpcodeOfPred), &(p->OpcodeOfPred)+1, p, GPROF_NEW_PRED_THREAD);
  if (!(p->PredFlags & (CPredFlag|AsmPredFlag))) {
    Yap_inform_profiler_of_clause(&(p->cs.p_code.ExpandCode), &(p->cs.p_code.ExpandCode)+1, p, GPROF_NEW_PRED_THREAD);
  }
  return AbsPredProp(p);
}
#endif

Prop
Yap_NewPredPropByAtom(AtomEntry *ae, Term cur_mod)
{
  CACHE_REGS
  Prop p0;
  PredEntry *p = (PredEntry *) Yap_AllocAtomSpace(sizeof(*p));

/* Printf("entering %s:%s/0\n", RepAtom(AtomOfTerm(cur_mod))->StrOfAE, ae->StrOfAE); */

  if (p == NULL) {
    WRITE_UNLOCK(ae->ARWLock);
    return NIL;
  }
  INIT_LOCK(p->PELock);
  p->KindOfPE = PEProp;
  p->ArityOfPE = 0;
  p->cs.p_code.FirstClause = p->cs.p_code.LastClause = NULL;
  p->cs.p_code.NOfClauses = 0;
  p->PredFlags = 0L;
  p->src.OwnerFile = AtomNil;
  p->OpcodeOfPred = UNDEF_OPCODE;
  p->cs.p_code.ExpandCode = EXPAND_OP_CODE; 
  p->CodeOfPred = p->cs.p_code.TrueCodeOfPred = (yamop *)(&(p->OpcodeOfPred)); 
  if (cur_mod == TermProlog)
    p->ModuleOfPred = 0;
  else
    p->ModuleOfPred = cur_mod;
  Yap_NewModulePred(cur_mod, p);
  INIT_LOCK(p->StatisticsForPred.lock);
  p->StatisticsForPred.NOfEntries = 0;
  p->StatisticsForPred.NOfHeadSuccesses = 0;
  p->StatisticsForPred.NOfRetries = 0;
  p->TimeStampOfPred = 0L; 
  p->LastCallOfPred = LUCALL_ASSERT; 
#ifdef TABLING
  p->TableOfPred = NULL;
#endif /* TABLING */
#ifdef BEAM
  p->beamTable = NULL;
#endif 
  /* careful that they don't cross MkFunctor */
  if (PRED_GOAL_EXPANSION_FUNC) {
    Prop p1 = ae->PropsOfAE;

    while (p1) {
      PredEntry *pe = RepPredProp(p1);

      if (pe->KindOfPE == PEProp) {
	if (pe->PredFlags & GoalExPredFlag) {
	  p->PredFlags |= GoalExPredFlag;
	}
	break;
      }
      p1 = pe->NextOfPE;
    }
  }
  AddPropToAtom(ae, (PropEntry *)p);
  p0 = AbsPredProp(p);
  p->FunctorOfPred = (Functor)AbsAtom(ae);
  WRITE_UNLOCK(ae->ARWLock);
  Yap_inform_profiler_of_clause(&(p->OpcodeOfPred), &(p->OpcodeOfPred)+1, p, GPROF_NEW_PRED_ATOM);
  if (!(p->PredFlags & (CPredFlag|AsmPredFlag))) {
    Yap_inform_profiler_of_clause(&(p->cs.p_code.ExpandCode), &(p->cs.p_code.ExpandCode)+1, p, GPROF_NEW_PRED_ATOM);
  }
  return p0;
}

Prop
Yap_PredPropByFunctorNonThreadLocal(Functor f, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  PredEntry *p;

  WRITE_LOCK(f->FRWLock);
  if (!(p = RepPredProp(f->PropsOfFE))) 
    return Yap_NewPredPropByFunctor(f,cur_mod);

  if ((p->ModuleOfPred == cur_mod || !(p->ModuleOfPred))) {
    /* don't match multi-files */
    if (!(p->PredFlags & MultiFileFlag) ||
	p->ModuleOfPred ||
	!cur_mod ||
	cur_mod == TermProlog) {
      WRITE_UNLOCK(f->FRWLock);
      return AbsPredProp(p);
    }
  }
  if (p->NextOfPE) {
    UInt hash = PRED_HASH(f,cur_mod,PredHashTableSize);
    READ_LOCK(PredHashRWLock);
    p = PredHash[hash];
    
    while (p) {
      if (p->FunctorOfPred == f &&
	  p->ModuleOfPred == cur_mod)
	{
	  READ_UNLOCK(PredHashRWLock);
	  WRITE_UNLOCK(f->FRWLock);
	  return AbsPredProp(p);
	}
      p = RepPredProp(p->NextOfPE);
    }
    READ_UNLOCK(PredHashRWLock);
  }
  return Yap_NewPredPropByFunctor(f,cur_mod);
}

Prop
Yap_PredPropByAtomNonThreadLocal(Atom at, Term cur_mod)
/* get predicate entry for ap/arity; create it if neccessary.              */
{
  Prop p0;
  AtomEntry *ae = RepAtom(at);

  WRITE_LOCK(ae->ARWLock);
  p0 = ae->PropsOfAE;
  while (p0) {
    PredEntry *pe = RepPredProp(p0);
    if ( pe->KindOfPE == PEProp && 
	 (pe->ModuleOfPred == cur_mod || !pe->ModuleOfPred)) {
      /* don't match multi-files */
      if (!(pe->PredFlags & MultiFileFlag) ||
	  pe->ModuleOfPred ||
	  !cur_mod ||
	  cur_mod == TermProlog) {
	WRITE_UNLOCK(ae->ARWLock);
	return(p0);
      }
    }
    p0 = pe->NextOfPE;
  }
  return Yap_NewPredPropByAtom(ae,cur_mod);
}



Term
Yap_GetValue(Atom a)
{
  Prop p0 = GetAProp(a, ValProperty);
  Term out;

  if (p0 == NIL)
    return (TermNil);
  READ_LOCK(RepValProp(p0)->VRWLock);
  out = RepValProp(p0)->ValueOfVE;
  if (IsApplTerm(out)) {
    Functor f = FunctorOfTerm(out);
    if (f == FunctorDouble) {
      out = MkFloatTerm(FloatOfTerm(out));
    } else if (f == FunctorLongInt) {
      out = MkLongIntTerm(LongIntOfTerm(out));
    }
#ifdef USE_GMP
    else {
      out = Yap_MkBigIntTerm(Yap_BigIntOfTerm(out));
    }
#endif
  }
  READ_UNLOCK(RepValProp(p0)->VRWLock);
  return (out);
}

void
Yap_PutValue(Atom a, Term v)
{
  AtomEntry *ae = RepAtom(a);
  Prop p0;
  ValEntry *p;
  Term t0;

  WRITE_LOCK(ae->ARWLock);
  p0 = GetAPropHavingLock(ae, ValProperty);
  if (p0 != NIL) {
    p = RepValProp(p0);
    WRITE_LOCK(p->VRWLock);
    WRITE_UNLOCK(ae->ARWLock);
  } else {
    p = (ValEntry *) Yap_AllocAtomSpace(sizeof(ValEntry));
    if (p == NULL) {
      WRITE_UNLOCK(ae->ARWLock);
      return;
    }
    p->KindOfPE = ValProperty;
    p->ValueOfVE = TermNil;
    AddPropToAtom(RepAtom(a), (PropEntry *)p);
    /* take care that the lock for the property will be inited even
       if someone else searches for the property */
    INIT_RWLOCK(p->VRWLock);
    WRITE_LOCK(p->VRWLock);
    WRITE_UNLOCK(ae->ARWLock);
  }
  t0 = p->ValueOfVE;
  if (IsFloatTerm(v)) {
    /* store a float in code space, so that we can access the property */
    union {
      Float f;
      CELL ar[sizeof(Float) / sizeof(CELL)];
    } un;
    CELL *pt, *iptr;
    unsigned int i;

    un.f = FloatOfTerm(v);
    if (IsFloatTerm(t0)) {
      pt = RepAppl(t0);
    } else {
      if (IsApplTerm(t0)) {
	Yap_FreeCodeSpace((char *) (RepAppl(t0)));
      }
      pt = (CELL *) Yap_AllocAtomSpace(sizeof(CELL)*(1 + 2*sizeof(Float)/sizeof(CELL)));
      if (pt == NULL) {
	WRITE_UNLOCK(ae->ARWLock);
	return;
      }
      p->ValueOfVE = AbsAppl(pt);
      pt[0] = (CELL)FunctorDouble;
    }

    iptr = pt+1;
    for (i = 0; i < sizeof(Float) / sizeof(CELL); i++) {
      *iptr++ = (CELL)un.ar[i];
    }
  } else if (IsLongIntTerm(v)) {
    CELL *pt;
    Int val = LongIntOfTerm(v);

    if (IsLongIntTerm(t0)) {
      pt = RepAppl(t0);
    } else {
      if (IsApplTerm(t0)) {
	Yap_FreeCodeSpace((char *) (RepAppl(t0)));
      }
      pt = (CELL *) Yap_AllocAtomSpace(2*sizeof(CELL));
      if (pt == NULL) {
	WRITE_UNLOCK(ae->ARWLock);
	return;
      }
      p->ValueOfVE = AbsAppl(pt);
      pt[0] = (CELL)FunctorLongInt;
    }
    pt[1] = (CELL)val;
#ifdef USE_GMP
  } else if (IsBigIntTerm(v)) {
    CELL *ap = RepAppl(v);
    Int sz = 
      sizeof(MP_INT)+sizeof(CELL)+
      (((MP_INT *)(ap+1))->_mp_alloc*sizeof(mp_limb_t));
    CELL *pt = (CELL *) Yap_AllocAtomSpace(sz);

    if (pt == NULL) {
      WRITE_UNLOCK(ae->ARWLock);
      return;
    }
    if (IsApplTerm(t0)) {
      Yap_FreeCodeSpace((char *) RepAppl(t0));
    }
    memcpy((void *)pt, (void *)ap, sz);
    p->ValueOfVE = AbsAppl(pt);
#endif
  } else {
    if (IsApplTerm(t0)) {
      /* recover space */
      Yap_FreeCodeSpace((char *) (RepAppl(p->ValueOfVE)));
    }
    p->ValueOfVE = v;
  }
  WRITE_UNLOCK(p->VRWLock);
}

Term
Yap_StringToList(char *s)
{
  CACHE_REGS
  register Term t;
  register unsigned char *cp = (unsigned char *)s + strlen(s);

  t = MkAtomTerm(AtomNil);
  while (cp > (unsigned char *)s) {
    t = MkPairTerm(MkIntTerm(*--cp), t);
  }
  return (t);
}

Term
Yap_NStringToList(char *s, size_t len)
{
  CACHE_REGS
  Term t;
  unsigned char *cp = (unsigned char *)s + len;

  t = MkAtomTerm(AtomNil);
  while (cp > (unsigned char *)s) {
    t = MkPairTerm(MkIntegerTerm(*--cp), t);
  }
  return t;
}


Term
Yap_WideStringToList(wchar_t *s)
{
  CACHE_REGS
  Term t;
  wchar_t *cp = s + wcslen(s);

  t = MkAtomTerm(AtomNil);
  while (cp > s) {
    if (ASP < H+1024)
      return (CELL)0;    
    t = MkPairTerm(MkIntegerTerm(*--cp), t);
  }
  return t;
}

Term
Yap_NWideStringToList(wchar_t *s, size_t len)
{
  CACHE_REGS
  Term t;
  wchar_t *cp = s + len;

  t = MkAtomTerm(AtomNil);
  while (cp > s) {
    if (ASP < H+1024)
      return (CELL)0;    
    t = MkPairTerm(MkIntegerTerm(*--cp), t);
  }
  return t;
}

Term
Yap_StringToDiffList(char *s, Term t USES_REGS)
{
  register unsigned char *cp = (unsigned char *)s + strlen(s);

 t = Yap_Globalise(t);
  while (cp > (unsigned char *)s) {
    if (ASP < H+1024)
      return (CELL)0;
    t = MkPairTerm(MkIntTerm(*--cp), t);
  }
  return t;
}

Term
Yap_NStringToDiffList(char *s, Term t, size_t len)
{
  CACHE_REGS
  register unsigned char *cp = (unsigned char *)s + len;

  t = Yap_Globalise(t);
  while (cp > (unsigned char *)s) {
    t = MkPairTerm(MkIntTerm(*--cp), t);
  }
  return t;
}

Term
Yap_WideStringToDiffList(wchar_t *s, Term t)
{
  CACHE_REGS
 wchar_t *cp = s + wcslen(s);

  t = Yap_Globalise(t);
  while (cp > s) {
    t = MkPairTerm(MkIntegerTerm(*--cp), t);
  }
  return t;
}

Term
Yap_NWideStringToDiffList(wchar_t *s, Term t, size_t len)
{
  CACHE_REGS
 wchar_t *cp = s + len;

 t = Yap_Globalise(t);
  while (cp > s) {
    t = MkPairTerm(MkIntegerTerm(*--cp), t);
  }
  return t;
}

Term
Yap_StringToListOfAtoms(char *s)
{
  CACHE_REGS
  register Term t;
  char so[2];
  register unsigned char *cp = (unsigned char *)s + strlen(s);

  so[1] = '\0';
  t = MkAtomTerm(AtomNil);
  while (cp > (unsigned char *)s) {
    so[0] = *--cp;
    t = MkPairTerm(MkAtomTerm(LookupAtom(so)), t);
  }
  return t;
}

Term
Yap_NStringToListOfAtoms(char *s, size_t len)
{
  CACHE_REGS
  register Term t;
  char so[2];
  register unsigned char *cp = (unsigned char *)s + len;

  so[1] = '\0';
  t = MkAtomTerm(AtomNil);
  while (cp > (unsigned char *)s) {
    so[0] = *--cp;
    t = MkPairTerm(MkAtomTerm(LookupAtom(so)), t);
  }
  return t;
}

Term
Yap_WideStringToListOfAtoms(wchar_t *s)
{
  CACHE_REGS
  register Term t;
  wchar_t so[2];
  wchar_t *cp = s + wcslen(s);

  so[1] = '\0';
  t = MkAtomTerm(AtomNil);
  while (cp > s) {
    so[0] = *--cp;
    if (ASP < H+1024)
      return (CELL)0;    
    t = MkPairTerm(MkAtomTerm(LookupWideAtom(so)), t);
  }
  return t;
}

Term
Yap_NWideStringToListOfAtoms(wchar_t *s, size_t len)
{
  CACHE_REGS
  register Term t;
  wchar_t so[2];
  wchar_t *cp = s + len;

  so[1] = '\0';
  t = MkAtomTerm(AtomNil);
  while (cp > s) {
    if (ASP < H+1024)
      return (CELL)0;    
    so[0] = *--cp;
    t = MkPairTerm(MkAtomTerm(LookupWideAtom(so)), t);
  }
  return t;
}

Term
Yap_NWideStringToDiffListOfAtoms(wchar_t *s, Term t0, size_t len)
{
  CACHE_REGS
  register Term t;
  wchar_t so[2];
  wchar_t *cp = s + len;

  so[1] = '\0';
  t = Yap_Globalise(t0);
  while (cp > s) {
    so[0] = *--cp;
    t = MkPairTerm(MkAtomTerm(LookupWideAtom(so)), t);
  }
  return t;
}

Term
Yap_ArrayToList(register Term *tp, int nof)
{
  CACHE_REGS
  register Term *pt = tp + nof;
  register Term t;

  t = MkAtomTerm(AtomNil);
  while (pt > tp) {
    Term tm = *--pt;
#if YAPOR_SBA
    if (tm == 0)
      t = MkPairTerm((CELL)pt, t);
    else
#endif
      t = MkPairTerm(tm, t);
  }
  return (t);
}

int
Yap_GetName(char *s, UInt max, Term t)
{
  register Term Head;
  register Int i;

  if (IsVarTerm(t) || !IsPairTerm(t))
    return FALSE;
  while (IsPairTerm(t)) {
    Head = HeadOfTerm(t);
    if (!IsNumTerm(Head))
      return (FALSE);
    i = IntOfTerm(Head);
    if (i < 0 || i > MAX_ISO_LATIN1)
      return FALSE;
    *s++ = i;
    t = TailOfTerm(t);
    if (--max == 0) {
      Yap_Error(FATAL_ERROR,t,"not enough space for GetName");      
    }
  }
  *s = '\0';
  return TRUE;
}

#ifdef SFUNC

Term
MkSFTerm(Functor f, int n, Term *a, empty_value)
{
  Term t, p = AbsAppl(H);
  int i;

  *H++ = f;
  RESET_VARIABLE(H);
  ++H;
  for (i = 1; i <= n; ++i) {
    t = Derefa(a++);
    if (t != empty_value) {
      *H++ = i;
      *H++ = t;
    }
  }
  *H++ = 0;
  return (p);
}

CELL *
ArgsOfSFTerm(Term t)
{
  CELL *p = RepAppl(t) + 1;

  while (*p != (CELL) p)
    p = CellPtr(*p) + 1;
  return (p + 1);
}

#endif

Int
Yap_NewSlots(int n USES_REGS)
{
  Int old_slots = IntOfTerm(ASP[0]), oldn = n;
  while (n > 0) {
    RESET_VARIABLE(ASP);
    ASP--;
    n--;
  }
  ASP[0] = MkIntTerm(old_slots+oldn);
  CurSlot = LCL0-ASP;
  return((ASP+1)-LCL0);
}

Int
Yap_InitSlot(Term t USES_REGS)
{
  Int old_slots = IntOfTerm(ASP[0]);
  *ASP = t;
  ASP--;
  CurSlot ++;
  ASP[0] = MkIntTerm(old_slots+1);
  return((ASP+1)-LCL0);
}

int
Yap_RecoverSlots(int n USES_REGS)
{
  Int old_slots = IntOfTerm(ASP[0]);
  if (old_slots - n < 0) {
    return FALSE;
  }
  ASP += n;
  CurSlot -= n;
  ASP[0] = MkIntTerm(old_slots-n);
  return TRUE;
}

static HoldEntry *
InitAtomHold(void)
{
  HoldEntry *x = (HoldEntry *)Yap_AllocAtomSpace(sizeof(struct hold_entry));
  if (x == NULL) {
    return NULL;
  }
  x->KindOfPE = HoldProperty;
  x->NextOfPE = NIL;
  x->RefsOfPE = 1;
  return x;
}

int
Yap_AtomIncreaseHold(Atom at)
{
  AtomEntry *ae = RepAtom(at);
  HoldEntry *pp;
  Prop *opp = &(ae->PropsOfAE);

  WRITE_LOCK(ae->ARWLock);
  pp = RepHoldProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp)
	 && pp->KindOfPE != HoldProperty) {
    opp = &(pp->NextOfPE);
    pp = RepHoldProp(pp->NextOfPE);
  }
  if (!pp) {
    HoldEntry *new = InitAtomHold();
    if (!new) {
      WRITE_UNLOCK(ae->ARWLock);
      return FALSE;
    }
    *opp = AbsHoldProp(new);
  } else {
    pp->RefsOfPE++;
  }
  WRITE_UNLOCK(ae->ARWLock);
  return TRUE;
}

int
Yap_AtomDecreaseHold(Atom at)
{
  AtomEntry *ae = RepAtom(at);
  HoldEntry *pp;
  Prop *opp = &(ae->PropsOfAE);

  WRITE_LOCK(ae->ARWLock);
  pp = RepHoldProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp)
	 && pp->KindOfPE != HoldProperty) {
    opp = &(pp->NextOfPE);
    pp = RepHoldProp(pp->NextOfPE);
  }
  if (!pp) {
    WRITE_UNLOCK(ae->ARWLock);
    return FALSE;
  }
  pp->RefsOfPE--;
  if (!pp->RefsOfPE) {
    *opp = pp->NextOfPE;
    Yap_FreeCodeSpace((ADDR)pp);
  }
  WRITE_UNLOCK(ae->ARWLock);
  return TRUE;
}

