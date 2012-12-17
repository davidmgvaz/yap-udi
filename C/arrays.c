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
* File:		arrays.c						 *
* Last rev:								 *
* mods:									 *
* comments:	Array Manipulation Routines	                         *
*									 *
*************************************************************************/

#include "Yap.h"
#include "clause.h"
#include "eval.h"
#include "heapgc.h"
#if HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#if __simplescalar__
#ifdef HAVE_MMAP
#undef HAVE_MMAP
#endif
#endif

STATIC_PROTO(Int  p_compile_array_refs, ( USES_REGS1 ));
STATIC_PROTO(Int  p_array_refs_compiled, ( USES_REGS1 ));
STATIC_PROTO(Int  p_sync_mmapped_arrays, ( USES_REGS1 ));

/*
 * 
 * This file works together with pl/arrays.yap and arrays.h.
 * 
 * YAP now supports a very simple notion of arrays. Arrays may be
 * allocated dynamically or statically:
 * 
 * o anonymous arrays are created during execution and allocated
 * in the heap. They have the lifetime of any other other heap
 * object. Any term can be an argument to a dynamic array.
 * 
 * Dynamic arrays are named as a free variable and are
 * initialised with free variables. 
 * 
 * o named arrays are created during execution but allocated
 * in the code space. They have the lifetime of an heap
 * object. Any term can be an argument to a dynamic array.
 * 
 * Named arrays are named with atoms and are initialised with
 * free variables.
 * 
 * o static arrays are allocated in the heap. Their space is
 * never recovered unless explictly said so by the
 * program. Arguments to these arrays must have fixed size,
 * and can only be atomic (at least for now).
 * 
 * Static arrays can be named through an  atom. They are
 * initialised with [].
 * 
 * Users create arrays by a declaration X array Arity. If X is an atom
 * A, then this it is a static array and A's the array name, otherwise
 * X refers to a dynamic array.
 * 
 * As in C, arrays start counting from 0.
 * 
 * Users access arrays by a token X[I] or a[I], this token can appear
 * anywhere within the computation, so a[2] = X[3*4] means that the
 * second element of global array a should unify with the 12th element
 * of array X. The mechanism used to implement this is the same
 * mechanism used to implement suspension variables.
 * 
 * Representation:
 * 
 * Dynamic Arrays are represented as a compound term of arity N, where
 * N is the size of the array. Even so, I will not include array bound
 * checking for now.
 * 
 * |--------------------------------------------------------------|
 * | $ARRAY/N|....
 * |______________________________________________________________
 * 
 * 
 * Unbound Var is used as a place to point to.
 * 
 * Static Arrays are represented as a special property for an atom,
 * with field size and 
 * 
 * A term of the form X[I] is represented as a Reference pointing to
 * the compound term:
 * 
 * '$array_arg'(X,I)
 * 
 * Dereferecing will automatically find X[I].
 * 
 * The only exception is the compiler, which uses a different
 * dereferencing routine. The clause cl(a[2], Y[X], Y) will be
 * compiled as:
 * 
 * cl(A, B, Y) :- '$access_array'(a, A, 2), '$access_array'(Y, B, X).
 * 
 * There are three operations to access arrays:
 * 
 * X[I] = A, This is normal unification.
 * 
 * X[I] := A, This is multiassignment, and therefore
 * backtrackable.
 * 
 * X[I] ::= A, This is non-backtrackable multiassignment, ans most
 * useful for static arrays.
 * 
 * The LHS of := and of ::= must be an array element!
 * 
 */

STATIC_PROTO(Int  p_create_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_create_mmapped_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_array_references, ( USES_REGS1 ));
STATIC_PROTO(Int  p_create_static_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_resize_static_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_close_static_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_access_array, ( USES_REGS1 ));
STATIC_PROTO(Int  p_assign_static, ( USES_REGS1 ));
STATIC_PROTO(Int  p_assign_dynamic, ( USES_REGS1 ));

#if HAVE_MMAP

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* keep a list of mmaped blocks to synch on exit */

typedef struct MMAP_ARRAY_BLOCK {
  Atom name;
  void *start;
  size_t  size;
  Int items;
  int  fd;
  struct MMAP_ARRAY_BLOCK *next;
} mmap_array_block;


static Int
CloseMmappedArray(StaticArrayEntry *pp, void *area USES_REGS)
{
  mmap_array_block *ptr = GLOBAL_mmap_arrays, *optr = GLOBAL_mmap_arrays;

  while (ptr != NULL && ptr->start != area) {
    ptr = ptr->next;
    optr = ptr;
  }
  if (ptr == NULL) {
#if !defined(USE_SYSTEM_MALLOC)
    Yap_Error(SYSTEM_ERROR,ARG1,"close_mmapped_array (array chain incoherent)", strerror(errno));
#endif
    return FALSE;
  }
  if (munmap(ptr->start, ptr->size) == -1) {
      Yap_Error(SYSTEM_ERROR,ARG1,"close_mmapped_array (munmap: %s)", strerror(errno));
      return(FALSE);
  }
  optr->next = ptr->next;
  pp->ValueOfVE.ints = NULL;
  pp->ArrayEArity = 0;
  if (close(ptr->fd) < 0) {
    Yap_Error(SYSTEM_ERROR,ARG1,"close_mmapped_array (close: %s)", strerror(errno));
    return(FALSE);
  }
  Yap_FreeAtomSpace((char *)ptr);
  return(TRUE);
}

static void
ResizeMmappedArray(StaticArrayEntry *pp, Int dim, void *area USES_REGS)
{
  mmap_array_block *ptr = GLOBAL_mmap_arrays;
  size_t total_size; 
  while (ptr != NULL && ptr->start != area) {
    ptr = ptr->next;
  }
  if (ptr == NULL)
    return;
  /* This is a very stupid algorithm to change size for an array.

     First, we unmap it, then we actually change the size for the file,
     and last we initialise again
  */
  if (munmap(ptr->start, ptr->size) == -1) {
      Yap_Error(SYSTEM_ERROR,ARG1,"resize_mmapped_array (munmap: %s)", strerror(errno));
      return;
  }
  total_size = (ptr->size / ptr->items)*dim;
  if (ftruncate(ptr->fd, total_size) < 0) {
    Yap_Error(SYSTEM_ERROR,ARG1,"resize_mmapped_array (ftruncate: %s)", strerror(errno));
    return;
  }
  if (lseek(ptr->fd, total_size-1, SEEK_SET) < 0) {
    Yap_Error(SYSTEM_ERROR,ARG1,"resize_mmapped_array (lseek: %s)", strerror(errno));
    return;
  }
  if (write(ptr->fd, "", 1) < 0) {
    Yap_Error(SYSTEM_ERROR,ARG1,"resize_mmapped_array (write: %s)", strerror(errno));
    return;
  }
  if ((ptr->start = (void *)mmap(0, (size_t) total_size, PROT_READ | PROT_WRITE, MAP_SHARED, ptr->fd, 0)) == (void *) - 1) {
    Yap_Error(SYSTEM_ERROR,ARG1,"resize_mmapped_array (mmap: %s)", strerror(errno));
    return;
  }
  ptr->size = total_size;
  ptr->items = dim;
  pp->ValueOfVE.chars = ptr->start;
}

#endif

static Term
GetTermFromArray(DBTerm *ref USES_REGS)
{
  if (ref != NULL) {
    Term TRef;

    while ((TRef = Yap_FetchTermFromDB(ref)) == 0L) {
      if (LOCAL_Error_TYPE == OUT_OF_ATTVARS_ERROR) {
	LOCAL_Error_TYPE = YAP_NO_ERROR;
	if (!Yap_growglobal(NULL)) {
	  Yap_Error(OUT_OF_ATTVARS_ERROR, TermNil, LOCAL_ErrorMessage);
	  return TermNil;
	}
      } else {
	LOCAL_Error_TYPE = YAP_NO_ERROR;
	if (!Yap_gcl(LOCAL_Error_Size, 3, ENV, gc_P(P,CP))) {
	  Yap_Error(OUT_OF_STACK_ERROR, TermNil, LOCAL_ErrorMessage);
	  return TermNil;
	}
      }
    }
    return TRef;
  } else {
    P = (yamop *)FAILCODE;
    return TermNil;
  }
}

static Term
GetNBTerm(live_term *ar, Int indx USES_REGS)
{
  /* The object is now in use */
  Term livet = ar[indx].tlive;

  if (!IsVarTerm(livet)) {
    if (!IsApplTerm(livet)) {
      return livet;
    } else if (FunctorOfTerm(livet) == FunctorAtFoundOne) {
      return Yap_ReadTimedVar(livet);
    } else {
      return livet;
    }
  } else {
    Term termt = ar[indx].tstore;

    if (!IsUnboundVar(&(ar[indx].tlive))) {
      return livet;
    }
    if (IsVarTerm(termt)) {
      livet = MkVarTerm();
    } else if (IsAtomicTerm(termt)) {
      livet = termt;
    } else {
      DBTerm *ref = (DBTerm *)RepAppl(termt);
      if ((livet = GetTermFromArray(ref PASS_REGS)) == TermNil) {
	return TermNil;
      }
    }
    Bind(&(ar[indx].tlive), livet);
    return livet;
  }
}

static Term
AccessNamedArray(Atom a, Int indx USES_REGS)
{
  AtomEntry *ae = RepAtom(a);
  ArrayEntry *pp; 

  READ_LOCK(ae->ARWLock);
  pp = RepArrayProp(ae->PropsOfAE);
  while (!EndOfPAEntr(pp) &&
	 pp->KindOfPE != ArrayProperty
#if THREADS
	 && pp->owner_id != worker_id
#endif
	 )
    pp = RepArrayProp(pp->NextOfPE);
  READ_UNLOCK(ae->ARWLock);

  if (!EndOfPAEntr(pp)) {
    if (ArrayIsDynamic(pp)) {
      Term out;
      READ_LOCK(pp->ArRWLock);
      if (IsVarTerm(pp->ValueOfVE) ||
	  pp->ArrayEArity <= indx ||
	  indx < 0) {
	READ_UNLOCK(pp->ArRWLock);
	P = (yamop *)FAILCODE;
	return(MkAtomTerm(AtomFoundVar));
      }
      out = RepAppl(pp->ValueOfVE)[indx+1];
      READ_UNLOCK(pp->ArRWLock);
      return(out);
    } else {
      StaticArrayEntry *ptr = (StaticArrayEntry *)pp;

      READ_LOCK(ptr->ArRWLock);
      if (-(pp->ArrayEArity) <= indx || indx < 0) {
	/*	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW, MkIntegerTerm(indx), "access_array");*/
	READ_UNLOCK(ptr->ArRWLock);
	P = (yamop *)FAILCODE;
	return(MkAtomTerm(AtomFoundVar));
      }
      switch (ptr->ArrayType) {

      case array_of_ints:
	{
	  Term out;
	  out = MkIntegerTerm(ptr->ValueOfVE.ints[indx]);
	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_doubles:
	{
	  Term out;
	  out = MkEvalFl(ptr->ValueOfVE.floats[indx]);
	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_ptrs:
	{
	  Term out;
	  out = MkIntegerTerm((Int)(ptr->ValueOfVE.ptrs[indx]));
	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_atoms:
	{
	  Term out;
	  out = ptr->ValueOfVE.atoms[indx];
	  READ_UNLOCK(ptr->ArRWLock);
	  if (out == 0L)
	    return TermNil;
	  else
	    return out;
	}
	/* just return the atom */
      case array_of_chars:
	{
	  Term out;
	  out = MkIntegerTerm((Int)(ptr->ValueOfVE.chars[indx]));
	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_uchars:
	{
	  Term out;
	  out = MkIntegerTerm((Int)(ptr->ValueOfVE.uchars[indx]));
	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_dbrefs:
	{
	  /* The object is now in use */
	  Term TRef = ptr->ValueOfVE.dbrefs[indx];

	  READ_UNLOCK(ptr->ArRWLock);
	  if (TRef != 0L) {
	    DBRef ref = DBRefOfTerm(TRef);

#if MULTIPLE_STACKS
	    LOCK(ref->lock);
	    INC_DBREF_COUNT(ref);
	    TRAIL_REF(ref);	/* So that fail will erase it */
	    UNLOCK(ref->lock);
#else
	    if (ref->Flags & LogUpdMask) {
	      LogUpdClause *cl = (LogUpdClause *)ref;

	      if (!(cl->ClFlags & InUseMask)) {
		cl->ClFlags |= InUseMask;
		TRAIL_CLREF(cl);
	      }
	    } else {
	      if (!(ref->Flags & InUseMask)) {
		ref->Flags |= InUseMask;
		TRAIL_REF(ref);	/* So that fail will erase it */
	      }
	    }
#endif
	  } else {
	    P = (yamop *)FAILCODE;
	    TRef = TermNil;
	  }
	  return TRef;
	}
      case array_of_nb_terms:
	{
	  /* The object is now in use */
	  Term out = GetNBTerm(ptr->ValueOfVE.lterms, indx PASS_REGS);

	  READ_UNLOCK(ptr->ArRWLock);
	  return out;
	}
      case array_of_terms:
	{
	  /* The object is now in use */
	  DBTerm *ref = ptr->ValueOfVE.terms[indx];

	  READ_UNLOCK(ptr->ArRWLock);
	  return GetTermFromArray(ref PASS_REGS);
	}
      default:
	READ_UNLOCK(ptr->ArRWLock);
	return TermNil;
      }
    }      
  }
  else {
    Yap_Error(EXISTENCE_ERROR_ARRAY,MkAtomTerm(a),"named array");
    return (TermNil);
  }

}

static Int 
p_access_array( USES_REGS1 )
{
  Term t = Deref(ARG1);
  Term ti = Deref(ARG2);
  Term tf;
  Int indx;

  if (IsNonVarTerm(ti)) {
    Term nti;
    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      indx = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"access_array");
      return (FALSE);
    }
  }
  else {
    Yap_Error(INSTANTIATION_ERROR,ti,"access_array");
    return (TermNil);
  }

  if (IsNonVarTerm(t)) {
    if (IsApplTerm(t)) {
      if (indx >= ArityOfFunctor(FunctorOfTerm(t)) || indx < 0) {
	/*	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW, MkIntegerTerm(indx), "access_array");*/
	P = (yamop *)FAILCODE;
	return(FALSE);
      }
      tf = (RepAppl(t))[indx + 1];
    } else if (IsAtomTerm(t)) {
      tf = AccessNamedArray(AtomOfTerm(t), indx PASS_REGS);
      if (tf == MkAtomTerm(AtomFoundVar)) {
	return(FALSE);
      }
    } else {
      Yap_Error(TYPE_ERROR_ARRAY,t,"access_array");
      return(FALSE);
    }    
  } else {
    Yap_Error(INSTANTIATION_ERROR,t,"access_array");
    return(FALSE);
  }
  return Yap_unify(tf, ARG3);
}

static Int 
p_array_arg( USES_REGS1 )
{
  register Term ti = Deref(ARG3), t;
  register Int indx;

  if (IsNonVarTerm(ti)) {
    Term nti;
    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      indx = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"access_array");
      return (FALSE);
    }
  }
  else {
    Yap_Error(INSTANTIATION_ERROR,ti,"array_arg");
    return (FALSE);
  }

  t = Deref(ARG2);
  if (IsNonVarTerm(t)) {
    if (IsApplTerm(t)) {
      return (Yap_unify(((RepAppl(t))[indx + 1]), ARG1));
    }
    else if (IsAtomTerm(t)) {
      Term tf = AccessNamedArray(AtomOfTerm(t), indx PASS_REGS);
      if (tf == MkAtomTerm(AtomFoundVar)) {
	return(FALSE);
      }
      return (Yap_unify(tf, ARG1));
    }
    else
      Yap_Error(TYPE_ERROR_ARRAY,t,"array_arg");
  }
  else
    Yap_Error(INSTANTIATION_ERROR,t,"array_arg");

  return (FALSE);

}

static void
InitNamedArray(ArrayEntry * p, Int dim USES_REGS)
{
  Term *tp;

  WRITE_LOCK(p->ArRWLock);
  /* Leave a pointer so that we can reclaim array space when
   * we backtrack or when we abort */
  /* place terms in reverse order */
  Bind_Global(&(p->ValueOfVE),AbsAppl(H));
  tp = H;
  tp[0] =  (CELL)Yap_MkFunctor(AtomArray, dim);
  tp++;
  p->ArrayEArity = dim;
  /* Initialise the array as a set of variables */
  H = tp+dim;
  for (; tp < H; tp++) {
    RESET_VARIABLE(tp);
  }
  WRITE_UNLOCK(p->ArRWLock);
}

/* we assume the atom ae is already locked */
static void
CreateNamedArray(PropEntry * pp, Int dim, AtomEntry *ae USES_REGS)
{
  ArrayEntry *p;

  p = (ArrayEntry *) Yap_AllocAtomSpace(sizeof(*p));
  p->KindOfPE = ArrayProperty;
  AddPropToAtom(ae, (PropEntry *)p);
  INIT_RWLOCK(p->ArRWLock);
#if THREADS
  p->owner_id = worker_id;
#endif
  p->NextAE = LOCAL_DynamicArrays;
  LOCAL_DynamicArrays = p;
  InitNamedArray(p, dim PASS_REGS);

}

static void
AllocateStaticArraySpace(StaticArrayEntry *p, static_array_types atype, Int array_size USES_REGS)
{
  Int asize = 0;
  switch (atype) {
  case array_of_doubles:
    asize = array_size*sizeof(Float);
    break;
  case array_of_ints:
    asize = array_size*sizeof(Int);
    break;
  case array_of_chars:
    asize = array_size*sizeof(char);
    break;
  case array_of_uchars:
    asize = array_size*sizeof(unsigned char);
    break;
  case array_of_ptrs:
    asize = array_size*sizeof(AtomEntry *);
    break;
  case array_of_atoms:
  case array_of_terms:
  case array_of_nb_terms:
    asize = array_size*sizeof(live_term);
    break;
  case array_of_dbrefs:
    asize = array_size*sizeof(DBRef);
    break;
  }
  while ((p->ValueOfVE.floats = (Float *) Yap_AllocAtomSpace(asize) ) == NULL) {
    YAPLeaveCriticalSection();
    if (!Yap_growheap(FALSE, asize, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR, TermNil, LOCAL_ErrorMessage);
      return;
    }
    YAPEnterCriticalSection();
  }
}

/* ae and p are assumed to be locked, if they exist */
static StaticArrayEntry *
CreateStaticArray(AtomEntry *ae, Int dim, static_array_types type, CODEADDR start_addr, StaticArrayEntry *p USES_REGS)
{
  if (EndOfPAEntr(p)) {
    while ((p = (StaticArrayEntry *) Yap_AllocAtomSpace(sizeof(*p))) == NULL) {
      if (!Yap_growheap(FALSE, sizeof(*p), NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR, TermNil, LOCAL_ErrorMessage);
	return NULL;
      }
    }
    p->KindOfPE = ArrayProperty;
    INIT_RWLOCK(p->ArRWLock);
    AddPropToAtom(ae, (PropEntry *)p);
    p->NextAE = LOCAL_StaticArrays;
    LOCAL_StaticArrays = p;
  }
  WRITE_LOCK(p->ArRWLock);
  p->ArrayEArity = -dim;
  p->ArrayType = type;
  if (start_addr == NULL) {
    Int i;

    AllocateStaticArraySpace(p, type, dim PASS_REGS);
    if (p->ValueOfVE.ints == NULL) {
      WRITE_UNLOCK(p->ArRWLock);
      return p;
    }
    switch(type) {
    case array_of_ints:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.ints[i] = 0;
      break;
    case array_of_chars:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.chars[i] = '\0';
      break;
    case array_of_uchars:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.uchars[i] = '\0';
      break;
    case array_of_doubles:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.floats[i] = 0.0;
      break;
    case array_of_ptrs:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.ptrs[i] = NULL;
      break;
    case array_of_atoms:
    case array_of_dbrefs:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.atoms[i] = 0L;
      break;
    case array_of_terms:
      for (i = 0; i < dim; i++)
	p->ValueOfVE.terms[i] = NULL;
      break;
    case array_of_nb_terms:
      for (i = 0; i < dim; i++) {
	RESET_VARIABLE(&(p->ValueOfVE.lterms[i].tlive));
	p->ValueOfVE.lterms[i].tstore = TermNil;
      }
      break;
    }
  } else {
    /* external array */
    p->ValueOfVE.chars = (char *)start_addr;
  }
  WRITE_UNLOCK(p->ArRWLock);
  return p;
}

static void
ResizeStaticArray(StaticArrayEntry *pp, Int dim USES_REGS)
{
  statarray_elements old_v = pp->ValueOfVE;
  static_array_types type = pp->ArrayType;
  Int old_dim = - pp->ArrayEArity;
  Int mindim = (dim < old_dim ? dim : old_dim), i;

  /* change official size */
  if (pp->ArrayEArity >= 0){
    return;
  }
  WRITE_LOCK(pp->ArRWLock);
  pp->ArrayEArity = -dim;
#if HAVE_MMAP
  if (pp->ValueOfVE.chars < (char *)Yap_HeapBase || 
      pp->ValueOfVE.chars > (char *)HeapTop) {
    ResizeMmappedArray(pp, dim, (void *)(pp->ValueOfVE.chars) PASS_REGS);
    WRITE_UNLOCK(pp->ArRWLock);
    return;
  }
#endif
  AllocateStaticArraySpace(pp, type, dim PASS_REGS);
  switch(type) {
  case array_of_ints:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.ints[i] = old_v.ints[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.ints[i] = 0;
    break;
  case array_of_chars:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.chars[i] = old_v.chars[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.chars[i] = '\0';
    break;
  case array_of_uchars:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.uchars[i] = old_v.uchars[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.uchars[i] = '\0';
    break;
  case array_of_doubles:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.floats[i] = old_v.floats[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.floats[i] = 0.0;
    break;
  case array_of_ptrs:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.ptrs[i] = old_v.ptrs[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.ptrs[i] = NULL;
    break;
  case array_of_atoms:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.atoms[i] = old_v.atoms[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.atoms[i] = TermNil;
    break;
  case array_of_dbrefs:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.dbrefs[i] = old_v.dbrefs[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.dbrefs[i] = 0L;
    break;
  case array_of_terms:
    for (i = 0; i <mindim; i++)
      pp->ValueOfVE.terms[i] = old_v.terms[i];
    for (i = mindim; i<dim; i++)
      pp->ValueOfVE.terms[i] = NULL;
    break;
  case array_of_nb_terms:
    for (i = 0; i <mindim; i++) {
      Term tlive = pp->ValueOfVE.lterms[i].tlive;
      if (IsVarTerm(tlive) && IsUnboundVar(&(pp->ValueOfVE.lterms[i].tlive))) {
	RESET_VARIABLE(&(pp->ValueOfVE.lterms[i].tlive));
      } else {
	pp->ValueOfVE.lterms[i].tlive = tlive;
      }
      pp->ValueOfVE.lterms[i].tstore = old_v.lterms[i].tstore;
    }
    break;
  }
  WRITE_UNLOCK(pp->ArRWLock);
}

static void
ClearStaticArray(StaticArrayEntry *pp)
{
  statarray_elements old_v = pp->ValueOfVE;
  static_array_types type = pp->ArrayType;
  Int dim = - pp->ArrayEArity, i;

  /* change official size */
  if (pp->ArrayEArity >= 0){
    return;
  }
  WRITE_LOCK(pp->ArRWLock);
  switch(type) {
  case array_of_ints:
    memset((void *)pp->ValueOfVE.ints,0,sizeof(Int)*dim);
    break;
  case array_of_chars:
    memset((void *)pp->ValueOfVE.chars,0,sizeof(char)*dim);
    break;
  case array_of_uchars:
    memset((void *)pp->ValueOfVE.uchars,0,sizeof(unsigned char)*dim);
    break;
  case array_of_doubles:
    memset((void *)pp->ValueOfVE.floats,0,sizeof(double)*dim);
    break;
  case array_of_ptrs:
    memset((void *)pp->ValueOfVE.ptrs,0,sizeof(void *)*dim);
    break;
  case array_of_atoms:
    for (i = 0;  i< dim; i++)
      pp->ValueOfVE.atoms[i] = TermNil;
    break;
  case array_of_dbrefs:
    for (i = 0; i < dim; i++) {
      Term t0 = pp->ValueOfVE.dbrefs[i];
      if (t0 != 0L) {
	DBRef ptr = DBRefOfTerm(t0);

	if (ptr->Flags & LogUpdMask) {
	  LogUpdClause *lup = (LogUpdClause *)ptr;
	  //	  LOCK(lup->ClLock);
	  lup->ClRefCount--;
	  if (lup->ClRefCount == 0 &&
	      (lup->ClFlags & ErasedMask) &&
	      !(lup->ClFlags & InUseMask)) {
	    //	    UNLOCK(lup->ClLock);
	    Yap_ErLogUpdCl(lup);
	  } else {
	    //	    UNLOCK(lup->ClLock);
	  }
	} else {
	  ptr->NOfRefsTo--;
	  if (ptr->NOfRefsTo == 0 &&
	      (ptr->Flags & ErasedMask) &&
	      !(ptr->Flags & InUseMask)) {
	    Yap_ErDBE(ptr);
	  }
	}
      }
      pp->ValueOfVE.dbrefs[i] = 0L;
    }
    break;
  case array_of_terms:
    for (i = 0; i < dim; i++) {
      DBTerm *ref = pp->ValueOfVE.terms[i];

      if (ref != NULL) {
	Yap_ReleaseTermFromDB(ref);
      }
      pp->ValueOfVE.terms[i] = NULL;
    }
    break;
  case array_of_nb_terms:
    for (i = 0; i < dim; i++) {
      Term told = pp->ValueOfVE.lterms[i].tstore;
      CELL *livep = &(pp->ValueOfVE.lterms[i].tlive);

      RESET_VARIABLE(livep);
      /* recover space */
      if (IsApplTerm(told)) {
	Yap_ReleaseTermFromDB((DBTerm *)RepAppl(told));
      }
      pp->ValueOfVE.lterms[i].tstore = old_v.lterms[i].tstore;
    }
    break;
  }
  WRITE_UNLOCK(pp->ArRWLock);
}

/* create an array (?Name, + Size) */
static Int 
p_create_array( USES_REGS1 )
{
  Term ti;
  Term t;
  Int size;

 restart:
  ti = Deref(ARG2);
  t = Deref(ARG1);
  {
    Term nti;
    if (IsVarTerm(ti)) {
      Yap_Error(INSTANTIATION_ERROR,ti,"create_array");
      return (FALSE);
    }
    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      size = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"create_array");
      return (FALSE);
    }
  }

  if (IsVarTerm(t)) {
    /* Create an anonymous array */
    Functor farray;

    farray = Yap_MkFunctor(AtomArray, size);
    if (H+1+size > ASP-1024) {
      if (!Yap_gcl((1+size)*sizeof(CELL), 2, ENV, gc_P(P,CP))) {
	Yap_Error(OUT_OF_STACK_ERROR,TermNil,LOCAL_ErrorMessage);
	return(FALSE);
      } else {
	if (H+1+size > ASP-1024) {
	  if (!Yap_growstack( sizeof(CELL) * (size+1-(H-ASP-1024)))) {
	    Yap_Error(OUT_OF_HEAP_ERROR, TermNil, LOCAL_ErrorMessage);
	    return FALSE;
	  }
	}
      }
      goto restart;
    }
    t = AbsAppl(H);
    *H++ = (CELL) farray;
    for (; size >= 0; size--) {
      RESET_VARIABLE(H);
      H++;
    }
    return (Yap_unify(t, ARG1));
  }
  else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    PropEntry *pp;

    WRITE_LOCK(ae->ARWLock);
    pp = RepProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) &&
	   pp->KindOfPE != ArrayProperty
#if THREADS
	   && ((ArrayEntry *)pp)->owner_id != worker_id
#endif	   
	   )
      pp = RepProp(pp->NextOfPE);
    if (EndOfPAEntr(pp)) {
      if (H+1+size > ASP-1024) {
	WRITE_UNLOCK(ae->ARWLock);
	if (!Yap_gcl((1+size)*sizeof(CELL), 2, ENV, gc_P(P,CP))) {
	  Yap_Error(OUT_OF_STACK_ERROR,TermNil,LOCAL_ErrorMessage);
	  return(FALSE);
	} else
	  goto restart;
      }
      CreateNamedArray(pp, size, ae PASS_REGS);
      WRITE_UNLOCK(ae->ARWLock);
      return (TRUE);
    } else {
      ArrayEntry *app = (ArrayEntry *) pp;

      WRITE_UNLOCK(ae->ARWLock);
      if (!IsVarTerm(app->ValueOfVE)
	  || !IsUnboundVar(&app->ValueOfVE)) {
	if (size == app->ArrayEArity ||
	    size == -app->ArrayEArity)
	  return TRUE;
	Yap_Error(PERMISSION_ERROR_CREATE_ARRAY,t,"create_array",
	      ae->StrOfAE);
      } else {
	if (H+1+size > ASP-1024) {
	  if (!Yap_gcl((1+size)*sizeof(CELL), 2, ENV, gc_P(P,CP))) {
	    Yap_Error(OUT_OF_STACK_ERROR,TermNil,LOCAL_ErrorMessage);
	    return(FALSE);
	  } else
	    goto restart;
	}
	InitNamedArray(app, size PASS_REGS);
	return (TRUE);
      }
    }
  }
  return (FALSE);
}

/* create an array (+Name, + Size, +Props) */
static Int 
p_create_static_array( USES_REGS1 )
{
  Term ti = Deref(ARG2);
  Term t = Deref(ARG1);
  Term tprops = Deref(ARG3);
  Int size;
  static_array_types props;

  if (IsVarTerm(ti)) {
    Yap_Error(INSTANTIATION_ERROR,ti,"create static array");
    return (FALSE);
  } else {
    Term nti;

    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      size = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"create static array");
      return (FALSE);
    }
  }

  if (IsVarTerm(tprops)) {
    Yap_Error(INSTANTIATION_ERROR,tprops,"create static array");
    return (FALSE);
  } else if (IsAtomTerm(tprops)) {
    char *atname = RepAtom(AtomOfTerm(tprops))->StrOfAE;
    if (!strcmp(atname, "int"))
      props = array_of_ints;
    else if (!strcmp(atname, "dbref"))
      props = array_of_dbrefs;
    else if (!strcmp(atname, "float"))
      props = array_of_doubles;
    else if (!strcmp(atname, "ptr"))
      props = array_of_ptrs;
    else if (!strcmp(atname, "atom"))
      props = array_of_atoms;
    else if (!strcmp(atname, "char"))
      props = array_of_chars;
    else if (!strcmp(atname, "unsigned_char"))
      props = array_of_uchars;
    else if (!strcmp(atname, "term"))
      props = array_of_terms;
    else if (!strcmp(atname, "nb_term"))
      props = array_of_nb_terms;
    else {
      Yap_Error(DOMAIN_ERROR_ARRAY_TYPE,tprops,"create static array");
      return(FALSE);
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,tprops,"create static array");
    return (FALSE);
  }

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"create static array");
    return (FALSE);
  }
  else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    StaticArrayEntry *pp;
    ArrayEntry *app;

    WRITE_LOCK(ae->ARWLock);
    pp = RepStaticArrayProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);

    app = (ArrayEntry *) pp;
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      pp = CreateStaticArray(ae, size, props, NULL, pp PASS_REGS);
      if (pp == NULL || pp->ValueOfVE.ints == NULL) {
	WRITE_UNLOCK(ae->ARWLock);
	return FALSE;
      }
      WRITE_UNLOCK(ae->ARWLock);
      return TRUE;
    } else if (ArrayIsDynamic(app)) {
      if (IsVarTerm(app->ValueOfVE) && IsUnboundVar(&app->ValueOfVE)) {
	pp = CreateStaticArray(ae, size, props, NULL, pp PASS_REGS);
	WRITE_UNLOCK(ae->ARWLock);
	if (pp == NULL) {
	  return FALSE;
	}
	return TRUE;
      } else {
	WRITE_UNLOCK(ae->ARWLock);
	Yap_Error(PERMISSION_ERROR_CREATE_ARRAY,t,"cannot create static array over dynamic array");
	return FALSE;
      }
    } else {
      if (pp->ArrayEArity  == -size &&
	  pp->ArrayType == props) {
	WRITE_UNLOCK(ae->ARWLock);
	return TRUE;
      }
      WRITE_UNLOCK(ae->ARWLock);
      Yap_Error(PERMISSION_ERROR_CREATE_ARRAY,t,"cannot create static array over static array");
      return FALSE;
    }
  }
  Yap_Error(TYPE_ERROR_ATOM,t,"create static array");
  return FALSE;
}

/* has a static array associated (+Name) */
static Int 
p_static_array_properties( USES_REGS1 )
{
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    return (FALSE);
  }
  else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    StaticArrayEntry *pp;

    READ_LOCK(ae->ARWLock);
    pp = RepStaticArrayProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      READ_UNLOCK(ae->ARWLock);
      return (FALSE);
    } else {
      static_array_types tp = pp->ArrayType;
      Int dim = -pp->ArrayEArity;

      READ_UNLOCK(ae->ARWLock);
      if (dim <= 0 || !Yap_unify(ARG2,MkIntegerTerm(dim)))
	return(FALSE);
      switch(tp) {
      case array_of_ints:
	return(Yap_unify(ARG3,MkAtomTerm(AtomInt)));
      case array_of_dbrefs:
	return(Yap_unify(ARG3,MkAtomTerm(AtomDBref)));
      case array_of_doubles:
	return(Yap_unify(ARG3,MkAtomTerm(AtomFloat)));
      case array_of_ptrs:
	return(Yap_unify(ARG3,MkAtomTerm(AtomPtr)));
      case array_of_chars:
	return(Yap_unify(ARG3,MkAtomTerm(AtomChar)));
      case array_of_uchars:
	return(Yap_unify(ARG3,MkAtomTerm(AtomUnsignedChar)));
      case array_of_terms:
	return(Yap_unify(ARG3,MkAtomTerm(AtomTerm)));
      case array_of_nb_terms:
	return(Yap_unify(ARG3,MkAtomTerm(AtomNbTerm)));
      case array_of_atoms:
	return(Yap_unify(ARG3,MkAtomTerm(AtomAtom)));
      }
    }
  }
  return (FALSE);
}

/* resize a static array (+Name, + Size, +Props) */
/* does not work for mmap arrays yet */
static Int 
p_resize_static_array( USES_REGS1 )
{
  Term ti = Deref(ARG3);
  Term t = Deref(ARG1);
  Int size;

  if (IsVarTerm(ti)) {
    Yap_Error(INSTANTIATION_ERROR,ti,"resize a static array");
    return (FALSE);
  } else {
    Term nti;

    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      size = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"resize a static array");
      return (FALSE);
    }
  }

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"resize a static array");
    return (FALSE);
  }
  else if (IsAtomTerm(t)) {
    /* resize a named array */
    Atom a = AtomOfTerm(t);
    StaticArrayEntry *pp = RepStaticArrayProp(RepAtom(a)->PropsOfAE);

    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      Yap_Error(PERMISSION_ERROR_RESIZE_ARRAY,t,"resize a static array");
      return(FALSE);
    } else {
      Int osize =  - pp->ArrayEArity;
      ResizeStaticArray(pp, size PASS_REGS);
      return(Yap_unify(ARG2,MkIntegerTerm(osize)));
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,t,"resize a static array");
    return (FALSE);
  }
}

/* resize a static array (+Name, + Size, +Props) */
/* does not work for mmap arrays yet */
static Int 
p_clear_static_array( USES_REGS1 )
{
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"clear a static array");
    return FALSE;
  }
  else if (IsAtomTerm(t)) {
    /* resize a named array */
    Atom a = AtomOfTerm(t);
    StaticArrayEntry *pp = RepStaticArrayProp(RepAtom(a)->PropsOfAE);

    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      Yap_Error(PERMISSION_ERROR_RESIZE_ARRAY,t,"clear a static array");
      return FALSE;
    } else {
      ClearStaticArray(pp);
      return TRUE;
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,t,"clear a static array");
    return FALSE;
  }
}

/* Close a named array (+Name) */
static Int 
p_close_static_array( USES_REGS1 )
{
/* does not work for mmap arrays yet */
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"close static array");
    return (FALSE);
  }
  else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    PropEntry *pp;

    READ_LOCK(ae->ARWLock);
    pp = RepProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepProp(pp->NextOfPE);
    READ_UNLOCK(ae->ARWLock);
    if (EndOfPAEntr(pp)) {
      return (FALSE);
    } else {
      StaticArrayEntry *ptr = (StaticArrayEntry *)pp;
      if (ptr->ValueOfVE.ints != NULL) {
#if HAVE_MMAP
	if (ptr->ValueOfVE.chars < (char *)Yap_HeapBase || 
	    ptr->ValueOfVE.chars > (char *)HeapTop) {
	  Int val = CloseMmappedArray(ptr, (void *)ptr->ValueOfVE.chars PASS_REGS);
#if USE_SYSTEM_MALLOC
	  if (val)
#endif
	    return(val);
	}
#endif
	Yap_FreeAtomSpace((char *)(ptr->ValueOfVE.ints));
	ptr->ValueOfVE.ints = NULL;
	ptr->ArrayEArity = 0;
	return(TRUE);
      } else {
	return(FALSE);
      }
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,t,"close static array");
    return (FALSE);
  }
}

/* create an array (+Name, + Size, +Props) */
static Int 
p_create_mmapped_array( USES_REGS1 )
{
#ifdef HAVE_MMAP
  Term ti = Deref(ARG2);
  Term t = Deref(ARG1);
  Term tprops = Deref(ARG3);
  Term tfile = Deref(ARG4);
  Int size;
  static_array_types props;
  size_t total_size;
  CODEADDR array_addr;
  int fd;

  if (IsVarTerm(ti)) {
    Yap_Error(INSTANTIATION_ERROR,ti,"create_mmapped_array");
    return (FALSE);
  } else {
    Term nti;

    if (IsIntegerTerm(nti=Yap_Eval(ti)))
      size = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,ti,"create_mmapped_array");
      return (FALSE);
    }
  }

  if (IsVarTerm(tprops)) {
    Yap_Error(INSTANTIATION_ERROR,tprops,"create_mmapped_array");
    return (FALSE);
  } else if (IsAtomTerm(tprops)) {
    char *atname = RepAtom(AtomOfTerm(tprops))->StrOfAE;
    if (!strcmp(atname, "int")) {
      props = array_of_ints;
      total_size = size*sizeof(Int);
    } else if (!strcmp(atname, "dbref")) {
      props = array_of_dbrefs;
      total_size = size*sizeof(Int);
    } else if (!strcmp(atname, "float")) {
      props = array_of_doubles;
      total_size = size*sizeof(Float);
    } else if (!strcmp(atname, "ptr")) {
      props = array_of_ptrs;
      total_size = size*sizeof(AtomEntry *);
    } else if (!strcmp(atname, "atom")) {
      props = array_of_atoms;
      total_size = size*sizeof(Term);
    } else if (!strcmp(atname, "char")) {
      props = array_of_chars;
      total_size = size*sizeof(char);
    } else if (!strcmp(atname, "unsigned_char")) {
      props = array_of_uchars;
      total_size = size*sizeof(unsigned char);
    } else {
      Yap_Error(DOMAIN_ERROR_ARRAY_TYPE,tprops,"create_mmapped_array");
      return(FALSE);
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,tprops,"create_mmapped_array");
    return (FALSE);
  }

  if (IsVarTerm(tfile)) {
    Yap_Error(INSTANTIATION_ERROR,tfile,"create_mmapped_array");
    return (FALSE);
  } else if (IsAtomTerm(tfile)) {
    char *filename = RepAtom(AtomOfTerm(tfile))->StrOfAE;
    

    fd = open(filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd == -1) {
      Yap_Error(SYSTEM_ERROR,ARG1,"create_mmapped_array (open: %s)", strerror(errno));
      return(FALSE);
    }
    if (lseek(fd, total_size-1, SEEK_SET) < 0)
      Yap_Error(SYSTEM_ERROR,tfile,"create_mmapped_array (lseek: %s)", strerror(errno));
    if (write(fd, "", 1) < 0)
      Yap_Error(SYSTEM_ERROR,tfile,"create_mmapped_array (write: %s)", strerror(errno));
    /*
      if (ftruncate(fd, total_size) < 0)
      Yap_Error(SYSTEM_ERROR,tfile,"create_mmapped_array");
    */
    if ((array_addr = (CODEADDR)mmap(0, (size_t) total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (CODEADDR) - 1)
      Yap_Error(SYSTEM_ERROR,tfile,"create_mmapped_array (mmap: %s)", strerror(errno));
  } else {
    Yap_Error(TYPE_ERROR_ATOM,tfile,"create_mmapped_array");
    return (FALSE);
  }

  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"create_mmapped_array");
    return (FALSE);
  }
  else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    StaticArrayEntry *pp;

    WRITE_LOCK(ae->ARWLock);
    pp = RepStaticArrayProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      mmap_array_block *ptr;

      if (EndOfPAEntr(pp)) {
	WRITE_UNLOCK(ae->ARWLock);
	return FALSE;	
      } else {
	WRITE_LOCK(pp->ArRWLock);
      }
      CreateStaticArray(ae, size, props, array_addr, pp PASS_REGS);
      ptr = (mmap_array_block *)Yap_AllocAtomSpace(sizeof(mmap_array_block));
      ptr->name = AbsAtom(ae);
      ptr->size = total_size;
      ptr->items = size;
      ptr->start = (void *)array_addr;
      ptr->fd = fd;
      ptr->next = GLOBAL_mmap_arrays;
      GLOBAL_mmap_arrays = ptr;
      WRITE_UNLOCK(pp->ArRWLock);
      WRITE_UNLOCK(ae->ARWLock);
      return TRUE;
    } else {
      WRITE_UNLOCK(ae->ARWLock);
      Yap_Error(DOMAIN_ERROR_ARRAY_TYPE,t,"create_mmapped_array", ae->StrOfAE);
      return(FALSE);
    }
  } else {
    Yap_Error(TYPE_ERROR_ATOM,t,"create_mmapped_array");
    return FALSE;
  }
#else
  Yap_Error(SYSTEM_ERROR,ARG1,"create_mmapped_array (mmap)");
  return (FALSE);
#endif
}

/* This routine removes array references from complex terms? */
static void 
replace_array_references_complex(register CELL *pt0,
				 register CELL *pt0_end,
				 register CELL *ptn,
				 Term Var USES_REGS)
{

  register CELL **to_visit = (CELL **) Yap_PreAllocCodeSpace();
  CELL **to_visit_base = to_visit;

loop:
  while (pt0 < pt0_end) {
    register CELL d0;

    ++pt0;
    d0 = Derefa(pt0);
    if (IsVarTerm(d0)) {
      *ptn++ = d0;
    }
    else if (IsPairTerm(d0)) {
      /* store the terms to visit */
      *ptn++ = AbsPair(H);
#ifdef RATIONAL_TREES
      to_visit[0] = pt0;
      to_visit[1] = pt0_end;
      to_visit[2] = ptn;
      to_visit[3] = (CELL *)*pt0;
      to_visit += 4;
      *pt0 = TermNil;
#else
      if (pt0 < pt0_end) {
	to_visit[0] = pt0;
	to_visit[1] = pt0_end;
	to_visit[2] = ptn;
	to_visit += 3;
      }
#endif
      pt0 = RepPair(d0) - 1;
      pt0_end = RepPair(d0) + 1;
      /* write the head and tail of the list */
      ptn = H;
      H += 2;
    }
    else if (IsApplTerm(d0)) {
      register Functor f;

      f = FunctorOfTerm(d0);
      /* store the terms to visit */
      if (IsExtensionFunctor(f)) {
	{
	  *ptn++ = d0;
	  continue;
	}
      }
      *ptn++ = AbsAppl(H);
      /* store the terms to visit */
#ifdef RATIONAL_TREES
      to_visit[0] = pt0;
      to_visit[1] = pt0_end;
      to_visit[2] = ptn;
      to_visit[3] = (CELL *)*pt0;
      to_visit += 4;
      *pt0 = TermNil;
#else
      if (pt0 < pt0_end) {
	to_visit[0] = pt0;
	to_visit[1] = pt0_end;
	to_visit[2] = ptn;
	to_visit += 3;
      }
#endif
      pt0 = RepAppl(d0);
      d0 = ArityOfFunctor(f);
      pt0_end = pt0 + d0;
      /* start writing the compound term */
      ptn = H;
      *ptn++ = (CELL) f;
      H += d0 + 1;
    }
    else {			/* AtomOrInt */
      *ptn++ = d0;
    }
    /* just continue the loop */
  }

  /* Do we still have compound terms to visit */
  if (to_visit > (CELL **) to_visit_base) {
#ifdef RATIONAL_TREES
    to_visit -= 4;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptn = to_visit[2];
    *pt0 = (CELL)to_visit[3];
#else
    to_visit -= 3;
    pt0 = to_visit[0];
    pt0_end = to_visit[1];
    ptn = to_visit[2];
#endif
    goto loop;
  }

  Bind_Global(PtrOfTerm(Var), TermNil);
  Yap_ReleasePreAllocCodeSpace((ADDR)to_visit);
}

/*
 * 
 * Given a term t0, build a new term tf of the form ta+tb, where ta is
 * obtained by replacing the array references in t0 by empty
 * variables, and tb is a list of array references and corresponding
 * variables.
 */
static Term 
replace_array_references(Term t0 USES_REGS)
{
  Term t;

  t = Deref(t0);
  if (IsVarTerm(t)) {
    /* we found a variable */
    return (MkPairTerm(t, TermNil));
  } else if (IsAtomOrIntTerm(t)) {
    return (MkPairTerm(t, TermNil));
  } else if (IsPairTerm(t)) {
    Term VList = MkVarTerm();
    CELL *h0 = H;

    H += 2;
    replace_array_references_complex(RepPair(t) - 1, RepPair(t) + 1, h0,
				     VList PASS_REGS);
    return MkPairTerm(AbsPair(h0), VList);
  } else {
    Term VList = MkVarTerm();
    CELL *h0 = H;
    Functor f = FunctorOfTerm(t);

    *H++ = (CELL) (f);
    H += ArityOfFunctor(f);
    replace_array_references_complex(RepAppl(t),
				     RepAppl(t) + ArityOfFunctor(FunctorOfTerm(t)), h0 + 1,
				     VList PASS_REGS);
    return (MkPairTerm(AbsAppl(h0), VList));
  }
}

static Int 
p_array_references( USES_REGS1 )
{
  Term t = replace_array_references(ARG1 PASS_REGS);
  Term t1 = HeadOfTerm(t);
  Term t2 = TailOfTerm(t);

  return (Yap_unify(t1, ARG2) && Yap_unify(t2, ARG3));
}

static Int 
p_assign_static( USES_REGS1 )
{
  Term t1, t2, t3;
  StaticArrayEntry *ptr;
  Int indx;

  t2 = Deref(ARG2);
  if (IsNonVarTerm(t2)) {
    Term nti;

    if (IsIntegerTerm(nti=Yap_Eval(t2)))
      indx = IntegerOfTerm(nti);
    else {
      Yap_Error(TYPE_ERROR_INTEGER,t2,"update_array");
      return (FALSE);
    }
  } else {
    Yap_Error(INSTANTIATION_ERROR,t2,"update_array");
    return (FALSE);
  }
  t3 = Deref(ARG3);

  t1 = Deref(ARG1);
  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR,t1,"update_array");
    return(FALSE);
  }
  if (!IsAtomTerm(t1)) {
    if (IsApplTerm(t1)) {
      CELL *ptr;
      Functor f = FunctorOfTerm(t1);
      /* store the terms to visit */
      if (IsExtensionFunctor(f)) {
	Yap_Error(TYPE_ERROR_ARRAY,t1,"update_array");
	return(FALSE);
      }
      if (indx > 0 && indx > ArityOfFunctor(f)) {
	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"update_array");
	return(FALSE);
      }
      ptr = RepAppl(t1)+indx+1;
#ifdef MULTI_ASSIGNMENT_VARIABLES
      MaBind(ptr, t3);
      return(TRUE);
#else
      Yap_Error(SYSTEM_ERROR,t2,"update_array");
      return(FALSE);
#endif
    } else {
      Yap_Error(TYPE_ERROR_ATOM,t1,"update_array");
      return(FALSE);
    }
  }
  {
    AtomEntry *ae = RepAtom(AtomOfTerm(t1));

    READ_LOCK(ae->ARWLock);
    ptr =  RepStaticArrayProp(ae->PropsOfAE);    
    while (!EndOfPAEntr(ptr) && ptr->KindOfPE != ArrayProperty)
      ptr = RepStaticArrayProp(ptr->NextOfPE);

    if (EndOfPAEntr(ptr)) {
      READ_UNLOCK(ae->ARWLock);
      Yap_Error(EXISTENCE_ERROR_ARRAY,t1,"assign_static %s", RepAtom(AtomOfTerm(t1))->StrOfAE);
      return FALSE;
    }

    if (ArrayIsDynamic((ArrayEntry *)ptr)) {
      ArrayEntry *pp = (ArrayEntry *)ptr;
      CELL *pt;
    
      WRITE_LOCK(pp->ArRWLock);
      READ_UNLOCK(ae->ARWLock);
      if (indx < 0 || indx >= pp->ArrayEArity) {
	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"assign_static");
	WRITE_UNLOCK(pp->ArRWLock);
	return FALSE;
      }
      pt = RepAppl(pp->ValueOfVE) + indx + 1;
      WRITE_UNLOCK(pp->ArRWLock);
#ifdef MULTI_ASSIGNMENT_VARIABLES
      /* the evil deed is to be done now */
      MaBind(pt, t3);
      return TRUE;
#else
      Yap_Error(SYSTEM_ERROR,t2,"update_array");
      return FALSE;
#endif
    }

    WRITE_LOCK(ptr->ArRWLock);
    READ_UNLOCK(ae->ARWLock);
    /* a static array */
    if (indx < 0 || indx >= - ptr->ArrayEArity) {
      WRITE_UNLOCK(ptr->ArRWLock);
      Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"assign_static");
      return FALSE;
    }
    switch (ptr->ArrayType) {
    case array_of_ints:
      {
	Int i;
	Term nti;
      
	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}

	if (IsIntegerTerm(nti=Yap_Eval(t3)))
	  i = IntegerOfTerm(nti);
	else {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_INTEGER,t3,"assign_static");
	  return (FALSE);
	}
	ptr->ValueOfVE.ints[indx]= i;
      }
      break;

    case array_of_chars:
      {
	Int i;
	Term nti;
      
	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (IsIntegerTerm(nti=Yap_Eval(t3)))
	  i = IntegerOfTerm(nti);
	else {
	  Yap_Error(TYPE_ERROR_INTEGER,t3,"assign_static");
	  return (FALSE);
	}
	if (i > 127 || i < -128) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_CHAR,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.chars[indx]= i;
      }
      break;

    case array_of_uchars:
      {
	Int i;
	Term nti;
      
	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (IsIntegerTerm(nti=Yap_Eval(t3)))
	  i = IntegerOfTerm(nti);
	else {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_INTEGER,t3,"assign_static");
	  return FALSE;
	}
	if (i > 255 || i < 0) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_UCHAR,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.chars[indx]= i;
      }
      break;

    case array_of_doubles:
      {
	Float f;
	Term nti;

	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (IsFloatTerm(nti=Yap_Eval(t3)))
	  f = FloatOfTerm(nti);
	else if (IsIntegerTerm(nti))
	  f = IntegerOfTerm(nti);
	else {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_FLOAT,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.floats[indx]= f;
      }
      break;

    case array_of_ptrs:
      {
	Int r;

	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (IsIntegerTerm(t3))
	  r = IntegerOfTerm(t3);
	else {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_PTR,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.ptrs[indx]= (AtomEntry *)r;
      }
      break;

    case array_of_atoms:
      {
	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (!IsAtomTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_ATOM,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.atoms[indx]= t3;
      }
      break;

    case array_of_dbrefs:
      {
      
	Term t0 = ptr->ValueOfVE.dbrefs[indx];
	DBRef p = DBRefOfTerm(t3);
      
	if (IsVarTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(INSTANTIATION_ERROR,t3,"assign_static");
	  return FALSE;
	}
	if (!IsDBRefTerm(t3)) {
	  WRITE_UNLOCK(ptr->ArRWLock);
	  Yap_Error(TYPE_ERROR_DBREF,t3,"assign_static");
	  return FALSE;
	}
	ptr->ValueOfVE.dbrefs[indx]= t3;
	if (t0 != 0L) {
	  DBRef ptr = DBRefOfTerm(t0);

	  if (ptr->Flags & LogUpdMask) {
	    LogUpdClause *lup = (LogUpdClause *)ptr;
	    //	    LOCK(lup->ClLock);
	    lup->ClRefCount--;
	    if (lup->ClRefCount == 0 &&
		(lup->ClFlags & ErasedMask) &&
		!(lup->ClFlags & InUseMask)) {
	      //	      UNLOCK(lup->ClLock);
	      Yap_ErLogUpdCl(lup);
	    } else {
	      //	      UNLOCK(lup->ClLock);
	    }
	  } else {
	    ptr->NOfRefsTo--;
	    if (ptr->NOfRefsTo == 0 &&
		(ptr->Flags & ErasedMask) &&
		!(ptr->Flags & InUseMask)) {
	      Yap_ErDBE(ptr);
	    }
	  }
	}
      
	if (p->Flags & LogUpdMask) {
	  LogUpdClause *lup = (LogUpdClause *)p;
	  //	  LOCK(lup->ClLock);
	  lup->ClRefCount++;
	  //	  UNLOCK(lup->ClLock);
	} else {
	  p->NOfRefsTo++;
	}
      }
      break;

    case array_of_nb_terms:

      {
	Term told = ptr->ValueOfVE.lterms[indx].tstore;

	CELL *livep = &(ptr->ValueOfVE.lterms[indx].tlive);
	RESET_VARIABLE(livep);
	/* recover space */
	if (IsApplTerm(told)) {
	  Yap_ReleaseTermFromDB((DBTerm *)RepAppl(told));
	}
	if (IsVarTerm(t3)) {
	  RESET_VARIABLE(&(ptr->ValueOfVE.lterms[indx].tstore));
	} else if (IsAtomicTerm(t3)) {
	  ptr->ValueOfVE.lterms[indx].tstore = t3;
	} else {
	  DBTerm *new = Yap_StoreTermInDB(t3,3);
	  if (!new) {
	    WRITE_UNLOCK(ptr->ArRWLock);
	    return FALSE;
	  }
	  ptr->ValueOfVE.lterms[indx].tstore = AbsAppl((CELL *)new);
	}
      }
      break;

    case array_of_terms:
      {
      
	DBTerm *ref = ptr->ValueOfVE.terms[indx];

	if (ref != NULL) {
	  Yap_ReleaseTermFromDB(ref);
	}
	ptr->ValueOfVE.terms[indx] = Yap_StoreTermInDB(t3,3);
	if (ptr->ValueOfVE.terms[indx] == NULL){
	  WRITE_UNLOCK(ptr->ArRWLock);
	  return FALSE;
	}
      }
      break;
    }
    WRITE_UNLOCK(ptr->ArRWLock);
    return TRUE;
  }
}

static Int 
p_assign_dynamic( USES_REGS1 )
{
  Term t1, t2, t3;
  StaticArrayEntry *ptr;
  Int indx;

  t2 = Deref(ARG2);
  if (IsNonVarTerm(t2)) {
    Term nti;
    if (IsIntegerTerm(nti=Yap_Eval(t2))) {
      indx = IntegerOfTerm(nti);
    } else {
      Yap_Error(TYPE_ERROR_INTEGER,t2,"update_array");
      return (FALSE);
    }
  } else {
    Yap_Error(INSTANTIATION_ERROR,t2,"update_array");
    return (FALSE);
  }
  t3 = Deref(ARG3);

  t1 = Deref(ARG1);
  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR,t1,"update_array");
    return(FALSE);
  }
  if (!IsAtomTerm(t1)) {
    if (IsApplTerm(t1)) {
      CELL *ptr;
      Functor f = FunctorOfTerm(t1);
      /* store the terms to visit */
      if (IsExtensionFunctor(f)) {
	Yap_Error(TYPE_ERROR_ARRAY,t1,"update_array");
	return(FALSE);
      }
      if (indx > 0 && indx > ArityOfFunctor(f)) {
	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"update_array");
	return(FALSE);
      }
      ptr = RepAppl(t1)+indx+1;
#ifdef MULTI_ASSIGNMENT_VARIABLES
      MaBind(ptr, t3);
      return(TRUE);
#else
      Yap_Error(SYSTEM_ERROR,t2,"update_array");
      return(FALSE);
#endif
    } else {
      Yap_Error(TYPE_ERROR_ATOM,t1,"update_array");
      return(FALSE);
    }
  }
  {
    AtomEntry *ae = RepAtom(AtomOfTerm(t1));

    READ_LOCK(ae->ARWLock);
    ptr =  RepStaticArrayProp(ae->PropsOfAE);    
    while (!EndOfPAEntr(ptr) && ptr->KindOfPE != ArrayProperty)
      ptr = RepStaticArrayProp(ptr->NextOfPE);
    READ_UNLOCK(ae->ARWLock);
  }

  if (EndOfPAEntr(ptr)) {
    Yap_Error(EXISTENCE_ERROR_ARRAY,t1,"assign_static %s", RepAtom(AtomOfTerm(t1))->StrOfAE);
    return(FALSE);
  }

  if (ArrayIsDynamic((ArrayEntry *)ptr)) {
    ArrayEntry *pp = (ArrayEntry *)ptr;
    CELL *pt;
    WRITE_LOCK(pp->ArRWLock);
    if (indx < 0 || indx >= pp->ArrayEArity) {
      Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"assign_static");
      WRITE_UNLOCK(pp->ArRWLock);
      return(FALSE);
    }
    pt = RepAppl(pp->ValueOfVE) + indx + 1;
    WRITE_UNLOCK(pp->ArRWLock);
#ifdef MULTI_ASSIGNMENT_VARIABLES
    /* the evil deed is to be done now */
    MaBind(pt, t3);
    return TRUE;
#else
    Yap_Error(SYSTEM_ERROR,t2,"update_array");
    return FALSE;
#endif
  }

  WRITE_LOCK(ptr->ArRWLock);
  /* a static array */
  if (indx < 0 || indx >= - ptr->ArrayEArity) {
    WRITE_UNLOCK(ptr->ArRWLock);
    Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"assign_static");
    return FALSE;
  }
  switch (ptr->ArrayType) {
  case array_of_ints:
  case array_of_chars:
  case array_of_uchars:
  case array_of_doubles:
  case array_of_ptrs:
  case array_of_atoms:
  case array_of_dbrefs:
  case array_of_terms:
    WRITE_UNLOCK(ptr->ArRWLock);
    Yap_Error(DOMAIN_ERROR_ARRAY_TYPE, t3, "assign_static");
    return FALSE;

  case array_of_nb_terms:
#ifdef MULTI_ASSIGNMENT_VARIABLES
    { 
      Term t = ptr->ValueOfVE.lterms[indx].tlive;
      Functor f;
      /* we have a mutable term there */

      if (IsVarTerm(t) ||
	  !IsApplTerm(t) ||
	  (f = FunctorOfTerm(t)) != FunctorAtFoundOne) {
	Term tn = Yap_NewTimedVar(t3);
	CELL *sp = RepAppl(tn);
	*sp = (CELL)FunctorAtFoundOne;
	Bind(&(ptr->ValueOfVE.lterms[indx].tlive),tn);
      } else {
	Yap_UpdateTimedVar(t, t3);
      }
    }
    WRITE_UNLOCK(ptr->ArRWLock);
    return TRUE;
#else
    WRITE_UNLOCK(ptr->ArRWLock);
    Yap_Error(SYSTEM_ERROR,t2,"update_array");
    return FALSE;
#endif

  }
  WRITE_UNLOCK(ptr->ArRWLock);
  return TRUE;
}

static Int 
p_add_to_array_element( USES_REGS1 )
{
  Term t1, t2, t3;
  StaticArrayEntry *ptr;
  Int indx;

  t2 = Deref(ARG2);
  if (IsNonVarTerm(t2)) {
    Term nti;
    if (IsIntegerTerm(nti=Yap_Eval(t2))) {
      indx = IntegerOfTerm(nti);
    } else {
      Yap_Error(TYPE_ERROR_INTEGER,t2,"add_to_array_element");
      return (FALSE);
    }
  } else {
    Yap_Error(INSTANTIATION_ERROR,t2,"add_to_array_element");
    return (FALSE);
  }

  t1 = Deref(ARG1);
  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR,t1,"add_to_array_element");
    return(FALSE);
  }
  t3 = Deref(ARG3);
  if (IsVarTerm(t3)) {
    Yap_Error(INSTANTIATION_ERROR,t3,"add_to_array_element");
    return(FALSE);
  }
  if (!IsAtomTerm(t1)) {
    if (IsApplTerm(t1)) {
      CELL *ptr;
      Functor f = FunctorOfTerm(t1);
      Term ta;

      /* store the terms to visit */
      if (IsExtensionFunctor(f)) {
	Yap_Error(TYPE_ERROR_ARRAY,t1,"add_to_array_element");
	return(FALSE);
      }
      if (indx > 0 && indx > ArityOfFunctor(f)) {
	Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"add_to_array_element");
	return(FALSE);
      }
      ptr = RepAppl(t1)+indx+1;
      ta = RepAppl(t1)[indx+1];
      if (IsIntegerTerm(ta)) {
	if (IsIntegerTerm(t3)) {
	  ta = MkIntegerTerm(IntegerOfTerm(ta)+IntegerOfTerm(t3));
	} else if (IsFloatTerm(t3)) {
	  ta = MkFloatTerm(IntegerOfTerm(ta)+FloatOfTerm(t3));
	} else {
	  Yap_Error(TYPE_ERROR_NUMBER,t3,"add_to_array_element");
	  return(FALSE);
	}
      } else if (IsFloatTerm(ta)) {
	if (IsFloatTerm(t3)) {
	  ta = MkFloatTerm(FloatOfTerm(ta)+IntegerOfTerm(t3));
	} else if (IsFloatTerm(t3)) {
	  ta = MkFloatTerm(FloatOfTerm(ta)+FloatOfTerm(t3));
	} else {
	  Yap_Error(TYPE_ERROR_NUMBER,t3,"add_to_array_element");
	  return(FALSE);
	}
      } else {
	Yap_Error(TYPE_ERROR_NUMBER,ta,"add_to_array_element");
	return(FALSE);
      }
#ifdef MULTI_ASSIGNMENT_VARIABLES
      MaBind(ptr, ta);
      return(Yap_unify(ARG4,ta));
#else
      Yap_Error(SYSTEM_ERROR,t2,"add_to_array_element");
      return(FALSE);
#endif
    } else {
      Yap_Error(TYPE_ERROR_ATOM,t1,"add_to_array_element");
      return(FALSE);
    }
  }
  {
    AtomEntry *ae = RepAtom(AtomOfTerm(t1));

    READ_LOCK(ae->ARWLock);
    ptr =  RepStaticArrayProp(ae->PropsOfAE);    
    while (!EndOfPAEntr(ptr) && ptr->KindOfPE != ArrayProperty)
      ptr = RepStaticArrayProp(ptr->NextOfPE);
    READ_UNLOCK(ae->ARWLock);
  }

  if (EndOfPAEntr(ptr)) {
    Yap_Error(EXISTENCE_ERROR_ARRAY,t1,"add_to_array_element %s", RepAtom(AtomOfTerm(t1))->StrOfAE);
    return(FALSE);
  }

  if (ArrayIsDynamic((ArrayEntry *)ptr)) {
    ArrayEntry *pp = (ArrayEntry *)ptr;
    CELL *pt;
    Term ta;

    WRITE_LOCK(pp->ArRWLock);
    if (indx < 0 || indx >= pp->ArrayEArity) {
      Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"add_to_array_element");
      READ_UNLOCK(pp->ArRWLock);
      return FALSE;
    }
    pt = RepAppl(pp->ValueOfVE) + indx + 1;
    ta = RepAppl(pp->ValueOfVE)[indx+1];
    if (IsIntegerTerm(ta)) {
      if (IsIntegerTerm(t3)) {
	ta = MkIntegerTerm(IntegerOfTerm(ta)+IntegerOfTerm(t3));
      } else if (IsFloatTerm(t3)) {
	ta = MkFloatTerm(IntegerOfTerm(ta)+FloatOfTerm(t3));
      } else {
	WRITE_UNLOCK(pp->ArRWLock);
	Yap_Error(TYPE_ERROR_NUMBER,t3,"add_to_array_element");
	return FALSE;
      }
    } else if (IsFloatTerm(ta)) {
      if (IsFloatTerm(t3)) {
	ta = MkFloatTerm(FloatOfTerm(ta)+IntegerOfTerm(t3));
      } else if (IsFloatTerm(t3)) {
	ta = MkFloatTerm(FloatOfTerm(ta)+FloatOfTerm(t3));
      } else {
	WRITE_UNLOCK(pp->ArRWLock);
	Yap_Error(TYPE_ERROR_NUMBER,t3,"add_to_array_element");
	return FALSE;
      }
    } else {
      WRITE_UNLOCK(pp->ArRWLock);
      Yap_Error(TYPE_ERROR_NUMBER,ta,"add_to_array_element");
      return FALSE;
    }
#ifdef MULTI_ASSIGNMENT_VARIABLES
    /* the evil deed is to be done now */
    t3 = MkIntegerTerm(IntegerOfTerm(t3)+1);
    MaBind(pt, t3);
    WRITE_UNLOCK(pp->ArRWLock);
    return Yap_unify(ARG4,t3);
#else
    Yap_Error(SYSTEM_ERROR,t2,"add_to_array_element");
    WRITE_UNLOCK(pp->ArRWLock);
    return FALSE;
#endif
  }

  WRITE_LOCK(ptr->ArRWLock);
  /* a static array */
  if (indx < 0 || indx >= - ptr->ArrayEArity) {
    WRITE_UNLOCK(ptr->ArRWLock);
    Yap_Error(DOMAIN_ERROR_ARRAY_OVERFLOW,t2,"add_to_array_element");
    return FALSE;
  }
  switch (ptr->ArrayType) {
  case array_of_ints:
    {
      Int i = ptr->ValueOfVE.ints[indx];
      if (!IsIntegerTerm(t3)) {
	WRITE_UNLOCK(ptr->ArRWLock);
	Yap_Error(TYPE_ERROR_INTEGER,t3,"add_to_array_element");
	return FALSE;
      }
      i += IntegerOfTerm(t3);
      ptr->ValueOfVE.ints[indx] = i;
      WRITE_UNLOCK(ptr->ArRWLock);
      return Yap_unify(ARG4,MkIntegerTerm(i));
    }
    break;
  case array_of_doubles:
    {
      Float fl = ptr->ValueOfVE.floats[indx];

      if (IsFloatTerm(t3)) {
	fl += FloatOfTerm(t3);
      } else if (IsIntegerTerm(t3)) {
	fl += IntegerOfTerm(t3);
      } else {
	WRITE_UNLOCK(ptr->ArRWLock);
	Yap_Error(TYPE_ERROR_NUMBER,t3,"add_to_array_element");
	return FALSE;
      }
      ptr->ValueOfVE.floats[indx] = fl;
      WRITE_UNLOCK(ptr->ArRWLock);
      return Yap_unify(ARG4,MkFloatTerm(fl));
    }
    break;
  default:
    WRITE_UNLOCK(ptr->ArRWLock);
    Yap_Error(TYPE_ERROR_INTEGER,t2,"add_to_array_element");
    return FALSE;
  }
}

static Int 
p_compile_array_refs( USES_REGS1 )
{
  compile_arrays = TRUE;
  return (TRUE);
}

static Int 
p_array_refs_compiled( USES_REGS1 )
{
  return compile_arrays;
}

static Int
p_sync_mmapped_arrays( USES_REGS1 )
{
#ifdef HAVE_MMAP
  mmap_array_block *ptr = GLOBAL_mmap_arrays;
  while (ptr != NULL) {
    msync(ptr->start, ptr->size, MS_SYNC);
    ptr = ptr->next;
  }
#endif
  return(TRUE);
}

static Int
p_static_array_to_term( USES_REGS1 )
{
  Term t = Deref(ARG1);

  if (IsVarTerm(t)) {
    return FALSE;
  } else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    StaticArrayEntry *pp;

    READ_LOCK(ae->ARWLock);
    pp = RepStaticArrayProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      READ_UNLOCK(ae->ARWLock);
      return (FALSE);
    } else {
      static_array_types tp = pp->ArrayType;
      Int dim = -pp->ArrayEArity, indx;
      CELL *base;

      while (H+1+dim > ASP-1024) {
	if (!Yap_gcl((1+dim)*sizeof(CELL), 2, ENV, gc_P(P,CP))) {
	  Yap_Error(OUT_OF_STACK_ERROR,TermNil,LOCAL_ErrorMessage);
	  return(FALSE);
	} else {
	  if (H+1+dim > ASP-1024) {
	    if (!Yap_growstack( sizeof(CELL) * (dim+1-(H-ASP-1024)))) {
	      Yap_Error(OUT_OF_STACK_ERROR, TermNil, LOCAL_ErrorMessage);
	      return FALSE;
	    }
	  }
	}
      }
      READ_LOCK(pp->ArRWLock);
      READ_UNLOCK(ae->ARWLock);
      base = H;
      *H++ = (CELL)Yap_MkFunctor(AbsAtom(ae),dim);
      switch(tp) {
      case array_of_ints:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    *sptr++ = MkIntegerTerm(pp->ValueOfVE.ints[indx]);
	  }
	}
	break;
      case array_of_dbrefs:
	for (indx=0; indx < dim; indx++) {
	  /* The object is now in use */
	  Term TRef = pp->ValueOfVE.dbrefs[indx];

	  if (TRef != 0L) {
	    DBRef ref = DBRefOfTerm(TRef);
	    LOCK(ref->lock);
#if MULTIPLE_STACKS
	    INC_DBREF_COUNT(ref);
	    TRAIL_REF(ref);	/* So that fail will erase it */
#else
	    if (!(ref->Flags & InUseMask)) {
	      ref->Flags |= InUseMask;
	      TRAIL_REF(ref);	/* So that fail will erase it */
	    }
#endif
	    UNLOCK(ref->lock);
	  } else {
	    TRef = TermNil;
	  }
	  *H++ = TRef;
	}
	break;
      case array_of_doubles:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    *sptr++ = MkEvalFl(pp->ValueOfVE.floats[indx]);
	  }
	}
	break;
      case array_of_ptrs:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    *sptr++ = MkIntegerTerm((Int)(pp->ValueOfVE.ptrs[indx]));
	  }
	}
	break;
      case array_of_chars:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    *sptr++ = MkIntegerTerm((Int)(pp->ValueOfVE.chars[indx]));
	  }
	}
	break;
      case array_of_uchars:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    *sptr++ = MkIntegerTerm((Int)(pp->ValueOfVE.uchars[indx]));
	  }
	}
	break;
      case array_of_terms:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    /* The object is now in use */
	    DBTerm *ref = pp->ValueOfVE.terms[indx];

	    Term TRef = GetTermFromArray(ref PASS_REGS);

	    if (P == FAILCODE) {
	      return FALSE;
	    }

	    *sptr++ = TRef;
	  }
	}
	break;
      case array_of_nb_terms:
	{
	  CELL *sptr = H;
	  H += dim;
	  for (indx=0; indx < dim; indx++) {
	    /* The object is now in use */
	    Term To = GetNBTerm(pp->ValueOfVE.lterms, indx PASS_REGS);

	    if (P == FAILCODE) {
	      return FALSE;
	    }

	    *sptr++ = To;
	  }
	}
	break;
      case array_of_atoms:
	for (indx=0; indx < dim; indx++) {
	  Term out;
	  out = pp->ValueOfVE.atoms[indx];
	  if (out == 0L)
	    out = TermNil;
	  *H++ = out;
	}
	break;
      }
      READ_UNLOCK(pp->ArRWLock);
      return Yap_unify(AbsAppl(base),ARG2);
    }
  }
  Yap_Error(TYPE_ERROR_ATOM,t,"add_to_array_element");
  return FALSE;
}

static Int
p_static_array_location( USES_REGS1 )
{
  Term t = Deref(ARG1);
  Int *ptr;

  if (IsVarTerm(t)) {
    return FALSE;
  } else if (IsAtomTerm(t)) {
    /* Create a named array */
    AtomEntry *ae = RepAtom(AtomOfTerm(t));
    StaticArrayEntry *pp;

    READ_LOCK(ae->ARWLock);
    pp = RepStaticArrayProp(ae->PropsOfAE);
    while (!EndOfPAEntr(pp) && pp->KindOfPE != ArrayProperty)
      pp = RepStaticArrayProp(pp->NextOfPE);
    if (EndOfPAEntr(pp) || pp->ValueOfVE.ints == NULL) {
      READ_UNLOCK(ae->ARWLock);
      return FALSE;
    } else {
      ptr =  pp->ValueOfVE.ints;
      READ_UNLOCK(ae->ARWLock);
    }
    return Yap_unify(ARG2,MkIntegerTerm((Int)ptr));
  }
  return FALSE;
}

void 
Yap_InitArrayPreds( void )
{
  Yap_InitCPred("$create_array", 2, p_create_array, SyncPredFlag);
  Yap_InitCPred("$array_references", 3, p_array_references, SafePredFlag);
  Yap_InitCPred("$array_arg", 3, p_array_arg, SafePredFlag);
  Yap_InitCPred("static_array", 3, p_create_static_array, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("resize_static_array", 3, p_resize_static_array, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("mmapped_array", 4, p_create_mmapped_array, SafePredFlag|SyncPredFlag);
  Yap_InitCPred("update_array", 3, p_assign_static, SafePredFlag);
  Yap_InitCPred("dynamic_update_array", 3, p_assign_dynamic, SafePredFlag);
  Yap_InitCPred("add_to_array_element", 4, p_add_to_array_element, SafePredFlag);
  Yap_InitCPred("array_element", 3, p_access_array, 0);
  Yap_InitCPred("reset_static_array", 1, p_clear_static_array, SafePredFlag);
  Yap_InitCPred("close_static_array", 1, p_close_static_array, SafePredFlag);
  Yap_InitCPred("$sync_mmapped_arrays", 0, p_sync_mmapped_arrays, SafePredFlag);
  Yap_InitCPred("$compile_array_refs", 0, p_compile_array_refs, SafePredFlag);
  Yap_InitCPred("$array_refs_compiled", 0, p_array_refs_compiled, SafePredFlag);
  Yap_InitCPred("$static_array_properties", 3, p_static_array_properties, SafePredFlag);
  Yap_InitCPred("static_array_to_term", 2, p_static_array_to_term, 0L);
  Yap_InitCPred("static_array_location", 2, p_static_array_location, 0L);
}

