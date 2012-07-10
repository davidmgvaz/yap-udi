/*************************************************************************
*									 *
*	 YAP Prolog 	%W% %G% 					 *
*	Yap Prolog was developed at NCCUP - Universidade do Porto	 *
*									 *
* Copyright L.Damas, V.S.Costa and Universidade do Porto 1985-2012	 *
*									 *
**************************************************************************
*									 *
* File:		YapCompounTerm.h					 *
* mods:									 *
* comments:	main header file for YAP				 *
* version:      $Id: Yap.h,v 1.38 2008-06-18 10:02:27 vsc Exp $	 *
*************************************************************************/

/*************************************************************************************************
                           High level macros to access arguments
*************************************************************************************************/

#ifndef YAPCOMPOUNDTERM_H

#define YAPCOMPOUNDTERM_H 1

#include "inline-only.h"

EXTERN Int Yap_unify(Term a,Term b);

INLINE_ONLY EXTERN inline Term Deref(Term a);

INLINE_ONLY EXTERN inline Term Deref(Term a)
{
   while(IsVarTerm(a)) {
	Term *b = (Term *) a;
	a = *b;
	if(a==((Term) b)) return a;
   }
   return(a);
}

INLINE_ONLY EXTERN inline Term Derefa(CELL *b);

INLINE_ONLY EXTERN inline Term
Derefa(CELL *b)
{
  Term a = *b;
 restart:
  if (!IsVarTerm(a)) {
    return(a);
  } else if (a == (CELL)b) {
    return(a);
  } else {
    b = (CELL *)a;
    a = *b;
    goto restart;
  }
}

INLINE_ONLY inline EXTERN Term ArgOfTerm (int i, Term t);

INLINE_ONLY inline EXTERN Term
ArgOfTerm (int i, Term t)
{
  return (Term) (Derefa (RepAppl (t) + (i)));
}



INLINE_ONLY inline EXTERN Term HeadOfTerm (Term);

INLINE_ONLY inline EXTERN Term
HeadOfTerm (Term t)
{
  return (Term) (Derefa (RepPair (t)));
}



INLINE_ONLY inline EXTERN Term TailOfTerm (Term);

INLINE_ONLY inline EXTERN Term
TailOfTerm (Term t)
{
  return (Term) (Derefa (RepPair (t) + 1));
}




INLINE_ONLY inline EXTERN Term ArgOfTermCell (int i, Term t);

INLINE_ONLY inline EXTERN Term
ArgOfTermCell (int i, Term t)
{
  return (Term) ((CELL) (RepAppl (t) + (i)));
}



INLINE_ONLY inline EXTERN Term HeadOfTermCell (Term);

INLINE_ONLY inline EXTERN Term
HeadOfTermCell (Term t)
{
  return (Term) ((CELL) (RepPair (t)));
}



INLINE_ONLY inline EXTERN Term TailOfTermCell (Term);

INLINE_ONLY inline EXTERN Term
TailOfTermCell (Term t)
{
  return (Term) ((CELL) (RepPair (t) + 1));
}

#endif /* YAPCOMPOUNDTERM_H */
