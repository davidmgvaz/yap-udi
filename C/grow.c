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
* File:		grow.c							 *
* Last rev:	Thu Feb 23 1989		vv				 *
* mods:									 *
* comments:	Shifting the stacks       				 *
*									 *
*************************************************************************/

#include "absmi.h"
#include "YapHeap.h"
#include "yapio.h"
#include "alloc.h"
#include "sshift.h"
#include "compile.h"
#include "attvar.h"
#ifdef CUT_C
#include "cut_c.h"
#endif /* CUT_C */
#if HAVE_STRING_H
#include <string.h>
#endif
#if YAPOR_THREADS
#include "opt.mavar.h"
#endif /* YAPOR_THREADS */

#if !HAVE_STRNCAT
#define strncat(s0,s1,sz)   strcat(s0,s1)
#endif

typedef enum {
  STACK_SHIFTING = 0,
  STACK_COPYING = 1,
  STACK_INCREMENTAL_COPYING = 2
} what_stack_copying;



STATIC_PROTO(Int p_growheap, ( USES_REGS1 ));
STATIC_PROTO(Int p_growstack, ( USES_REGS1 ));
STATIC_PROTO(Int p_inform_trail_overflows, ( USES_REGS1 ));
STATIC_PROTO(Int p_inform_heap_overflows, ( USES_REGS1 ));
STATIC_PROTO(Int p_inform_stack_overflows, ( USES_REGS1 ));

/* #define undf7  */
/* #define undf5 */

STATIC_PROTO(int growstack, (long CACHE_TYPE));
STATIC_PROTO(void MoveGlobal, ( CACHE_TYPE1 ));
STATIC_PROTO(void MoveLocalAndTrail, ( CACHE_TYPE1 ));
STATIC_PROTO(void SetHeapRegs, (int CACHE_TYPE));
STATIC_PROTO(void AdjustTrail, (int, int CACHE_TYPE));
STATIC_PROTO(void AdjustLocal, (int CACHE_TYPE));
STATIC_PROTO(void AdjustGlobal, (long, int CACHE_TYPE));
STATIC_PROTO(void AdjustGrowStack, ( CACHE_TYPE1 ));
STATIC_PROTO(int  static_growheap, (long,int,struct intermediates *,tr_fr_ptr *, TokEntry **, VarEntry ** CACHE_TYPE));
STATIC_PROTO(void cpcellsd, (CELL *, CELL *, CELL));
STATIC_PROTO(CELL AdjustAppl, (CELL CACHE_TYPE));
STATIC_PROTO(CELL AdjustPair, (CELL CACHE_TYPE));
STATIC_PROTO(void AdjustStacksAndTrail, (long, int CACHE_TYPE));
STATIC_PROTO(void AdjustRegs, (int CACHE_TYPE));
STATIC_PROTO(Term AdjustGlobTerm, (Term CACHE_TYPE));

static void
LeaveGrowMode(prolog_exec_mode grow_mode)
{
  CACHE_REGS
  LOCAL_PrologMode &= ~grow_mode;
}


static void
cpcellsd(register CELL *Dest, register CELL *Org, CELL NOf)
{
#if HAVE_MEMMOVE
  memmove((void *)Dest, (void *)Org, NOf*sizeof(CELL));
#else
  register Int    n_of = NOf;
  for (; n_of >= 0; n_of--)
    *Dest++ = *Org++;
#endif
}


static void
SetHeapRegs(int copying_threads USES_REGS)
{
#ifdef undf7
  fprintf(GLOBAL_stderr,"HeapBase = %x\tHeapTop=%x\nGlobalBase=%x\tGlobalTop=%x\nLocalBase=%x\tLocatTop=%x\n", Yap_HeapBase, HeapTop, LOCAL_GlobalBase, H, LCL0, ASP);
#endif
  /* The old stack pointers */
  LOCAL_OldLCL0 = LCL0;
  LOCAL_OldASP = ASP;
  LOCAL_OldGlobalBase = (CELL *)LOCAL_GlobalBase;
  LOCAL_OldH = H;
  LOCAL_OldH0 = H0;
  LOCAL_OldTrailBase = LOCAL_TrailBase;
  LOCAL_OldTrailTop = LOCAL_TrailTop;
  LOCAL_OldTR = TR;
  LOCAL_OldHeapBase = Yap_HeapBase;
  LOCAL_OldHeapTop = HeapTop;
  /* Adjust stack addresses */
  LOCAL_TrailBase = TrailAddrAdjust(LOCAL_TrailBase);
  LOCAL_TrailTop = TrailAddrAdjust(LOCAL_TrailTop);
  CurrentTrailTop = (tr_fr_ptr)(LOCAL_TrailTop-MinTrailGap);
  if (LOCAL_GDiff) {
    /* make sure we are not just expanding the delay stack */
    LOCAL_GlobalBase = BaseAddrAdjust(LOCAL_GlobalBase);
  }
  LOCAL_LocalBase = LocalAddrAdjust(LOCAL_LocalBase);
#if !USE_SYSTEM_MALLOC && !USE_DL_MALLOC
  AuxSp = PtoBaseAdjust(AuxSp);
  AuxTop = (ADDR)PtoBaseAdjust((CELL *)AuxTop);
#endif
#if !USE_SYSTEM_MALLOC
  if (HeapLim)
    HeapLim = BaseAddrAdjust(HeapLim);
#endif
  /* The registers pointing to one of the stacks */
  if (ENV)
    ENV = PtoLocAdjust(ENV);
  if (ASP)
    ASP = PtoLocAdjust(ASP);
  if (H0)
    H0 = PtoGloAdjust(H0);
  LOCK(LOCAL_SignalLock);
  if (LCL0)
    LCL0 = PtoLocAdjust(LCL0);
  UNLOCK(LOCAL_SignalLock);
  if (H)
    H = PtoGloAdjust(H);
#ifdef CUT_C
  if (Yap_REGS.CUT_C_TOP)
    Yap_REGS.CUT_C_TOP = CutCAdjust(Yap_REGS.CUT_C_TOP);
#endif
  if (HB)
    HB = PtoGloAdjust(HB);
  if (LOCAL_OpenArray)
    LOCAL_OpenArray = PtoGloAdjust(LOCAL_OpenArray);
  if (B)
    B = ChoicePtrAdjust(B);
#ifdef TABLING
  if (B_FZ)
    B_FZ = ChoicePtrAdjust(B_FZ);
  if (BB)
    BB = ChoicePtrAdjust(BB);
  if (H_FZ)
    H_FZ = PtoGloAdjust(H_FZ);
  if (TR_FZ)
    TR_FZ = PtoTRAdjust(TR_FZ);
#endif /* TABLING */
  if (TR)
    TR = PtoTRAdjust(TR);
  if (YENV)
    YENV = PtoLocAdjust(YENV);
  if (IsOldGlobalPtr(S))
    S = PtoGloAdjust(S);
  else if (IsOldLocalPtr(S))
    S = PtoLocAdjust(S);	   
  if (!copying_threads) {
    if (LOCAL_GlobalArena)
      LOCAL_GlobalArena = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_GlobalArena)));
  }
#ifdef COROUTINING
  if (LOCAL_AttsMutableList)
    LOCAL_AttsMutableList = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_AttsMutableList)));
  if (LOCAL_WokenGoals)
    LOCAL_WokenGoals = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_WokenGoals)));
#endif
  LOCAL_GcGeneration = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_GcGeneration)));
  LOCAL_GcPhase = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_GcPhase)));
}

static void
MoveLocalAndTrail( USES_REGS1 )
{
	/* cpcellsd(To,From,NOfCells) - copy the cells downwards  */
#if USE_SYSTEM_MALLOC
  cpcellsd(ASP, (CELL *)((char *)LOCAL_OldASP+LOCAL_BaseDiff), (CELL *)LOCAL_OldTR - LOCAL_OldASP);
#else
  cpcellsd(ASP, LOCAL_OldASP, (CELL *)LOCAL_OldTR - LOCAL_OldASP);
#endif
}

#ifdef YAPOR_THREADS

static void
CopyLocalAndTrail( USES_REGS1 )
{
	/* cpcellsd(To,From,NOfCells) - copy the cells downwards  */
#if USE_SYSTEM_MALLOC
  cpcellsd((void *)ASP, (void *)LOCAL_OldASP, (CELL *)LOCAL_OldTR - LOCAL_OldASP);
#endif
}

static void
IncrementalCopyStacksFromWorker( USES_REGS1 )
{
  memcpy((void *) PtoGloAdjust((CELL *)LOCAL_start_global_copy),
	 (void *) (LOCAL_start_global_copy),
	 (size_t) (LOCAL_end_global_copy - LOCAL_start_global_copy));
  memcpy((void *) PtoLocAdjust((CELL *)LOCAL_start_local_copy),
	 (void *) LOCAL_start_local_copy,
	 (size_t) (LOCAL_end_local_copy - LOCAL_start_local_copy));
  memcpy((void *) PtoTRAdjust((tr_fr_ptr)LOCAL_start_trail_copy),
	 (void *) (LOCAL_start_trail_copy),
	 (size_t) (LOCAL_end_trail_copy - LOCAL_start_trail_copy));
}

#ifndef TABLING
static CELL
worker_p_binding(int worker_p, CELL *aux_ptr)
{
  CACHE_REGS
  if (aux_ptr > H) {
    CELL reg = REMOTE_ThreadHandle(worker_p).current_yaam_regs->LCL0_[aux_ptr-LCL0];
    reg = AdjustGlobTerm(reg PASS_REGS);
    return reg;
  } else {
    CELL reg = REMOTE_ThreadHandle(worker_p).current_yaam_regs-> H0_[aux_ptr-H0];
    reg = AdjustGlobTerm(reg PASS_REGS);
    return reg;
  }
}
#endif

static void
RestoreTrail(int worker_p USES_REGS)
{
  tr_fr_ptr aux_tr;

  /* install fase --> TR and LOCAL_top_cp->cp_tr are equal */
  aux_tr = ((choiceptr) LOCAL_start_local_copy)->cp_tr;
  TR = ((choiceptr) LOCAL_end_local_copy)->cp_tr;
  if (TR == aux_tr)
    return;
  if (aux_tr < TR){
    Yap_Error(SYSTEM_ERROR, TermNil, "oops");
  }
  Yap_NEW_MAHASH((ma_h_inner_struct *)H PASS_REGS);
  while (TR != aux_tr) {
    CELL aux_cell = TrailTerm(--aux_tr);
    if (IsVarTerm(aux_cell)) {
      if (aux_cell < LOCAL_start_global_copy || EQUAL_OR_YOUNGER_CP((choiceptr)LOCAL_end_local_copy, (choiceptr)aux_cell)) {
	YAPOR_ERROR_CHECKING((CELL *)aux_cell < H0, "RestoreTrail: aux_cell < H0");
	YAPOR_ERROR_CHECKING((ADDR)aux_cell > LOCAL_LocalBase, "RestoreTrail: aux_cell > LocalBase");
#ifdef TABLING
        *((CELL *) aux_cell) = TrailVal(aux_tr);
#else
        *((CELL *) aux_cell) = worker_p_binding(worker_p, CellPtr(aux_cell));
#endif /* TABLING */
      }
#ifdef TABLING 
    } else if (IsPairTerm(aux_cell)) {
      /* avoid frozen segments */
      aux_cell = (CELL) RepPair(aux_cell);
      if (IN_BETWEEN(LOCAL_TrailBase, aux_cell, LOCAL_TrailTop)) {
        aux_tr = (tr_fr_ptr) aux_cell;
      }
#endif /* TABLING */
#ifdef MULTI_ASSIGNMENT_VARIABLES
    } else if (IsApplTerm(aux_cell)) {
      CELL *cell_ptr = RepAppl(aux_cell);
      if (((CELL *)aux_cell < Get_LOCAL_top_cp()->cp_h || 
	   EQUAL_OR_YOUNGER_CP(Get_LOCAL_top_cp(), (choiceptr)aux_cell)) &&
	  !Yap_lookup_ma_var(cell_ptr PASS_REGS)) {
	/* first time we found the variable, let's put the new value */
#ifdef TABLING
        *cell_ptr = TrailVal(aux_tr);
#else
        *cell_ptr = worker_p_binding(worker_p, cell_ptr);
#endif /* TABLING */
      }
      /* skip the old value */
      aux_tr--;
#endif /* MULTI_ASSIGNMENT_VARIABLES */
    }
  }
}

#endif /* YAPOR_THREADS */

static void
MoveGlobal( USES_REGS1 )
{
  /*
   * cpcellsd(To,From,NOfCells) - copy the cells downwards - in
   * absmi.asm 
   */
   cpcellsd((CELL *)LOCAL_GlobalBase, (CELL *)LOCAL_OldGlobalBase, LOCAL_OldH - (CELL *)LOCAL_OldGlobalBase);  
}

static void
MoveExpandedGlobal( USES_REGS1 )
{
  /*
   * cpcellsd(To,From,NOfCells) - copy the cells downwards - in
   * absmi.asm 
   */
  cpcellsd((CELL *)(LOCAL_GlobalBase+(LOCAL_GDiff-LOCAL_BaseDiff)), (CELL *)LOCAL_GlobalBase, LOCAL_OldH - (CELL *)LOCAL_OldGlobalBase);  
}

static void
MoveGlobalWithHole( USES_REGS1 )
{
  /*
   * cpcellsd(To,From,NOfCells) - copy the cells downwards - in
   * absmi.asm 
   */
#if USE_SYSTEM_MALLOC
  cpcellsd((CELL *)((char *)LOCAL_GlobalBase+(LOCAL_GDiff0-LOCAL_BaseDiff)), (CELL *)LOCAL_GlobalBase, LOCAL_OldH - (CELL *)LOCAL_OldGlobalBase);  
#else
  cpcellsd((CELL *)((char *)LOCAL_OldGlobalBase+LOCAL_GDiff0), (CELL *)LOCAL_OldGlobalBase, LOCAL_OldH - (CELL *)LOCAL_OldGlobalBase);  
#endif
}

static void
MoveHalfGlobal(CELL *OldPt USES_REGS)
{
	/*
	 * cpcellsd(To,From,NOfCells) - copy the cells downwards - in
	 * absmi.asm 
	 */
  UInt diff = LOCAL_OldH-OldPt;
  CELL *NewPt = (CELL *)((char*)OldPt+LOCAL_GDiff);
  CELL *IntPt = (CELL *)((char*)OldPt+LOCAL_GDiff0);
  cpcellsd(NewPt, IntPt, diff);
}

static inline CELL
AdjustAppl(register CELL t0 USES_REGS)
{
  register CELL   *t = RepAppl(t0);

  if (IsOldGlobalPtr(t))
    return (AbsAppl(PtoGloAdjust(t)));
  else if (IsOldTrailPtr(t))
    return (AbsAppl(CellPtoTRAdjust(t)));
  else if (IsHeapP(t))
    return (AbsAppl(CellPtoHeapAdjust(t)));
#ifdef DEBUG
  else {
    /* strange cell */
    /*    fprintf(GLOBAL_stderr,"% garbage appl %lx found in stacks by stack shifter\n", t0);*/
  }
#endif
  return(t0);
}

static inline CELL
AdjustPair(register CELL t0 USES_REGS)
{
  register CELL  *t = RepPair(t0);

  if (IsOldGlobalPtr(t))
    return (AbsPair(PtoGloAdjust(t)));
  if (IsOldTrailPtr(t))
    return (AbsPair(CellPtoTRAdjust(t)));
  else if (IsHeapP(t))
    return (AbsPair(CellPtoHeapAdjust(t)));
#ifdef DEBUG
  /* fprintf(GLOBAL_stderr,"% garbage pair %lx found in stacks by stack shifter\n", t0);*/
#endif
  return(t0);
}

static void
AdjustTrail(int adjusting_heap, int thread_copying USES_REGS)
{
  volatile tr_fr_ptr ptt, tr_base = (tr_fr_ptr)LOCAL_TrailBase;

#if defined(YAPOR_THREADS)
  if (thread_copying == STACK_INCREMENTAL_COPYING) {
    ptt =  (tr_fr_ptr)(LOCAL_end_trail_copy);
    tr_base =  (tr_fr_ptr)(LOCAL_start_trail_copy);
  } else {
#endif
    ptt = TR;
#if defined(YAPOR_THREADS)
  }
#endif
  /* moving the trail is simple, yeaahhh! */
  while (ptt != tr_base) {
    register CELL reg = TrailTerm(ptt-1);
#ifdef FROZEN_STACKS
    register CELL reg2 = TrailVal(ptt-1);
#endif

    ptt--;
    if (IsVarTerm(reg)) {
      if (IsOldLocalInTR(reg))
	TrailTerm(ptt) = LocalAdjust(reg);
      else if (IsOldGlobal(reg))
	TrailTerm(ptt) = GlobalAdjust(reg);
      else if (IsOldTrail(reg))
	TrailTerm(ptt) = TrailAdjust(reg);
      else if (thread_copying) {
	RESET_VARIABLE(&TrailTerm(ptt));
      }
    } else if (IsPairTerm(reg)) {
      TrailTerm(ptt) = AdjustPair(reg PASS_REGS);
#ifdef MULTI_ASSIGNMENT_VARIABLES /* does not work with new structures */
    } else if (IsApplTerm(reg)) {
      TrailTerm(ptt) = AdjustAppl(reg PASS_REGS);
#endif
    }
#ifdef FROZEN_STACKS
    if (IsVarTerm(reg2)) {
      if (IsOldLocal(reg2))
	TrailVal(ptt) = LocalAdjust(reg2);
      else if (IsOldGlobal(reg2))
	TrailVal(ptt) = GlobalAdjust(reg2);
      else if (IsOldTrail(reg2))
	TrailVal(ptt) = TrailAdjust(reg2);
    } else if (IsApplTerm(reg2)) {
      TrailVal(ptt) = AdjustAppl(reg2 PASS_REGS);
    } else if (IsPairTerm(reg2)) {
      TrailVal(ptt) = AdjustPair(reg2 PASS_REGS);
    }
#endif
  }

}

static void
AdjustLocal(int thread_copying USES_REGS)
{
  register CELL   reg, *pt, *pt_bot;

  /* Adjusting the local */
#if defined(YAPOR_THREADS)
  if (thread_copying == STACK_INCREMENTAL_COPYING) {
    pt =  (CELL *) (LOCAL_end_local_copy);
    pt_bot =  (CELL *) (LOCAL_start_local_copy);
  } else {
#endif
    pt = LCL0;
    pt_bot = ASP;
#if defined(YAPOR_THREADS)
  }
#endif
  while (pt > pt_bot) {
    reg = *--pt;
    if (IsVarTerm(reg)) {
      if (IsOldLocal(reg))
	*pt = LocalAdjust(reg);
      else if (IsOldGlobal(reg))
	*pt = GlobalAdjust(reg);
      else if (IsOldTrail(reg))
	*pt = TrailAdjust(reg);
      else if (IsOldCode(reg))
	*pt = CodeAdjust(reg);
    } else if (IsApplTerm(reg)) {
      *pt = AdjustAppl(reg PASS_REGS);
    } else if (IsPairTerm(reg)) {
      *pt = AdjustPair(reg PASS_REGS);
    }
  }

}

static Term
AdjustGlobTerm(Term reg USES_REGS)
{
  if (IsVarTerm(reg)) {
    if (IsOldGlobal(reg))
      return GlobalAdjust(reg);
    else if (IsOldLocal(reg))
      return LocalAdjust(reg);
#ifdef MULTI_ASSIGNMENT_VARIABLES
    else if (IsOldTrail(reg))
      return TrailAdjust(reg);
#endif
  } else if (IsApplTerm(reg))
    return AdjustAppl(reg PASS_REGS);
  else if (IsPairTerm(reg))
    return AdjustPair(reg PASS_REGS);
  return AtomTermAdjust(reg);
}

static volatile CELL *cpt=NULL;

static void
AdjustGlobal(long sz, int thread_copying USES_REGS)
{
  CELL *pt, *pt_max;
  ArrayEntry *al = LOCAL_DynamicArrays;
  StaticArrayEntry *sal = LOCAL_StaticArrays;
  GlobalEntry *gl = LOCAL_GlobalVariables;

  while (al) {
    al->ValueOfVE = AdjustGlobTerm(al->ValueOfVE PASS_REGS);
    al = al->NextAE;
  }
  while (gl) {
    if (IsVarTerm(gl->global) ||
	!IsAtomOrIntTerm(gl->global)) {
      gl->global = AdjustGlobTerm(gl->global PASS_REGS);
    }
    gl = gl->NextGE;
  }
  while (sal) {
    if (sal->ArrayType == array_of_nb_terms) {
      UInt arity = -sal->ArrayEArity, i;
      for (i=0; i < arity; i++) {
	/*	    sal->ValueOfVE.lterms[i].tlive = AdjustGlobTerm(sal->ValueOfVE.lterms[i].tlive); */
	Term tlive  = sal->ValueOfVE.lterms[i].tlive;
	if (!IsVarTerm(tlive) || !IsUnboundVar(&sal->ValueOfVE.lterms[i].tlive)) {
	    sal->ValueOfVE.lterms[i].tlive = AdjustGlobTerm(sal->ValueOfVE.lterms[i].tlive PASS_REGS);
	}
      }
    }
    sal = sal->NextAE;
  }

  /*
   * to clean the global now that functors are just variables pointing to
   * the code 
   */
#if defined(YAPOR_THREADS)
  if (thread_copying == STACK_INCREMENTAL_COPYING) {
    pt =  (CELL *) (LOCAL_start_global_copy);
    pt_max =  (CELL *) (LOCAL_end_global_copy);
  } else {
#endif
    pt = H0;
    pt_max = (H-sz/CellSize);
#if defined(YAPOR_THREADS)
  }
#endif
  pt = H0;
  while (pt < pt_max) {
    CELL reg;
    
    cpt = pt;
    reg = *pt;
    if (IsVarTerm(reg)) {
      if (IsOldGlobal(reg))
	*pt = GlobalAdjust(reg);
      else if (IsOldLocal(reg))
	*pt = LocalAdjust(reg);
      else if (IsOldCode(reg) || IsExtensionFunctor((Functor)reg)) {
	Functor f;
	f = (Functor)reg;
	/* skip bitmaps */
	switch((CELL)f) {
	case (CELL)FunctorDouble:
#if SIZEOF_DOUBLE == 2*SIZEOF_LONG_INT
	  pt += 3;
#else
	  pt += 2;
#endif
	  break;
	case (CELL)FunctorBigInt:
	  {
	    Int sz = 2+
	      (sizeof(MP_INT)+
	       (((MP_INT *)(pt+2))->_mp_alloc*sizeof(mp_limb_t)))/CellSize;
	    Opaque_CallOnGCMark f;
	    Opaque_CallOnGCRelocate f2;
	    Term t = AbsAppl(pt);

	    if ( (f = Yap_blob_gc_mark_handler(t)) ) {
	      CELL ar[256];
	      Int i,n = (f)(Yap_BlobTag(t), Yap_BlobInfo(t), ar, 256);
	      if (n < 0) {
		Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"not enough space for slot internal variables");
	      }
	      for (i = 0; i< n; i++) {
		CELL *pt = ar+i;
		CELL reg = *pt;
		if (IsVarTerm(reg)) {
		  if (IsOldGlobal(reg))
		    *pt = GlobalAdjust(reg);
		  else if (IsOldLocal(reg))
		    *pt = LocalAdjust(reg);
#ifdef MULTI_ASSIGNMENT_VARIABLES
		  else if (IsOldTrail(reg))
		    *pt = TrailAdjust(reg);
#endif
		} else if (IsApplTerm(reg))
		  *pt = AdjustAppl(reg PASS_REGS);
		else if (IsPairTerm(reg))
		  *pt = AdjustPair(reg PASS_REGS);
		else if (IsAtomTerm(reg))
		  *pt = AtomTermAdjust(reg);
	      }
	      if ( (f2 = Yap_blob_gc_relocate_handler(t)) < 0 ) {
		int out = (f2)(Yap_BlobTag(t), Yap_BlobInfo(t), ar, n);
		if (out < 0)
		Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"bad restore of slot internal variables");
	      }
	    }
	    pt += sz;
	  }
	  break;
	case (CELL)0L:
	  break;
	case (CELL)FunctorLongInt:
	  pt += 2;
	  break;
	default:
	  *pt = CodeAdjust(reg);
	}
      }
#ifdef MULTI_ASSIGNMENT_VARIABLES
      else if (IsOldTrail(reg))
	*pt = TrailAdjust(reg);
#endif
    } else if (IsApplTerm(reg))
      *pt = AdjustAppl(reg PASS_REGS);
    else if (IsPairTerm(reg))
      *pt = AdjustPair(reg PASS_REGS);
    else if (IsAtomTerm(reg))
      *pt = AtomTermAdjust(reg);
    pt++;
  }
}

/*
 * When growing the stack we need to adjust: the local stack cells pointing
 * to the local; the local stack cells and the X terms pointing to the global
 * (just once) the trail cells pointing both to the global and to the local
 */
static void
AdjustStacksAndTrail(long sz, int copying_threads USES_REGS)
{
  AdjustTrail(TRUE, copying_threads PASS_REGS);
  AdjustLocal(copying_threads PASS_REGS);
  AdjustGlobal(sz, copying_threads PASS_REGS);
}

void
Yap_AdjustStacksAndTrail(void)
{
  CACHE_REGS
  AdjustStacksAndTrail(0, FALSE PASS_REGS);
}

/*
 * When growing the stack we need to adjust: the local cells pointing to the
 * local; the trail cells pointing to the local
 */
static void
AdjustGrowStack( USES_REGS1 )
{
  AdjustTrail(FALSE, STACK_SHIFTING PASS_REGS);
  AdjustLocal(STACK_SHIFTING PASS_REGS);
}

static void
AdjustRegs(int n USES_REGS)
{
  int            i;
  CELL	       reg;
	
  for (i = 1; i < n; ++i) {
    reg = (CELL) XREGS[i];
    if (IsVarTerm(reg)) {
      if (IsOldLocal(reg))
	reg = LocalAdjust(reg);
      else if (IsOldGlobal(reg))
	reg = GlobalAdjust(reg);
      else if (IsOldTrail(reg))
	reg = TrailAdjust(reg);
      else if (IsOldCode(reg))
	reg = CodeAdjust(reg);
    } else if (IsApplTerm(reg))
      reg = AdjustAppl(reg PASS_REGS);
    else if (IsPairTerm(reg))
      reg = AdjustPair(reg PASS_REGS);
    XREGS[i] = (Term) reg;
  }
}

static void
AdjustVarTable(VarEntry *ves USES_REGS)
{
  ves->VarAdr = TermNil;
  if (ves->VarRight != NULL) {
    if (IsOldVarTableTrailPtr(ves->VarRight)) {
      ves->VarRight = (VarEntry *)TrailAddrAdjust((ADDR)(ves->VarRight));
    }
    AdjustVarTable(ves->VarRight PASS_REGS);
  }
  if (ves->VarLeft != NULL) {
    if (IsOldVarTableTrailPtr(ves->VarLeft)) {
      ves->VarLeft = (VarEntry *)TrailAddrAdjust((ADDR)(ves->VarLeft));
    }
    AdjustVarTable(ves->VarLeft PASS_REGS);
  }
}

/*
  If we have to shift while we are scanning we need to adjust all
  pointers created by the scanner (Tokens and Variables)
*/
static void
AdjustScannerStacks(TokEntry **tksp, VarEntry **vep USES_REGS)
{
  TokEntry *tks = *tksp;
  VarEntry *ves = *vep;

  if (tks != NULL) {
    if (IsOldTokenTrailPtr(tks)) {
      tks = *tksp = TokEntryAdjust(tks);
    }
  }
  while (tks != NULL) {
    TokEntry *tktmp;

    switch (tks->Tok) {
    case Number_tok:
      if (IsApplTerm(tks->TokInfo)) {
	tks->TokInfo = AdjustAppl(tks->TokInfo PASS_REGS);
      }
      break;      
    case Var_tok:
    case String_tok:
      if (IsOldTrail(tks->TokInfo))
	tks->TokInfo = TrailAdjust(tks->TokInfo);
      break;
    case Name_tok:
      tks->TokInfo = (Term)AtomAdjust((Atom)(tks->TokInfo));
      break;
    default:
      break;
    }
    tktmp = tks->TokNext;
    if (tktmp != NULL) {
      if (IsOldTokenTrailPtr(tktmp)) {
	tktmp = TokEntryAdjust(tktmp);
	tks->TokNext = tktmp;
      }
    }
    tks = tktmp;
  }
  if (ves != NULL) {
    if (IsOldVarTableTrailPtr(ves))
      ves = *vep = (VarEntry *)TrailAddrAdjust((ADDR)ves);
    AdjustVarTable(ves PASS_REGS);
  }
  ves = LOCAL_AnonVarTable;
  if (ves != NULL) {
    if (IsOldVarTableTrailPtr(ves))
      ves = LOCAL_AnonVarTable = VarEntryAdjust(ves);
  }
  while (ves != NULL) {
    VarEntry *vetmp = ves->VarLeft;
    if (vetmp != NULL) {
      if (IsOldVarTableTrailPtr(vetmp)) {
	vetmp = VarEntryAdjust(vetmp);
      }
      ves->VarLeft = vetmp;
    }
    ves->VarAdr = TermNil;
    ves = vetmp;
  }
}

void
Yap_AdjustRegs(int n)
{
  CACHE_REGS
  AdjustRegs(n PASS_REGS);
}

/* Used by do_goal() when we're short of heap space */
static int
static_growheap(long size, int fix_code, struct intermediates *cip, tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep USES_REGS)
{
  UInt start_growth_time, growth_time;
  int gc_verbose;
  UInt minimal_request = 0L;

  /* adjust to a multiple of 256) */
  if (size < YAP_ALLOC_SIZE)
    size = YAP_ALLOC_SIZE;
  size = AdjustPageSize(size);
  LOCAL_ErrorMessage = NULL;
  if (!Yap_ExtendWorkSpace(size)) {
    Int min_size = AdjustPageSize(((CELL)LOCAL_TrailTop-(CELL)LOCAL_GlobalBase)+MinHeapGap);

    LOCAL_ErrorMessage = NULL;
    if (size < min_size) size = min_size;
    minimal_request = size;
    size = Yap_ExtendWorkSpaceThroughHole(size);
    if (size < 0) {
      LOCAL_ErrorMessage = "Database crashed against Stacks";
      return FALSE;
    }
  }
  start_growth_time = Yap_cputime();
  gc_verbose = Yap_is_gc_verbose();
  LOCAL_heap_overflows++;
  if (gc_verbose) {
#if  defined(YAPOR_THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% Database Overflow %d\n", LOCAL_heap_overflows);
    fprintf(GLOBAL_stderr, "%%   growing the heap %ld bytes\n", size);
  }
  /* CreepFlag is set to force heap expansion */
  if (LOCAL_ActiveSignals == YAP_CDOVF_SIGNAL) {
    LOCK(LOCAL_SignalLock);
    CreepFlag = CalculateStackGap();
    UNLOCK(LOCAL_SignalLock);
  }
  ASP -= 256;
  LOCAL_TrDiff = LOCAL_LDiff = LOCAL_GDiff = LOCAL_GDiff0 = LOCAL_DelayDiff = LOCAL_BaseDiff = size;
  LOCAL_XDiff = LOCAL_HDiff = 0;
  LOCAL_GSplit = NULL;
  YAPEnterCriticalSection();
  SetHeapRegs(FALSE PASS_REGS);
  MoveLocalAndTrail( PASS_REGS1 );
  if (fix_code) {
    CELL *SaveOldH = LOCAL_OldH;
    LOCAL_OldH = (CELL *)cip->freep;
    MoveGlobal( PASS_REGS1 );
    LOCAL_OldH = SaveOldH;
  } else {
    MoveGlobal( PASS_REGS1 );
  }
  if (old_trp) {
    tr_fr_ptr nTR;

    AdjustScannerStacks(tksp, vep PASS_REGS);
    nTR = TR;
    *old_trp = PtoTRAdjust(*old_trp);
    TR = *old_trp;
    AdjustStacksAndTrail(0, FALSE PASS_REGS);
    TR = nTR;
  } else {
    AdjustStacksAndTrail(0, FALSE PASS_REGS);
  }
  AdjustRegs(MaxTemps PASS_REGS);
  ASP += 256;
  if (minimal_request) 
    Yap_AllocHole(minimal_request, size);
  YAPLeaveCriticalSection();
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_heap_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%%   took %g sec\n", (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% Total of %g sec expanding Database\n", (double)LOCAL_total_heap_overflow_time/1000);
  }
  return(TRUE);
}

/* Used when we're short of heap, usually because of an overflow in
   the attributed stack, but also because we allocated a zone  */
static int
static_growglobal(long request, CELL **ptr, CELL *hsplit USES_REGS)
{
  UInt start_growth_time, growth_time;
  int gc_verbose;
  char *omax = (char *)H0;
  ADDR old_GlobalBase = LOCAL_GlobalBase;
  UInt minimal_request = 0L;
  long size = request;
  char vb_msg1 = '\0', *vb_msg2;
  int do_grow = TRUE;
  int insert_in_delays = FALSE;
  /*
    request is the amount of memory we requested, in bytes;
    base_move is the shift in global stacks we had to do
    size is how much space we allocate: it's negative if we just expand
	the delay stack.
    do_grow is whether we expand stacks
  */

  if (hsplit) {
    /* just a little bit of sanity checking */
    if (hsplit < H0 && hsplit > (CELL *)LOCAL_GlobalBase) {
      insert_in_delays = TRUE;
      /* expanding attributed variables */
      if (omax - size > LOCAL_GlobalBase+4096*sizeof(CELL)) {
	/* we can just ask for more room */
	size = 0;
	do_grow = FALSE;
      }
    } else if (hsplit < (CELL*)omax ||
	hsplit > H)
      return FALSE;
    else if (hsplit == (CELL *)omax)
      hsplit = NULL;
    if (size < 0 ||
	(Unsigned(H)+size < Unsigned(ASP)-CreepFlag &&
	 hsplit > H0)) {
      /* don't need to expand stacks */
      insert_in_delays = FALSE;
      do_grow = FALSE;
    }
  } else {
    if (Unsigned(H)+size < Unsigned(ASP)-CreepFlag) {
      /* we can just ask for more room */
      do_grow = FALSE;
    }    
  }
  if (do_grow) {
    if (size < YAP_ALLOC_SIZE)
      size = YAP_ALLOC_SIZE;
    size = AdjustPageSize(size);
  }
  /* adjust to a multiple of 256) */
  LOCAL_ErrorMessage = NULL;
  LOCAL_PrologMode |= GrowStackMode;
  start_growth_time = Yap_cputime();
  if (do_grow) {
    if (!GLOBAL_AllowGlobalExpansion) {
      LOCAL_ErrorMessage = "Global Stack crashed against Local Stack";
      LeaveGrowMode(GrowStackMode);
      return 0;
    }
    if (!GLOBAL_AllowGlobalExpansion || !Yap_ExtendWorkSpace(size)) {
      /* always fails when using malloc */
      LOCAL_ErrorMessage = NULL;
      size += AdjustPageSize(((CELL)LOCAL_TrailTop-(CELL)LOCAL_GlobalBase)+MinHeapGap);   
      minimal_request = size;
      size = Yap_ExtendWorkSpaceThroughHole(size);
      if (size < 0) {
	LOCAL_ErrorMessage = "Global Stack crashed against Local Stack";
	LeaveGrowMode(GrowStackMode);
	return 0;
      }
    }
  }
  gc_verbose = Yap_is_gc_verbose();
  LOCAL_delay_overflows++;
  if (gc_verbose) {
    if (hsplit) {
      if (hsplit > H0) {
	vb_msg1 = 'H';
	vb_msg2 = "Global Variable Space";
      } else {
	vb_msg1 = 'D';
	vb_msg2 = "Global Variable Delay Space";
      }
    } else {
      vb_msg1 = 'D';
      vb_msg2 = "Delay";
    }
#if  defined(YAPOR_THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% %cO %s Overflow %d\n", vb_msg1, vb_msg2, LOCAL_delay_overflows);
    fprintf(GLOBAL_stderr, "%% %cO   growing the stacks %ld bytes\n", vb_msg1, size);
  }
  ASP -= 256;
  YAPEnterCriticalSection();
  /* we always shift the local and the stack by the same amount */
  if (do_grow) {
    /* we got over a hole */
    if (minimal_request) {
      /* we went over a hole */
      LOCAL_BaseDiff = size+((CELL)LOCAL_TrailTop-(CELL)LOCAL_GlobalBase)-minimal_request;
      LOCAL_LDiff = LOCAL_TrDiff = size;
    } else {
      /* we may still have an overflow */
      LOCAL_BaseDiff = LOCAL_GlobalBase - old_GlobalBase;
      /* if we grow, we need to move the stacks */
      LOCAL_LDiff = LOCAL_TrDiff = LOCAL_BaseDiff+size;
    }
  } else {
    /* stay still */
    LOCAL_LDiff = LOCAL_TrDiff = 0;
    LOCAL_BaseDiff = 0;
  }
  /* now, remember we have delay -- global with a hole in delay or a 
     hole in global */
  if (!hsplit) {
    if (!do_grow) {
      LOCAL_DelayDiff = LOCAL_GDiff = LOCAL_GDiff0 = size;
      request = 0L;
    } else {
      /* expand delay stack */
      LOCAL_DelayDiff = LOCAL_GDiff = LOCAL_GDiff0 = LOCAL_LDiff;
    }
  } else if (insert_in_delays) {
    /* we want to expand a hole for the delay stack */
    LOCAL_DelayDiff = size-request;
    LOCAL_GDiff = LOCAL_GDiff0 = size;
  } else {
    /* we want to expand a hole for the delay stack */
    LOCAL_GDiff0 = LOCAL_DelayDiff = LOCAL_BaseDiff;
    LOCAL_GDiff = LOCAL_BaseDiff+request;
  }
  LOCAL_GSplit = hsplit;
  LOCAL_XDiff = LOCAL_HDiff = 0;
  LOCAL_GlobalBase = old_GlobalBase;
  SetHeapRegs(FALSE PASS_REGS);
  if (do_grow) {
    MoveLocalAndTrail( PASS_REGS1 );
    if (hsplit) {
      MoveGlobalWithHole( PASS_REGS1 );
    } else {
      MoveExpandedGlobal( PASS_REGS1 );
    }
  } else if (!hsplit) {
    MoveExpandedGlobal( PASS_REGS1 );
  }
  /* don't run through garbage */
  if (hsplit && (LOCAL_OldH != hsplit)) {
    AdjustStacksAndTrail(request, FALSE PASS_REGS);
  } else {
    AdjustStacksAndTrail(0, FALSE PASS_REGS);
  }
  AdjustRegs(MaxTemps PASS_REGS);
  if (ptr) {
    *ptr = PtoLocAdjust(*ptr);
  }
  if (hsplit) {
    if (insert_in_delays) {
      /* we have things not quite where we want to have them */
      cpcellsd((CELL *)(omax+LOCAL_DelayDiff), (CELL *)(omax+LOCAL_GDiff0), (ADDR)hsplit-omax);
    } else {
      MoveHalfGlobal(hsplit PASS_REGS);
    }
  }
  YAPLeaveCriticalSection();
  ASP += 256;
  if (minimal_request) {
    Yap_AllocHole(minimal_request, size);
  }
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_delay_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%% %cO   took %g sec\n", vb_msg1, (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% %cO Total of %g sec expanding stacks \n", vb_msg1, (double)LOCAL_total_delay_overflow_time/1000);
  }
  LeaveGrowMode(GrowStackMode);
  if (hsplit) {
    return request;
  } else
    return LOCAL_GDiff-LOCAL_BaseDiff;
}

static void
fix_compiler_instructions(PInstr *pcpc USES_REGS)
{
  while (pcpc != NULL) {
    PInstr *ncpc = pcpc->nextInst;

    switch(pcpc->op) {
      /* check c_var for functions that point at variables */
    case get_var_op:
    case get_val_op:
    case unify_var_op:
    case unify_last_var_op:
    case unify_val_op:
    case unify_local_op:
    case unify_last_val_op:
    case unify_last_local_op:
    case put_var_op:
    case put_val_op:
    case put_unsafe_op:
    case write_unsafe_op:
    case write_var_op:
    case write_val_op:
    case write_local_op:
    case f_var_op:
    case f_val_op:
    case fetch_args_for_bccall:
    case bccall_op:
    case save_pair_op:
    case save_appl_op:
    case save_b_op:
    case commit_b_op:
    case fetch_args_vv_op:
    case fetch_args_cv_op:
    case fetch_args_vc_op:
      pcpc->rnd1 = GlobalAdjust(pcpc->rnd1);
      break;
    case get_float_op:
    case put_float_op:
    case get_longint_op:
    case put_longint_op:
    case unify_float_op:
    case unify_last_float_op:
    case write_float_op:
      /* floats might be in the global */
      pcpc->rnd1 = AdjustAppl(pcpc->rnd1 PASS_REGS);
      break;
      /* hopefully nothing to do */
    case nop_op:
    case ensure_space_op:
    case get_atom_op:
    case put_atom_op:
    case get_num_op:
    case put_num_op:
    case align_float_op:
    case get_bigint_op:
    case put_bigint_op:
    case get_dbterm_op:
    case put_dbterm_op:
    case get_list_op:
    case put_list_op:
    case get_struct_op:
    case put_struct_op:
    case unify_atom_op:
    case unify_last_atom_op:
    case write_atom_op:
    case unify_num_op:
    case unify_last_num_op:
    case write_num_op:
    case unify_longint_op:
    case unify_last_longint_op:
    case write_longint_op:
    case unify_bigint_op:
    case unify_last_bigint_op:
    case unify_dbterm_op:
    case unify_last_dbterm_op:
    case write_bigint_op:
    case write_dbterm_op:
    case unify_list_op:
    case write_list_op:
    case unify_struct_op:
    case write_struct_op:
    case fail_op:
    case cut_op:
    case cutexit_op:
    case allocate_op:
    case deallocate_op:
    case tryme_op:
    case jump_op:
    case jumpi_op:
    case procceed_op:
    case call_op:
    case execute_op:
    case safe_call_op:
    case label_op:
    case name_op:
    case pop_op:
    case retryme_op:
    case trustme_op:
    case either_op:
    case orelse_op:
    case orlast_op:
    case push_or_op:
    case pushpop_or_op:
    case pop_or_op:
    case patch_b_op:
    case try_op:
    case retry_op:
    case trust_op:
    case try_in_op:
    case jump_v_op:
    case jump_nv_op:
    case cache_arg_op:
    case cache_sub_arg_op:
    case user_switch_op:
    case switch_on_type_op:
    case switch_c_op:
    case if_c_op:
    case switch_f_op:
    case if_f_op:
    case if_not_op:
    case index_dbref_op:
    case index_blob_op:
    case index_long_op:
    case if_nonvar_op:
    case unify_last_list_op:
    case write_last_list_op:
    case unify_last_struct_op:
    case write_last_struct_op:
    case mark_initialised_pvars_op:
    case mark_live_regs_op:
    case enter_profiling_op:
    case retry_profiled_op:
    case count_call_op:
    case count_retry_op:
    case restore_tmps_op:
    case restore_tmps_and_skip_op:
    case enter_lu_op:
    case empty_call_op:
    case blob_op:
    case fetch_args_vi_op:
    case fetch_args_iv_op:
    case label_ctl_op:
    case f_0_op:
    case native_op:
#ifdef TABLING
    case table_new_answer_op:
    case table_try_single_op:
#endif /* TABLING */
#ifdef YAPOR
    case sync_op:
#endif
#ifdef BEAM
    case run_op:
    case body_op:
    case endgoal_op:
    case try_me_op:
    case retry_me_op:
    case trust_me_op:
    case only_1_clause_op:
    case create_first_box_op:
    case create_box_op:
    case create_last_box_op:
    case remove_box_op:
    case remove_last_box_op:
    case prepare_tries:
    case std_base_op:
    case direct_safe_call_op:
    case commit_op:
    case skip_while_var_op:
    case wait_while_var_op:
    case force_wait_op:
    case write_op:
    case is_op:
    case equal_op:
    case exit_op:
#endif
      break;
    }
    if (ncpc != NULL) {
      ncpc = (PInstr *)GlobalAddrAdjust((ADDR)(pcpc->nextInst));
      pcpc->nextInst = ncpc;
    }
    pcpc = ncpc;
  }
}

#ifdef TABLING
static void
fix_tabling_info( USES_REGS1 ) 
{
  /* we must fix the dependency frames and the subgoal frames, as they are
     pointing back to the global stack. */
  struct dependency_frame *df;
  struct subgoal_frame *sg;

  df = LOCAL_top_dep_fr;
  while (df) {
    if (DepFr_backchain_cp(df))
      DepFr_backchain_cp(df) = ChoicePtrAdjust(DepFr_backchain_cp(df));
    if (DepFr_leader_cp(df))
      DepFr_leader_cp(df) = ChoicePtrAdjust(DepFr_leader_cp(df));
    if (DepFr_cons_cp(df))
      DepFr_cons_cp(df) = ConsumerChoicePtrAdjust(DepFr_cons_cp(df));
    df = DepFr_next(df);
  }
  sg = LOCAL_top_sg_fr;
  while (sg) {
    if (SgFr_gen_cp(sg))
      SgFr_gen_cp(sg) = GeneratorChoicePtrAdjust(SgFr_gen_cp(sg));
    sg = SgFr_next(sg);
  }
}
#endif /* TABLING */

static int
do_growheap(int fix_code, UInt in_size, struct intermediates *cip, tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep USES_REGS)
{
  unsigned long size = sizeof(CELL) * K16;
  int shift_factor = (LOCAL_heap_overflows > 8 ? 8 : LOCAL_heap_overflows);
  unsigned long sz =  size << shift_factor;

  if (sz < in_size) {
    sz = in_size;
  }
#ifdef YAPOR
  Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"cannot grow Heap: more than a worker/thread running");
  return FALSE;
#endif
  if (GLOBAL_SizeOfOverflow > sz) {
    if (size < YAP_ALLOC_SIZE)
      size = YAP_ALLOC_SIZE;
    sz = AdjustPageSize(GLOBAL_SizeOfOverflow);
  }
  while(sz >= sizeof(CELL) * K16 && !static_growheap(sz, fix_code, cip, old_trp, tksp, vep PASS_REGS)) {
    size = size/2;
    sz =  size << shift_factor;
    if (sz < in_size) {
      return FALSE;
    }
  }
  /* we must fix an instruction chain */
  if (fix_code) {
    PInstr *pcpc = cip->CodeStart;
    if (pcpc != NULL) {
      cip->CodeStart = pcpc = (PInstr *)GlobalAddrAdjust((ADDR)pcpc);
    }
    fix_compiler_instructions(pcpc PASS_REGS);
    pcpc = cip->BlobsStart;
    if (pcpc != NULL) {
      cip->BlobsStart = pcpc = (PInstr *)GlobalAddrAdjust((ADDR)pcpc);
    }
    fix_compiler_instructions(pcpc PASS_REGS);
    cip->freep = (char *)GlobalAddrAdjust((ADDR)cip->freep);
    cip->label_offset = (Int *)GlobalAddrAdjust((ADDR)cip->label_offset);
  }
#ifdef TABLING
  fix_tabling_info( PASS_REGS1 );
#endif /* TABLING */
  if (sz >= sizeof(CELL) * K16) {
    LOCK(LOCAL_SignalLock);
    LOCAL_ActiveSignals &= ~YAP_CDOVF_SIGNAL;
    if (!LOCAL_ActiveSignals)
	CreepFlag = CalculateStackGap();
    UNLOCK(LOCAL_SignalLock);
    return TRUE;
  }
  /* failed */
  return FALSE;
}

static void
init_new_table(AtomHashEntry *ntb, UInt nsize)
{
  UInt i;

  for (i = 0; i < nsize; ++i) {
    INIT_RWLOCK(ntb[i].AERWLock);
    ntb[i].Entry = NIL;
  }
}

static void
cp_atom_table(AtomHashEntry *ntb, UInt nsize)
{
  UInt i;

  for (i = 0; i < AtomHashTableSize; i++) {
    Atom            catom;

    READ_LOCK(HashChain[i].AERWLock);
    catom = HashChain[i].Entry;
    while (catom != NIL) {
      AtomEntry *ap = RepAtom(catom);
      Atom natom;
      CELL hash;

      hash = HashFunction((unsigned char *)ap->StrOfAE) % nsize;
      natom = ap->NextOfAE;
      ap->NextOfAE = ntb[hash].Entry;
      ntb[hash].Entry = catom;
      catom = natom;
    }
    READ_UNLOCK(HashChain[i].AERWLock);
  }
}

static int
growatomtable( USES_REGS1 )
{
  AtomHashEntry *ntb;
  UInt nsize = 3*AtomHashTableSize-1;
  UInt start_growth_time = Yap_cputime(), growth_time;
  int gc_verbose = Yap_is_gc_verbose();
  if (nsize -AtomHashTableSize  > 4*1024*1024)
    nsize =  AtomHashTableSize+4*1024*1024+7919;

  LOCK(LOCAL_SignalLock);
  if (LOCAL_ActiveSignals == YAP_CDOVF_SIGNAL) {
    CreepFlag = CalculateStackGap();
  }
  LOCAL_ActiveSignals &= ~YAP_CDOVF_SIGNAL;
  UNLOCK(LOCAL_SignalLock);
  while ((ntb = (AtomHashEntry *)Yap_AllocCodeSpace(nsize*sizeof(AtomHashEntry))) == NULL) {
    /* leave for next time */
#if !USE_SYSTEM_MALLOC
    if (!do_growheap(FALSE, nsize*sizeof(AtomHashEntry), NULL, NULL, NULL, NULL))
#endif
      return FALSE;
  }
  LOCAL_atom_table_overflows ++;
  if (gc_verbose) {
#if  defined(YAPOR_THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% Atom Table Overflow %d\n", LOCAL_atom_table_overflows );
    fprintf(GLOBAL_stderr, "%%    growing the atom table to %ld entries\n", (long int)(nsize));
  }
  YAPEnterCriticalSection();
  init_new_table(ntb, nsize);
  cp_atom_table(ntb, nsize);
  Yap_FreeCodeSpace((char *)HashChain);
  HashChain = ntb;
  AtomHashTableSize = nsize;
  YAPLeaveCriticalSection();
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_atom_table_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%%   took %g sec\n", (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% Total of %g sec expanding atom table \n", (double)LOCAL_total_atom_table_overflow_time/1000);
  }
#if USE_SYSTEM_MALLOC
  return TRUE;
#else
  if (HeapTop + sizeof(YAP_SEG_SIZE)  > HeapLim - MinHeapGap) {
    /* make sure there is no heap overflow */
    int res;

    res = do_growheap(FALSE, 0, NULL, NULL, NULL, NULL PASS_REGS);
    return res;
  } else {
    return TRUE;
  }
#endif
}


int
Yap_growheap(int fix_code, size_t in_size, void *cip)
{
  CACHE_REGS
  int res;
  int blob_overflow = (NOfBlobs > NOfBlobsMax);

  if (NOfAtoms > 2*AtomHashTableSize || blob_overflow) {
    UInt n = NOfAtoms;
    if (GLOBAL_AGcThreshold)
      Yap_atom_gc( PASS_REGS1 );
    /* check if we have a significant improvement from agc */
    if (!blob_overflow &&
	(n > NOfAtoms+ NOfAtoms/10 ||
	 /* +1 = make sure we didn't lose the current atom */
	 NOfAtoms+1 > 2*AtomHashTableSize)) {
      res  = growatomtable( PASS_REGS1 );
    } else {
      LOCK(LOCAL_SignalLock);
      if (LOCAL_ActiveSignals == YAP_CDOVF_SIGNAL) {
	CreepFlag = CalculateStackGap();
      }
      LOCAL_ActiveSignals &= ~YAP_CDOVF_SIGNAL;
      UNLOCK(LOCAL_SignalLock);
      return TRUE;
    }
    LeaveGrowMode(GrowHeapMode);
    if (res)
      return res;
  }
#if USE_SYSTEM_MALLOC
  P = Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"malloc failed");
  res = FALSE;
#else
  res=do_growheap(fix_code, in_size, (struct intermediates *)cip, NULL, NULL, NULL PASS_REGS);
#endif
  LeaveGrowMode(GrowHeapMode);
  return res;
}

int
Yap_growheap_in_parser(tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep)
{
  CACHE_REGS
  int res;

  res=do_growheap(FALSE, 0L, NULL, old_trp, tksp, vep PASS_REGS);
  LeaveGrowMode(GrowHeapMode);
  return res;
}

int
Yap_growglobal(CELL **ptr)
{
  CACHE_REGS
  unsigned long sz = sizeof(CELL) * K16;

#if defined(YAPOR_THREADS)
  if (GLOBAL_number_workers != 1) {
    Yap_Error(OUT_OF_STACK_ERROR,TermNil,"cannot grow Global: more than a worker/thread running");
    return(FALSE);
  }
#elif defined(THREADS)
  if (GLOBAL_NOfThreads != 1) {
    Yap_Error(OUT_OF_STACK_ERROR,TermNil,"cannot grow Global: more than a worker/thread running");
    return(FALSE);
  }
#endif
  if ( static_growglobal(sz, ptr, NULL PASS_REGS) == 0)
    return FALSE;
#ifdef TABLING
  fix_tabling_info( PASS_REGS1 );
#endif /* TABLING */
  return TRUE;
}


UInt
Yap_InsertInGlobal(CELL *where, UInt howmuch)
{
  CACHE_REGS
  if ((howmuch = static_growglobal(howmuch, NULL, where PASS_REGS)) == 0)
    return 0;
#ifdef TABLING
  fix_tabling_info( PASS_REGS1 );
#endif /* TABLING */
  return howmuch;
}


int
Yap_growstack(long size)
{
  CACHE_REGS
  int res;

  LOCAL_PrologMode |= GrowStackMode;
  res=growstack(size PASS_REGS);
  LeaveGrowMode(GrowStackMode);
  return res;
}

static int
execute_growstack(long size0, int from_trail, int in_parser, tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep USES_REGS)
{
  UInt minimal_request = 0L;
  long size = size0;
  ADDR old_LOCAL_GlobalBase = LOCAL_GlobalBase;
  
  if (!GLOBAL_AllowGlobalExpansion) {
    LOCAL_ErrorMessage = "Database crashed against stacks";
    return FALSE;
  }
  if (!Yap_ExtendWorkSpace(size)) {
    /* make sure stacks and trail are contiguous */

    LOCAL_ErrorMessage = NULL;    
    minimal_request = AdjustPageSize(((CELL)LOCAL_TrailTop-(CELL)LOCAL_GlobalBase)+4*MinHeapGap+size0);

    size = Yap_ExtendWorkSpaceThroughHole(minimal_request);
    if (size < 0) {
      LOCAL_ErrorMessage = "Database crashed against stacks";
      return FALSE;
    }
    YAPEnterCriticalSection();
    LOCAL_GDiff = LOCAL_DelayDiff = LOCAL_BaseDiff = size-size0;
  } else {
    YAPEnterCriticalSection();
    if (LOCAL_GlobalBase != old_LOCAL_GlobalBase) {
      LOCAL_GDiff = LOCAL_BaseDiff = LOCAL_DelayDiff = LOCAL_GlobalBase-old_LOCAL_GlobalBase;
      LOCAL_GlobalBase=old_LOCAL_GlobalBase;
    } else {
      LOCAL_GDiff = LOCAL_BaseDiff = LOCAL_DelayDiff = 0;
    }
  }
  LOCAL_XDiff = LOCAL_HDiff = 0;
  LOCAL_GDiff0=LOCAL_GDiff;
#if USE_SYSTEM_MALLOC
  if (from_trail) {
    LOCAL_TrDiff = LOCAL_LDiff = LOCAL_GDiff;
  } else {
    LOCAL_TrDiff = LOCAL_LDiff = size+LOCAL_GDiff;
  }
#else
  if (from_trail) {
    LOCAL_TrDiff = LOCAL_LDiff = size-size0;
  } else {
    LOCAL_TrDiff = LOCAL_LDiff = size;
  }
#endif
  ASP -= 256;
  SetHeapRegs(FALSE PASS_REGS);
  if (from_trail) {
    LOCAL_TrailTop += size0;
    CurrentTrailTop = (tr_fr_ptr)(LOCAL_TrailTop-MinTrailGap);
  }
  if (LOCAL_LDiff) {
    MoveLocalAndTrail( PASS_REGS1 );
  }
  if (LOCAL_GDiff) {
#if !USE_SYSTEM_MALLOC
    /* That is done by realloc */
    MoveGlobal( PASS_REGS1 );
#endif
    if (in_parser) {
      tr_fr_ptr nTR;

      AdjustScannerStacks(tksp, vep PASS_REGS);
      nTR = TR;
      *old_trp = PtoTRAdjust(*old_trp);
      TR = *old_trp;
      AdjustStacksAndTrail(0, FALSE PASS_REGS);
      TR = nTR;
    } else {
      AdjustStacksAndTrail(0, FALSE PASS_REGS);
    }
    AdjustRegs(MaxTemps PASS_REGS);
#ifdef TABLING
    fix_tabling_info( PASS_REGS1 );
#endif /* TABLING */
  } else if (LOCAL_LDiff) {
    if (in_parser) {
      tr_fr_ptr nTR;

      AdjustScannerStacks(tksp, vep PASS_REGS);
      nTR = TR;
      *old_trp = PtoTRAdjust(*old_trp);
      TR = *old_trp;
      AdjustGrowStack( PASS_REGS1 );
      TR = nTR;
    } else {
      AdjustGrowStack( PASS_REGS1 );
    }
    AdjustRegs(MaxTemps PASS_REGS);
#ifdef TABLING
    fix_tabling_info( PASS_REGS1 );
#endif /* TABLING */
  }
  YAPLeaveCriticalSection();
  ASP += 256;
  if (minimal_request) 
    Yap_AllocHole(minimal_request, size);
  return TRUE;
}

/* Used by do_goal() when we're short of stack space */
static int
growstack(long size USES_REGS)
{
  UInt start_growth_time, growth_time;
  int gc_verbose;

  /* adjust to a multiple of 256) */
  if (size < YAP_ALLOC_SIZE)
    size = YAP_ALLOC_SIZE;
  size = AdjustPageSize(size);
  LOCAL_ErrorMessage = NULL;
  start_growth_time = Yap_cputime();
  gc_verbose = Yap_is_gc_verbose();
  LOCAL_stack_overflows++;
  if (gc_verbose) {
#if  defined(YAPOR) || defined(THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% Stack Overflow %d\n", LOCAL_stack_overflows);
    fprintf(GLOBAL_stderr, "%%   Global: %8ld cells (%p-%p)\n", (unsigned long int)(H-(CELL *)LOCAL_GlobalBase),LOCAL_GlobalBase,H);
    fprintf(GLOBAL_stderr, "%%   Local:%8ld cells (%p-%p)\n", (unsigned long int)(LCL0-ASP),LCL0,ASP);
    fprintf(GLOBAL_stderr, "%%   Trail:%8ld cells (%p-%p)\n",
	       (unsigned long int)(TR-(tr_fr_ptr)LOCAL_TrailBase),LOCAL_TrailBase,TR);
    fprintf(GLOBAL_stderr, "%% Growing the stacks %ld bytes\n", size);
  }
  if (!execute_growstack(size, FALSE, FALSE, NULL, NULL, NULL PASS_REGS))
    return FALSE;
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_stack_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%%   took %g sec\n", (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% Total of %g sec expanding stacks \n", (double)LOCAL_total_stack_overflow_time/1000);
  }
  return TRUE;
}

/* Used by parser when we're short of stack space */
int
Yap_growstack_in_parser(tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep)
{
  CACHE_REGS
  UInt size;
  UInt start_growth_time, growth_time;
  int gc_verbose;

  LOCAL_PrologMode |= GrowStackMode;
  /* adjust to a multiple of 256) */
  size = AdjustPageSize((ADDR)LCL0-LOCAL_GlobalBase);
  LOCAL_ErrorMessage = NULL;
  start_growth_time = Yap_cputime();
  gc_verbose = Yap_is_gc_verbose();
  LOCAL_stack_overflows++;
  if (gc_verbose) {
#if  defined(YAPOR) || defined(THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% Stack Overflow %d\n", LOCAL_stack_overflows);
    fprintf(GLOBAL_stderr, "%%   Global: %8ld cells (%p-%p)\n", (unsigned long int)(H-(CELL *)LOCAL_GlobalBase),LOCAL_GlobalBase,H);
    fprintf(GLOBAL_stderr, "%%   Local:%8ld cells (%p-%p)\n", (unsigned long int)(LCL0-ASP),LCL0,ASP);
    fprintf(GLOBAL_stderr, "%%   Trail:%8ld cells (%p-%p)\n",
	       (unsigned long int)(TR-(tr_fr_ptr)LOCAL_TrailBase),LOCAL_TrailBase,TR);
    fprintf(GLOBAL_stderr, "%% Growing the stacks %ld bytes\n", (unsigned long int)size);
  }
  if (!execute_growstack(size, FALSE, TRUE, old_trp, tksp, vep PASS_REGS)) {
    LeaveGrowMode(GrowStackMode);
    return FALSE;
  }
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_stack_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%%   took %g sec\n", (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% Total of %g sec expanding stacks \n", (double)LOCAL_total_stack_overflow_time/1000);
  }
  LeaveGrowMode(GrowStackMode);
  return TRUE;
}

static int do_growtrail(long size, int contiguous_only, int in_parser, tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep USES_REGS)
{
  UInt start_growth_time = Yap_cputime(), growth_time;
  int gc_verbose = Yap_is_gc_verbose();
  long size0 = size;

#if USE_SYSTEM_MALLOC
  if (contiguous_only)
    return FALSE;
#endif
  /* at least 64K for trail */
  if (!size) 
    size = ((ADDR)TR-LOCAL_TrailBase);
  size *= 2;
  if (size < YAP_ALLOC_SIZE)
    size = YAP_ALLOC_SIZE;
  if (size > M2)
    size = M2;
  if (size < size0)
    size=size0;
  /* adjust to a multiple of 256) */
  size = AdjustPageSize(size);
  LOCAL_trail_overflows++;
  if (gc_verbose) {
#if defined(YAPOR) || defined(THREADS)
    fprintf(GLOBAL_stderr, "%% Worker Id %d:\n", worker_id);
#endif
    fprintf(GLOBAL_stderr, "%% Trail Overflow %d\n", LOCAL_trail_overflows);
#if USE_SYSTEM_MALLOC
    fprintf(GLOBAL_stderr, "%%  Heap: %8ld cells (%p-%p)\n", (unsigned long int)(H-(CELL *)LOCAL_GlobalBase),(CELL *)LOCAL_GlobalBase,H);
    fprintf(GLOBAL_stderr, "%%  Local:%8ld cells (%p-%p)\n", (unsigned long int)(LCL0-ASP),LCL0,ASP);
    fprintf(GLOBAL_stderr, "%%  Trail:%8ld cells (%p-%p)\n",
	       (unsigned long int)(TR-(tr_fr_ptr)LOCAL_TrailBase),LOCAL_TrailBase,TR);
#endif
    fprintf(GLOBAL_stderr, "%% growing the trail %ld bytes\n", size);
  }
  LOCAL_ErrorMessage = NULL;
  if (!GLOBAL_AllowTrailExpansion) {
    LOCAL_ErrorMessage = "Trail Overflow";
    return FALSE;
  }
#if USE_SYSTEM_MALLOC
  execute_growstack(size, TRUE, in_parser, old_trp, tksp, vep PASS_REGS);
#else
  YAPEnterCriticalSection();
  if (!Yap_ExtendWorkSpace(size)) {
    YAPLeaveCriticalSection();
    LOCAL_ErrorMessage = NULL;
    if (contiguous_only) {
      /* I can't expand in this case */
      LOCAL_trail_overflows--;
      return FALSE;
    }
    execute_growstack(size, TRUE, in_parser, old_trp, tksp, vep PASS_REGS);
  } else {
    if (in_parser) {
      LOCAL_TrDiff = LOCAL_LDiff = LOCAL_GDiff = LOCAL_BaseDiff = LOCAL_DelayDiff = LOCAL_XDiff = LOCAL_HDiff = LOCAL_GDiff0 = 0;
      AdjustScannerStacks(tksp, vep PASS_REGS);
    }
    LOCAL_TrailTop += size;
    CurrentTrailTop = (tr_fr_ptr)(LOCAL_TrailTop-MinTrailGap);
    YAPLeaveCriticalSection();
  }
#endif
  growth_time = Yap_cputime()-start_growth_time;
  LOCAL_total_trail_overflow_time += growth_time;
  if (gc_verbose) {
    fprintf(GLOBAL_stderr, "%%  took %g sec\n", (double)growth_time/1000);
    fprintf(GLOBAL_stderr, "%% Total of %g sec expanding trail \n", (double)LOCAL_total_trail_overflow_time/1000);
  }
  LOCK(LOCAL_SignalLock);
  if (LOCAL_ActiveSignals == YAP_TROVF_SIGNAL) {
    CreepFlag = CalculateStackGap();
  }
  LOCAL_ActiveSignals &= ~YAP_TROVF_SIGNAL;
  UNLOCK(LOCAL_SignalLock);
  return TRUE;
}


/* Used by do_goal() when we're short of stack space */
int
Yap_growtrail(long size, int contiguous_only)
{ 
  CACHE_REGS
    return do_growtrail(size, contiguous_only, FALSE, NULL, NULL, NULL PASS_REGS);
}

int
Yap_growtrail_in_parser(tr_fr_ptr *old_trp, TokEntry **tksp, VarEntry **vep)
{
  CACHE_REGS
  return do_growtrail(0, FALSE, TRUE, old_trp, tksp, vep PASS_REGS);
}

CELL **
Yap_shift_visit(CELL **to_visit, CELL ***to_visit_maxp, CELL ***to_visit_base)
{
  CACHE_REGS
  CELL **to_visit_max = *to_visit_maxp;
  /* relative position of top of stack */
  Int off = (ADDR)to_visit-AuxBase;
  /* how much space the top stack was using */
  Int sz = AuxTop - (ADDR)to_visit_max;
  /* how much space the bottom stack was using */
  Int szlow = (ADDR)to_visit_max-AuxBase;
  /* original size for AuxSpace */
  Int totalsz0 = AuxTop - AuxBase; /* totalsz0 == szlow+sz */
  /* new size for AuxSpace */
  Int totalsz;
  /* how much we grow */
  Int dsz; /* totalsz == szlow+dsz+sz */
  char *newb = Yap_ExpandPreAllocCodeSpace(0, NULL, FALSE);

  if (newb == NULL) {
    Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"cannot allocate temporary space for unification (%p)", to_visit);       
    return to_visit;
  }
  /* check new size */
  totalsz  = AuxTop-AuxBase;
  /* how much we grew */
  dsz = totalsz-totalsz0;
  if (dsz == 0) {
    Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"cannot allocate temporary space for unification (%p)", to_visit);       
    return to_visit;
  }
  /* copy whole block to end */
  cpcellsd((CELL *)(newb+(dsz+szlow)), (CELL *)(newb+szlow), sz/sizeof(CELL));
  /* base pointer is block start */
  *to_visit_maxp = (CELL **)(newb+szlow);
  /* base pointer is block start */
  if (to_visit_base)
    *to_visit_base = (CELL **)AuxSp;
  /* current top is originall diff + diff size */
  return (CELL **)(newb+(off+dsz));
}

static Int
p_inform_trail_overflows( USES_REGS1 )
{
  Term tn = MkIntTerm(LOCAL_trail_overflows);
  Term tt = MkIntegerTerm(LOCAL_total_trail_overflow_time);
 
  return(Yap_unify(tn, ARG1) && Yap_unify(tt, ARG2));
}

/* :- grow_heap(Size) */
static Int
p_growheap( USES_REGS1 )
{
  Int             diff;
  Term t1 = Deref(ARG1);

  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR, t1, "grow_heap/1");
    return(FALSE);
  } else if (!IsIntTerm(t1)) {
    Yap_Error(TYPE_ERROR_INTEGER, t1, "grow_heap/1");
    return(FALSE);
  }
  diff = IntOfTerm(t1);
  if (diff < 0) {
    Yap_Error(DOMAIN_ERROR_NOT_LESS_THAN_ZERO, t1, "grow_heap/1");
  }
  return(static_growheap(diff, FALSE, NULL, NULL, NULL, NULL PASS_REGS));
}

static Int
p_inform_heap_overflows( USES_REGS1 )
{
  Term tn = MkIntTerm(LOCAL_heap_overflows);
  Term tt = MkIntegerTerm(LOCAL_total_heap_overflow_time);
 
  return(Yap_unify(tn, ARG1) && Yap_unify(tt, ARG2));
}

#if defined(YAPOR_THREADS)
void
Yap_CopyThreadStacks(int worker_q, int worker_p, int incremental)
{
  CACHE_REGS
  Int size;

  /* make sure both stacks have same size */
  Int p_size = REMOTE_ThreadHandle(worker_p).ssize+REMOTE_ThreadHandle(worker_p).tsize;
  Int q_size = REMOTE_ThreadHandle(worker_q).ssize+REMOTE_ThreadHandle(worker_q).tsize;
  if (p_size != q_size) {
    if (!(REMOTE_ThreadHandle(worker_q).stack_address = realloc(REMOTE_ThreadHandle(worker_q).stack_address,p_size*K1))) {
      exit(1);
    }
  }
  REMOTE_ThreadHandle(worker_q).ssize = REMOTE_ThreadHandle(worker_p).ssize;
  REMOTE_ThreadHandle(worker_q).tsize = REMOTE_ThreadHandle(worker_p).tsize;
  /* compute offset indicators */
  LOCAL_GlobalBase = REMOTE_GlobalBase(worker_p);
  LOCAL_LocalBase = REMOTE_LocalBase(worker_p);
  LOCAL_TrailBase = REMOTE_TrailBase(worker_p);
  LOCAL_TrailTop = REMOTE_TrailTop(worker_p);
  CurrentTrailTop = (tr_fr_ptr)(LOCAL_TrailTop-MinTrailGap);
  size = REMOTE_ThreadHandle(worker_q).stack_address-REMOTE_ThreadHandle(worker_p).stack_address;
  LOCAL_TrDiff = LOCAL_LDiff = LOCAL_GDiff = LOCAL_GDiff0 = LOCAL_DelayDiff = LOCAL_BaseDiff = size;
  LOCAL_XDiff = LOCAL_HDiff = 0;
  LOCAL_GSplit = NULL;
  H = REMOTE_ThreadHandle(worker_p).current_yaam_regs->H_;
  H0 = REMOTE_ThreadHandle(worker_p).current_yaam_regs->H0_;
  B = REMOTE_ThreadHandle(worker_p).current_yaam_regs->B_;
  ENV = REMOTE_ThreadHandle(worker_p).current_yaam_regs->ENV_;
  YENV = REMOTE_ThreadHandle(worker_p).current_yaam_regs->YENV_;
  ASP = REMOTE_ThreadHandle(worker_p).current_yaam_regs->ASP_;
  TR = REMOTE_ThreadHandle(worker_p).current_yaam_regs->TR_;
  if (ASP > CellPtr(B))
    ASP = CellPtr(B);
  LCL0 = REMOTE_ThreadHandle(worker_p).current_yaam_regs->LCL0_;
#ifdef CUT_C
  Yap_REGS.CUT_C_TOP = REMOTE_ThreadHandle(worker_p).current_yaam_regs->CUT_C_TOP;
#endif
  LOCAL_DynamicArrays = NULL;
  LOCAL_StaticArrays = NULL;
  LOCAL_GlobalVariables = NULL;
  SetHeapRegs(TRUE PASS_REGS);
  if (incremental) {
    IncrementalCopyStacksFromWorker( PASS_REGS1 );
    LOCAL_start_global_copy = 
      (CELL)PtoGloAdjust((CELL *)LOCAL_start_global_copy);
    LOCAL_end_global_copy = 
      (CELL)PtoGloAdjust((CELL *)LOCAL_end_global_copy);
    LOCAL_start_local_copy = 
      (CELL)PtoLocAdjust((CELL *)LOCAL_start_local_copy);
    LOCAL_end_local_copy = 
      (CELL)PtoLocAdjust((CELL *)LOCAL_end_local_copy);
    LOCAL_start_trail_copy = 
      (CELL)PtoTRAdjust((tr_fr_ptr)LOCAL_start_trail_copy);
    LOCAL_end_trail_copy = 
      (CELL)PtoTRAdjust((tr_fr_ptr)LOCAL_end_trail_copy);
    AdjustStacksAndTrail(0, STACK_INCREMENTAL_COPYING PASS_REGS);
    RestoreTrail(worker_p PASS_REGS);
    TR = (tr_fr_ptr) LOCAL_end_trail_copy;
  } else {
    CopyLocalAndTrail( PASS_REGS1 );
    MoveGlobal( PASS_REGS1 );
    AdjustStacksAndTrail(0, STACK_COPYING PASS_REGS);
  }
}
#endif

/* :- grow_stack(Size) */
static Int
p_growstack( USES_REGS1 )
{
  Int             diff;
  Term t1 = Deref(ARG1);

  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR, t1, "grow_stack/1");
    return(FALSE);
  } else if (!IsIntTerm(t1)) {
    Yap_Error(TYPE_ERROR_INTEGER, t1, "grow_stack/1");
    return(FALSE);
  }
  diff = IntOfTerm(t1);
  if (diff < 0) {
    Yap_Error(DOMAIN_ERROR_NOT_LESS_THAN_ZERO, t1, "grow_stack/1");
  }
  return(growstack(diff PASS_REGS));
}

static Int
p_inform_stack_overflows( USES_REGS1 )
{				/*  */
  Term tn = MkIntTerm(LOCAL_stack_overflows);
  Term tt = MkIntegerTerm(LOCAL_total_stack_overflow_time);
 
  return(Yap_unify(tn, ARG1) && Yap_unify(tt, ARG2));

}

Int
Yap_total_stack_shift_time(void)
{
  CACHE_REGS
  return(LOCAL_total_heap_overflow_time+
	 LOCAL_total_stack_overflow_time+
	 LOCAL_total_trail_overflow_time);
}

void
Yap_InitGrowPreds(void)
{
  Yap_InitCPred("$grow_heap", 1, p_growheap, SafePredFlag);
  Yap_InitCPred("$grow_stack", 1, p_growstack, SafePredFlag);
  Yap_InitCPred("$inform_trail_overflows", 2, p_inform_trail_overflows, SafePredFlag);
  Yap_InitCPred("$inform_heap_overflows", 2, p_inform_heap_overflows, SafePredFlag);
  Yap_InitCPred("$inform_stack_overflows", 2, p_inform_stack_overflows, SafePredFlag);
  Yap_init_gc();
  Yap_init_agc();
}
