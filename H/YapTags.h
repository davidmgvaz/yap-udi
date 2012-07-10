/*************************************************************************
*									 *
*	 YAP Prolog 	%W% %G% 					 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-1997	 *
*									 *
**************************************************************************
*									 *
* File:		YapTags.h						 *
* mods:									 *
* comments:	Term Operations for YAP					 *
* version:      $Id: Yap.h,v 1.38 2008-06-18 10:02:27 vsc Exp $	 *
*************************************************************************/

#ifndef EXTERN
#define EXTERN extern
#endif

#include "inline-only.h"

#ifndef SHORT_ADDRESSES
#	define LONG_ADDRESSES	1
#else
#	define LONG_ADDRESSES	0
#endif

/***********************************************************************/

     /*
        absrectype Term = Int + Float + Atom + Pair + Appl + Ref + Var

        with AbsAppl(t) : *CELL -> Term
        and  RepAppl(t) : Term -> *CELL

        and  AbsPair(t) : *CELL -> Term
        and  RepPair(t) : Term -> *CELL

        and  IsIntTerm(t) = ...
        and  IsAtomTerm(t) = ...
        and  IsVarTerm(t) = ...
        and  IsPairTerm(t) = ...
        and  IsApplTerm(t) = ...
        and  IsFloatTerm(t) = ...
        and  IsRefTerm(t) = ...
        and  IsNonVarTerm(t) = ! IsVar(t)
        and  IsNumterm(t) = IsIntTerm(t) || IsFloatTerm(t)
        and  IsAtomicTerm(t) = IsNumTerm(t) || IsAtomTerm(t)
        and  IsPrimitiveTerm(t) = IsAtomicTerm(t) || IsRefTerm(t)

        and  MkIntTerm(n) = ...
        and  MkFloatTerm(f) = ...
        and  MkAtomTerm(a) = ...
        and  MkVarTerm(r) = ...
        and  MkApplTerm(f,n,args) = ...
        and  MkPairTerm(hd,tl) = ...
        and  MkRefTerm(R) = ...

        and  PtrOfTerm(t) : Term -> CELL * = ...
        and  IntOfTerm(t) : Term -> int = ...
        and  FloatOfTerm(t) : Term -> flt = ...
        and  AtomOfTerm(t) : Term -> Atom = ...
        and  VarOfTerm(t) : Term -> *Term = ....
        and  HeadOfTerm(t) : Term -> Term = ...
        and  TailOfTerm(t) : Term -> Term = ...
        and  FunctorOfTerm(t) : Term -> Functor = ...
        and  ArgOfTerm(i,t)  : Term -> Term= ...
        and  RefOfTerm(t) : Term -> DBRef = ...

      */

/* 
   YAP can use several different tag schemes, according to the kind of
   machine we are experimenting with.
*/

#if LONG_ADDRESSES && defined(OLD_TAG_SCHEME)

#include  "Tags_32bits.h"

#endif /* LONG_ADDRESSES && defined(OLD_TAG_SCHEME) */

/* AIX will by default place mmaped segments at 0x30000000. This is
	incompatible with the high tag scheme. Linux-ELF also does not like
	if you place things in the lower addresses (power to the libc people).
*/

#if defined(__APPLE__)
/* mmap on __APPLE__ is not the greatest idea. It overwrites memory allocated by malloc */
#undef USE_DL_MALLOC
#ifndef USE_SYSTEM_MALLOC
#define USE_SYSTEM_MALLOC 1
#endif
#elif (defined(_AIX) || (defined(__APPLE__) && !defined(__LP64__)) || defined(_WIN32) || defined(sparc) || defined(__sparc) || defined(mips) || defined(__FreeBSD__) || defined(_POWER) || defined(__POWERPC__) || defined(__linux__) || defined(IN_SECOND_QUADRANT) || defined(__CYGWIN__)) || defined(__NetBSD__) || defined(__DragonFly__)
#define USE_LOW32_TAGS 1
#endif

#if LONG_ADDRESSES && SIZEOF_INT_P==4 && !defined(OLD_TAG_SCHEME) && !defined(USE_LOW32_TAGS)

#include  "Tags_32Ops.h"

#endif /* LONG_ADDRESSES && !defined(OLD_TAG_SCHEME) && !defined(USE_LOW32_TAGS) */

#if LONG_ADDRESSES && SIZEOF_INT_P==4 && !defined(OLD_TAG_SCHEME) && defined(USE_LOW32_TAGS)

#include  "Tags_32LowTag.h"

#endif /* LONG_ADDRESSES && !defined(OLD_TAG_SCHEME) */

#if LONG_ADDRESSES && SIZEOF_INT_P==8 && !defined(OLD_TAG_SCHEME)

#include  "Tags_64bits.h"

#endif /* LONG_ADDRESSES && SIZEOF_INT_P==8 && !defined(OLD_TAG_SCHEME) */

#if !LONG_ADDRESSES

#include  "Tags_24bits.h"

#endif /* !LONG_ADDRESSES */

#ifdef TAG_LOW_BITS_32

#if !GC_NO_TAGS
#define MBIT     0x80000000
#define RBIT     0x40000000

#if IN_SECOND_QUADRANT
#define INVERT_RBIT 1		/* RBIT is 1 by default */
#endif
#endif /* !GC_NO_TAGS  */

#else

#if !GC_NO_TAGS
#if defined(YAPOR_SBA) && defined(__linux__)
#define MBIT     /* 0x20000000 */ MKTAG(0x1,0)	/* mark bit */
#else
#define RBIT     /* 0x20000000 */ MKTAG(0x1,0)	/* relocation chain bit */
#define MBIT     /* 0x40000000 */ MKTAG(0x2,0)	/* mark bit */
#endif
#endif /* !GC_NO_TAGS */

#endif

/*************************************************************************************************
                                              ???
*************************************************************************************************/

#define MkVarTerm() MkVarTerm__( PASS_REGS1 )
#define MkPairTerm(A,B) MkPairTerm__( A, B PASS_REGS )

/*************************************************************************************************
                                   applies to unbound variables
*************************************************************************************************/

INLINE_ONLY inline EXTERN Term *VarOfTerm (Term t);

INLINE_ONLY inline EXTERN Term *
VarOfTerm (Term t)
{
  return (Term *) (t);
}


#ifdef YAPOR_SBA

INLINE_ONLY inline EXTERN Term MkVarTerm__ ( USES_REGS1 );

INLINE_ONLY inline EXTERN Term
MkVarTerm__ ( USES_REGS1 )
{
  return (Term) ((*H = 0, H++));
}



INLINE_ONLY inline EXTERN int IsUnboundVar (Term *);

INLINE_ONLY inline EXTERN int
IsUnboundVar (Term * t)
{
  return (int) (*(t) == 0);
}


#else

INLINE_ONLY inline EXTERN Term MkVarTerm__ ( USES_REGS1 );

INLINE_ONLY inline EXTERN Term
MkVarTerm__ ( USES_REGS1 )
{
  return (Term) ((*H = (CELL) H, H++));
}


INLINE_ONLY inline EXTERN int IsUnboundVar (Term *);

INLINE_ONLY inline EXTERN int
IsUnboundVar (Term * t)
{
  return (int) (*(t) == (Term) (t));
}


#endif

INLINE_ONLY inline EXTERN CELL *PtrOfTerm (Term);

INLINE_ONLY inline EXTERN CELL *
PtrOfTerm (Term t)
{
  return (CELL *) (*(CELL *) (t));
}




INLINE_ONLY inline EXTERN Functor FunctorOfTerm (Term);

INLINE_ONLY inline EXTERN Functor
FunctorOfTerm (Term t)
{
  return (Functor) (*RepAppl (t));
}


#if USE_LOW32_TAGS

INLINE_ONLY inline EXTERN Term MkAtomTerm (Atom);

INLINE_ONLY inline EXTERN Term
MkAtomTerm (Atom a)
{
  return (Term) (AtomTag | (CELL) (a));
}



INLINE_ONLY inline EXTERN Atom AtomOfTerm (Term t);

INLINE_ONLY inline EXTERN Atom
AtomOfTerm (Term t)
{
  return (Atom) ((~AtomTag & (CELL) (t)));
}


#else

INLINE_ONLY inline EXTERN Term MkAtomTerm (Atom);

INLINE_ONLY inline EXTERN Term
MkAtomTerm (Atom a)
{
  return (Term) (TAGGEDA ((CELL)AtomTag, (CELL) (a)));
}



INLINE_ONLY inline EXTERN Atom AtomOfTerm (Term t);

INLINE_ONLY inline EXTERN Atom
AtomOfTerm (Term t)
{
  return (Atom) (NonTagPart (t));
}


#endif

INLINE_ONLY inline EXTERN int IsAtomTerm (Term);

INLINE_ONLY inline EXTERN int
IsAtomTerm (Term t)
{
  return (int) (CHKTAG ((t), AtomTag));
}




INLINE_ONLY inline EXTERN Term MkIntTerm (Int);

INLINE_ONLY inline EXTERN Term
MkIntTerm (Int n)
{
  return (Term) (TAGGED (NumberTag, (n)));
}


/*
  A constant to subtract or add to a well-known term, we assume no
  overflow problems are possible
*/

INLINE_ONLY inline EXTERN Term MkIntConstant (Int);

INLINE_ONLY inline EXTERN Term
MkIntConstant (Int n)
{
  return (Term) (NONTAGGED (NumberTag, (n)));
}



INLINE_ONLY inline EXTERN int IsIntTerm (Term);

INLINE_ONLY inline EXTERN int
IsIntTerm (Term t)
{
  return (int) (CHKTAG ((t), NumberTag));
}


INLINE_ONLY EXTERN inline Term MkPairTerm__(Term head, Term  tail USES_REGS );

INLINE_ONLY EXTERN inline Term
MkPairTerm__ (Term head, Term tail USES_REGS)
{
  register CELL *p = H;

  H[0] = head;
  H[1] = tail;
  H += 2;
  return (AbsPair (p));
}



/* Needed to handle numbers:
   	these two macros are fundamental in the integer/float conversions */

#ifdef M_WILLIAMS
#define IntInBnd(X)	(TRUE)
#else
#ifdef TAGS_FAST_OPS
#define IntInBnd(X)	(Unsigned( ( (Int)(X) >> (32-7) ) + 1) <= 1)
#else
#define IntInBnd(X)	( (X) < MAX_ABS_INT && \
                          (X) > -MAX_ABS_INT-1L )
#endif
#endif
#ifdef C_PROLOG
#define FlIsInt(X)	( (X) == (Int)(X) && IntInBnd((X)) )
#else
#define FlIsInt(X)	( FALSE )
#endif


/*
  There are two types of functors:

  o Special functors mark special terms
  on the heap that should be seen as constants.

  o Standard functors mark normal applications.

*/

#include  "TermExt.h"

#define IsAccessFunc(func)		((func) == FunctorAccess)

#ifdef YAP_H
INLINE_ONLY inline EXTERN Term MkIntegerTerm (Int);

INLINE_ONLY inline EXTERN Term
MkIntegerTerm (Int n)
{
  return (Term) (IntInBnd (n) ? MkIntTerm (n) : MkLongIntTerm (n));
}
#endif


INLINE_ONLY inline EXTERN int IsIntegerTerm (Term);

INLINE_ONLY inline EXTERN int
IsIntegerTerm (Term t)
{
  return (int) (IsIntTerm (t) || IsLongIntTerm (t));
}

INLINE_ONLY inline EXTERN Int IntegerOfTerm (Term);

INLINE_ONLY inline EXTERN Int
IntegerOfTerm (Term t)
{

  return (Int) (IsIntTerm (t) ? IntOfTerm (t) : LongIntOfTerm (t));
}

#ifndef YAP_H

#endif
