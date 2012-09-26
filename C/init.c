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
* File:		init.c							 *
* Last rev:								 *
* mods:									 *
* comments:	initializing a prolog session				 *
*									 *
*************************************************************************/
#ifdef SCCS
static char     SccsId[] = "%W% %G%";
#endif

/*
 * The code from this file is used to initialize the environment for prolog 
 *
 */

#include <stdlib.h>
#include "Yap.h"
#include "yapio.h"
#include "alloc.h"
#include "clause.h"
#include "Foreign.h"
#include "SWI-Prolog.h"

#ifdef LOW_LEVEL_TRACER
#include "tracer.h"
#endif
#ifdef YAPOR
#ifdef YAPOR_COW
#include <signal.h>
#endif /* YAPOR_COW */
#include "or.macros.h"
#endif /* YAPOR */
#if defined(YAPOR) || defined(TABLING)
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#endif	/* YAPOR || TABLING */
#if HAVE_STRING_H
#include <string.h>
#endif

#ifdef DEBUG

#define	LOGFILE	"logfile"

int  Yap_output_msg = FALSE;

#ifdef MACC
STATIC_PROTO(void  InTTYLine, (char *));
#endif
#endif
STATIC_PROTO(void  SetOp, (int, int, char *, Term));
STATIC_PROTO(void  InitOps, (void));
STATIC_PROTO(void  InitDebug, (void));
#ifdef CUT_C
STATIC_PROTO(void  CleanBack, (PredEntry *, CPredicate, CPredicate, CPredicate));
#else
STATIC_PROTO(void  CleanBack, (PredEntry *, CPredicate, CPredicate));
#endif
STATIC_PROTO(void  InitStdPreds,(void));
STATIC_PROTO(void  InitFlags, (void));
STATIC_PROTO(void  InitCodes, (void));
STATIC_PROTO(void  InitVersion, (void));
STD_PROTO(void  exit, (int));
static void InitWorker(int wid);


/**************	YAP PROLOG GLOBAL VARIABLES *************************/

/************* variables related to memory allocation ***************/
ADDR Yap_HeapBase;

/**************	declarations local to init.c ************************/
static char    *optypes[] =
{"", "xfx", "xfy", "yfx", "xf", "yf", "fx", "fy"};

/* OS page size for memory allocation */
int Yap_page_size;

#if USE_THREADED_CODE
/* easy access to instruction opcodes */
void **Yap_ABSMI_OPCODES;
#endif

#if DEBUG
#if COROUTINING
int  Yap_Portray_delays = FALSE;
#endif
#endif

#define	xfx	1
#define	xfy	2
#define	yfx	3
#define	xf	4
#define	yf	5
#define	fx	6
#define	fy	7

int
Yap_IsOpType(char *type)
{
  int i;

  for (i = 1; i <= 7; ++i)
    if (strcmp(type, optypes[i]) == 0)
      break;
  return (i <= 7);
}

static int 
OpDec(int p, char *type, Atom a, Term m)
{
  int             i;
  AtomEntry      *ae = RepAtom(a);
  OpEntry        *info;

  if (m == TermProlog)
    m = PROLOG_MODULE;
  else if (m == USER_MODULE)
    m = PROLOG_MODULE;
  for (i = 1; i <= 7; ++i)
    if (strcmp(type, optypes[i]) == 0)
      break;
  if (i > 7) {
    Yap_Error(DOMAIN_ERROR_OPERATOR_SPECIFIER,MkAtomTerm(Yap_LookupAtom(type)),"op/3");
    return(FALSE);
  }
  if (p) {
    if (i == 1 || i == 2 || i == 4)
      p |= DcrlpFlag;
    if (i == 1 || i == 3 || i == 6)
      p |= DcrrpFlag;
  }
  WRITE_LOCK(ae->ARWLock);
  info = Yap_GetOpPropForAModuleHavingALock(ae, m);
  if (EndOfPAEntr(info)) {
    info = (OpEntry *) Yap_AllocAtomSpace(sizeof(OpEntry));
    info->KindOfPE = Ord(OpProperty);
    info->OpModule = m;
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
  } else {
    WRITE_LOCK(info->OpRWLock);
    WRITE_UNLOCK(ae->ARWLock);
  }
  if (i <= 3) {
    if (yap_flags[STRICT_ISO_FLAG] && 
	info->Posfix != 0) /* there is a posfix operator */ {
      /* ISO dictates */
      WRITE_UNLOCK(info->OpRWLock);
      Yap_Error(PERMISSION_ERROR_CREATE_OPERATOR,MkAtomTerm(a),"op/3");
      return FALSE;
    }
    info->Infix = p;
  } else if (i <= 5) {
    if (yap_flags[STRICT_ISO_FLAG] && 
	info->Infix != 0) /* there is an infix operator */ {
      /* ISO dictates */
      WRITE_UNLOCK(info->OpRWLock);
      Yap_Error(PERMISSION_ERROR_CREATE_OPERATOR,MkAtomTerm(a),"op/3");
      return FALSE;
    }
    info->Posfix = p;
  } else {
    info->Prefix = p;
  }
  WRITE_UNLOCK(info->OpRWLock);
  return (TRUE);
}

int 
Yap_OpDec(int p, char *type, Atom a, Term m)
{
  return(OpDec(p,type,a,m));
}

static void 
SetOp(int p, int type, char *at, Term m)
{
#ifdef DEBUG
  if (GLOBAL_Option[5])
    fprintf(stderr,"[setop %d %s %s]\n", p, optypes[type], at);
#endif
  OpDec(p, optypes[type], Yap_LookupAtom(at), m);
}

/* Gets the info about an operator in a prop */
Atom 
Yap_GetOp(OpEntry *pp, int *prio, int fix)
{
  int             n;
  SMALLUNSGN      p;

  if (fix == 0) {
    p = pp->Prefix;
    if (p & DcrrpFlag)
      n = 6, *prio = (p ^ DcrrpFlag);
    else
      n = 7, *prio = p;
  } else if (fix == 1) {
    p = pp->Posfix;
    if (p & DcrlpFlag)
      n = 4, *prio = (p ^ DcrlpFlag);
    else
      n = 5, *prio = p;
  } else {
    p = pp->Infix;
    if ((p & DcrrpFlag) && (p & DcrlpFlag))
      n = 1, *prio = (p ^ (DcrrpFlag | DcrlpFlag));
    else if (p & DcrrpFlag)
      n = 3, *prio = (p ^ DcrrpFlag);
    else if (p & DcrlpFlag)
      n = 2, *prio = (p ^ DcrlpFlag);
    else
      n = 4, *prio = p;
  }
  return Yap_LookupAtom(optypes[n]);
}

typedef struct OPSTRUCT {
	char           *opName;
	short int       opType, opPrio;
}               Opdef;

static Opdef    Ops[] = {
  {":-", xfx, 1200},
  {"-->", xfx, 1200},
  {"?-", fx, 1200},
  {":-", fx, 1200},
  {"dynamic", fx, 1150},
  {"thread_local", fx, 1150},
  {"initialization", fx, 1150},
  {"volatile", fx, 1150},
  {"mode", fx, 1150},
  {"public", fx, 1150},
  {"multifile", fx, 1150},
  {"meta_predicate", fx, 1150},
  {"module_transparent", fx, 1150},
  {"discontiguous", fx, 1150},
#ifdef YAPOR
  {"sequential", fx, 1150},
#endif /* YAPOR */
#ifdef TABLING
  {"table", fx, 1150},
#endif /* TABLING */
#ifndef UNCUTABLE
  {"uncutable", fx, 1150},
#endif /*UNCUTABLE ceh:*/
  {"|", xfy, 1105},
  {";", xfy, 1100},
  /*  {";", yf, 1100}, not allowed in ISO */
  {"->", xfy, 1050},
  {"*->", xfy, 1050},
  {",", xfy, 1000},
  {".", xfy, 999},
  {"\\+", fy, 900},
  {"not", fy, 900},
  {"=", xfx, 700},
  {"\\=", xfx, 700},
  {"is", xfx, 700},
  {"=..", xfx, 700},
  {"==", xfx, 700},
  {"\\==", xfx, 700},
  {"@<", xfx, 700},
  {"@>", xfx, 700},
  {"@=<", xfx, 700},
  {"@>=", xfx, 700},
  {"=@=", xfx, 700},
  {"\\=@=", xfx, 700},
  {"=:=", xfx, 700},
  {"=\\=", xfx, 700},
  {"<", xfx, 700},
  {">", xfx, 700},
  {"=<", xfx, 700},
  {">=", xfx, 700},
  {"as", xfx, 600},
  {":", xfy, 600},
  {"+", yfx, 500},
  {"-", yfx, 500},
  {"/\\", yfx, 500},
  {"\\/", yfx, 500},
  {"><", yfx, 500},
  {"#", yfx, 500},
  {"rdiv", yfx, 400},
  {"div", yfx, 400},
  {"xor", yfx, 400},
  {"*", yfx, 400},
  {"/", yfx, 400},
  {"//", yfx, 400},
  {"<<", yfx, 400},
  {">>", yfx, 400},
  {"mod", yfx, 400},
  {"rem", yfx, 400},
  {"+", fy, 200},
  {"-", fy, 200},
  {"\\", fy, 200},
  {"//", yfx, 400},
  {"**", xfx, 200},
  {"^", xfy, 200}
};

static void 
InitOps(void)
{
  unsigned int             i;
  for (i = 0; i < sizeof(Ops) / sizeof(*Ops); ++i)
    SetOp(Ops[i].opPrio, Ops[i].opType, Ops[i].opName, PROLOG_MODULE);
}

#ifdef DEBUG
#ifdef HAVE_ISATTY
#include <unistd.h>
#endif
#endif

static void 
InitDebug(void)
{
  Atom            At;
#ifdef DEBUG
  int i;

  for (i = 1; i < 20; ++i)
    GLOBAL_Option[i] = 0;
  if (Yap_output_msg) {
    char            ch;

#if HAVE_ISATTY
    if (!isatty (0)) {
      return;
    }
#endif
    fprintf(stderr,"absmi address:%p\n", FunAdr(Yap_absmi));
    fprintf(stderr,"Set	Trace Options:\n");
    fprintf(stderr,"a getch\t\tb token\t\tc Lookup\td LookupVar\ti Index\n");
    fprintf(stderr,"e SetOp\t\tf compile\tg icode\t\th boot\t\tl log\n");
    fprintf(stderr,"m Machine\t p parser\n");
    while ((ch = YP_putchar(YP_getchar())) != '\n')
      if (ch >= 'a' && ch <= 'z')
	GLOBAL_Option[ch - 'a' + 1] = 1;
    if (GLOBAL_Option['l' - 96]) {
      GLOBAL_logfile = fopen(LOGFILE, "w");
      if (GLOBAL_logfile == NULL) {
	fprintf(stderr,"can not open %s\n", LOGFILE);
	getchar();
	exit(0);
      }
      fprintf(stderr,"logging session to file 'logfile'\n");
#ifdef MAC
      Yap_SetTextFile(LOGFILE);
      lp = my_line;
      curfile = Nill;
#endif
    }
  }
#endif
  /* Set at full leash */
  At = AtomLeash;
  Yap_PutValue(At, MkIntTerm(15));
}

static UInt 
update_flags_from_prolog(UInt flags, PredEntry *pe)
{
  if (pe->PredFlags & MetaPredFlag)
    flags |= MetaPredFlag;
  if (pe->PredFlags & SourcePredFlag)
    flags |= SourcePredFlag;
  if (pe->PredFlags & SequentialPredFlag)
    flags |= SequentialPredFlag;
  if (pe->PredFlags & UDIPredFlag)
    flags |= UDIPredFlag;
  if (pe->PredFlags & ModuleTransparentPredFlag)
    flags |= ModuleTransparentPredFlag;
  return flags;
}

void 
Yap_InitCPred(char *Name, unsigned long int Arity, CPredicate code, UInt flags)
{
  CACHE_REGS
  Atom              atom = NIL;
  PredEntry        *pe = NULL;
  yamop            *p_code;
  StaticClause     *cl = NULL;
  Functor           f = NULL;

  while (atom == NIL) {
    if (flags & UserCPredFlag)
      atom = Yap_LookupAtom(Name);
    else
      atom = Yap_FullLookupAtom(Name);
    if (atom == NIL && !Yap_growheap(FALSE, 0L, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  }
  if (Arity)  {
    while (!f) {
      f = Yap_MkFunctor(atom,Arity);
      if (!f && !Yap_growheap(FALSE, 0L, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	return;
      }
    }
  }
  while (pe == NULL) {
    if (Arity)
      pe = RepPredProp(PredPropByFunc(f,CurrentModule));
    else
      pe = RepPredProp(PredPropByAtom(atom,CurrentModule));
    if (!pe && !Yap_growheap(FALSE, sizeof(PredEntry), NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  } 
  if (pe->PredFlags & CPredFlag) {
    /* already exists */
    flags = update_flags_from_prolog(flags, pe);
    cl = ClauseCodeToStaticClause(pe->CodeOfPred);
    if ((flags | StandardPredFlag | CPredFlag ) != pe->PredFlags) {
      Yap_ClauseSpace -= cl->ClSize;
      Yap_FreeCodeSpace((ADDR)cl);
      cl = NULL;
    }
  }
  p_code = cl->ClCode;
  while (!cl) {
    UInt sz;

    if (flags & SafePredFlag) {
      sz = (CELL)NEXTOP(NEXTOP(NEXTOP(p_code,Osbpp),p),l);
    } else {
      sz = (CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(NEXTOP(p_code,e),p),Osbpp),p),l);
    }
    cl = (StaticClause *)Yap_AllocCodeSpace(sz);
    if (!cl) {
      if (!Yap_growheap(FALSE, sz, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	return;
      }
    } else {
      Yap_ClauseSpace += sz;
      cl->ClFlags = StaticMask;
      cl->ClNext = NULL;
      cl->ClSize = sz;
      cl->usc.ClPred = pe;
      p_code = cl->ClCode;
    }
  }
  pe->CodeOfPred = p_code;
  pe->PredFlags = flags | StandardPredFlag | CPredFlag;
  pe->cs.f_code = code;
  if (!(flags & SafePredFlag)) {
    p_code->opc = Yap_opcode(_allocate);
    p_code = NEXTOP(p_code,e);
  }
  if (flags & UserCPredFlag)
    p_code->opc = Yap_opcode(_call_usercpred);
  else
    p_code->opc = Yap_opcode(_call_cpred);
  p_code->u.Osbpp.bmap = NULL;
  p_code->u.Osbpp.s = -Signed(RealEnvSize);
  p_code->u.Osbpp.p =
    p_code->u.Osbpp.p0 =
    pe;
  p_code = NEXTOP(p_code,Osbpp);
  if (!(flags & SafePredFlag)) {
    p_code->opc = Yap_opcode(_deallocate);
    p_code->u.p.p = pe;
    p_code = NEXTOP(p_code,p);
  }
  p_code->opc = Yap_opcode(_procceed);
  p_code->u.p.p = pe;
  p_code = NEXTOP(p_code,p);
  p_code->opc = Yap_opcode(_Ystop);
  p_code->u.l.l = cl->ClCode;
  pe->OpcodeOfPred = pe->CodeOfPred->opc;
}

void 
Yap_InitCmpPred(char *Name, unsigned long int Arity, CmpPredicate cmp_code, UInt flags)
{
  CACHE_REGS
  Atom              atom = NIL;
  PredEntry        *pe = NULL;
  yamop            *p_code = NULL;
  StaticClause     *cl = NULL; 
  Functor           f = NULL;

  while (atom == NIL) {
    atom = Yap_FullLookupAtom(Name);
    if (atom == NIL && !Yap_growheap(FALSE, 0L, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  }
  if (Arity)  {
    while (!f) {
      f = Yap_MkFunctor(atom,Arity);
      if (!f && !Yap_growheap(FALSE, 0L, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	return;
      }
    }
  }
  while (pe == NULL) {
    if (Arity)
      pe = RepPredProp(PredPropByFunc(f,CurrentModule));
    else
      pe = RepPredProp(PredPropByAtom(atom,CurrentModule));
    if (!pe && !Yap_growheap(FALSE, sizeof(PredEntry), NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  } 
  if (pe->PredFlags & CPredFlag) {
    flags = update_flags_from_prolog(flags, pe);
    p_code = pe->CodeOfPred;
    /* already exists */
  } else {
    while (!cl) {
      UInt sz = sizeof(StaticClause)+(CELL)NEXTOP(NEXTOP(NEXTOP(((yamop *)NULL),plxxs),p),l);
      cl = (StaticClause *)Yap_AllocCodeSpace(sz); 
      if (!cl) {
	if (!Yap_growheap(FALSE, sz, NULL)) {
	  Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	  return;
	}
      } else {
	Yap_ClauseSpace += sz;
	cl->ClFlags = StaticMask;
	cl->ClNext = NULL;
	cl->ClSize = sz;
	cl->usc.ClPred = pe;
	p_code = cl->ClCode;
	break;
      }
    }
  }
  pe->PredFlags = flags | StandardPredFlag | CPredFlag;
  pe->CodeOfPred = p_code;
  pe->cs.d_code = cmp_code;
  pe->ModuleOfPred = CurrentModule;
  p_code->opc = pe->OpcodeOfPred = Yap_opcode(_call_bfunc_xx);
  p_code->u.plxxs.p = pe;
  p_code->u.plxxs.f = FAILCODE;
  p_code->u.plxxs.x1 = Yap_emit_x(1);
  p_code->u.plxxs.x2 = Yap_emit_x(2);
  p_code->u.plxxs.flags = Yap_compile_cmp_flags(pe);
  p_code = NEXTOP(p_code,plxxs);
  p_code->opc = Yap_opcode(_procceed);
  p_code->u.p.p = pe;
  p_code = NEXTOP(p_code,p);
  p_code->opc = Yap_opcode(_Ystop);
  p_code->u.l.l = cl->ClCode;
}

void 
Yap_InitAsmPred(char *Name,  unsigned long int Arity, int code, CPredicate def, UInt flags)
{
  CACHE_REGS
  Atom            atom = NIL;
  PredEntry      *pe = NULL;
  Functor           f = NULL;
	
  while (atom == NIL) {
    atom = Yap_FullLookupAtom(Name);
    if (atom == NIL && !Yap_growheap(FALSE, 0L, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  }
  if (Arity)  {
    while (!f) {
      f = Yap_MkFunctor(atom,Arity);
      if (!f && !Yap_growheap(FALSE, 0L, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	return;
      }
    }
  }
  while (pe == NULL) {
    if (Arity)
      pe = RepPredProp(PredPropByFunc(f,CurrentModule));
    else
      pe = RepPredProp(PredPropByAtom(atom,CurrentModule));
    if (!pe && !Yap_growheap(FALSE, sizeof(PredEntry), NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  } 
  flags |= AsmPredFlag | StandardPredFlag | (code);
  if (pe->PredFlags & AsmPredFlag) {
    flags = update_flags_from_prolog(flags, pe);
    /* already exists */
  }
  pe->PredFlags = flags;
  pe->cs.f_code =  def;
  pe->ModuleOfPred = CurrentModule;
  if (def != NULL) {
    yamop      *p_code = ((StaticClause *)NULL)->ClCode;
    StaticClause     *cl;

    if (pe->CodeOfPred == (yamop *)(&(pe->OpcodeOfPred))) {
      if (flags & SafePredFlag) {
	cl = (StaticClause *)Yap_AllocCodeSpace((CELL)NEXTOP(NEXTOP(NEXTOP(((yamop *)p_code),Osbpp),p),l));
      } else {
	cl = (StaticClause *)Yap_AllocCodeSpace((CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(NEXTOP(((yamop *)p_code),e),Osbpp),p),p),l));
      }	
      if (!cl) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"No Heap Space in InitAsmPred");
	return;
      }
      Yap_ClauseSpace += (CELL)NEXTOP(NEXTOP(NEXTOP(((yamop *)p_code),Osbpp),p),l);
    } else {
      cl = ClauseCodeToStaticClause(pe->CodeOfPred);
    }
    cl->ClFlags = StaticMask;
    cl->ClNext = NULL;
    if (flags & SafePredFlag) {
      cl->ClSize = (CELL)NEXTOP(NEXTOP(NEXTOP(((yamop *)p_code),Osbpp),e),e);
    } else {
      cl->ClSize = (CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(NEXTOP(((yamop *)p_code),e),Osbpp),p),e),e);
    }
    cl->usc.ClPred = pe;
    p_code = cl->ClCode;
    pe->CodeOfPred = p_code;
    if (!(flags & SafePredFlag)) {
      p_code->opc = Yap_opcode(_allocate);
      p_code = NEXTOP(p_code,e);
    }
    p_code->opc = Yap_opcode(_call_cpred);
    p_code->u.Osbpp.bmap = NULL;
    p_code->u.Osbpp.s = -Signed(RealEnvSize);
    p_code->u.Osbpp.p = p_code->u.Osbpp.p0 = pe;
    p_code = NEXTOP(p_code,Osbpp);
    if (!(flags & SafePredFlag)) {
      p_code->opc = Yap_opcode(_deallocate);
      p_code->u.p.p = pe;
      p_code = NEXTOP(p_code,p);
    }
    p_code->opc = Yap_opcode(_procceed);
    p_code->u.p.p = pe;
    p_code = NEXTOP(p_code,p);
    p_code->opc = Yap_opcode(_Ystop);
    p_code->u.l.l = cl->ClCode;
    pe->OpcodeOfPred = pe->CodeOfPred->opc;
  } else {
    pe->OpcodeOfPred = Yap_opcode(_undef_p);
    pe->CodeOfPred =  (yamop *)(&(pe->OpcodeOfPred)); 
  }
}


static void 
#ifdef CUT_C
CleanBack(PredEntry *pe, CPredicate Start, CPredicate Cont, CPredicate Cut)
#else
CleanBack(PredEntry *pe, CPredicate Start, CPredicate Cont)
#endif
{
  yamop   *code;
  if (pe->cs.p_code.FirstClause != pe->cs.p_code.LastClause ||
      pe->cs.p_code.TrueCodeOfPred != pe->cs.p_code.FirstClause ||
      pe->CodeOfPred != pe->cs.p_code.FirstClause) {
    Yap_Error(SYSTEM_ERROR,TermNil,
	  "initiating a C Pred with backtracking");
    return;
  }
  code = (yamop *)(pe->cs.p_code.FirstClause);
  code->u.OtapFs.p = pe;
  if (pe->PredFlags & UserCPredFlag)
    code->opc = Yap_opcode(_try_userc);
  else
    code->opc = Yap_opcode(_try_c);
#ifdef YAPOR
  INIT_YAMOP_LTT(code, 2);
  PUT_YAMOP_SEQ(code);
#endif /* YAPOR */
  code->u.OtapFs.f = Start;
  code = NEXTOP(code,OtapFs);
  if (pe->PredFlags & UserCPredFlag)
    code->opc = Yap_opcode(_retry_userc);
  else
    code->opc = Yap_opcode(_retry_c);
#ifdef YAPOR
  INIT_YAMOP_LTT(code, 1);
  PUT_YAMOP_SEQ(code);
#endif /* YAPOR */
  code->u.OtapFs.f = Cont;
#ifdef CUT_C
  code = NEXTOP(code,OtapFs);
  if (pe->PredFlags & UserCPredFlag)
    code->opc = Yap_opcode(_cut_c);
  else
    code->opc = Yap_opcode(_cut_userc);
  code->u.OtapFs.p = pe;
  code->u.OtapFs.f = Cut;
#endif
}


#ifdef CUT_C
void 
Yap_InitCPredBack(char *Name, unsigned long int Arity,
		  unsigned int Extra, CPredicate Start,
		  CPredicate Cont, UInt flags){
  Yap_InitCPredBack_(Name,Arity,Extra,Start,Cont,NULL,flags);
}

void
Yap_InitCPredBackCut(char *Name, unsigned long int Arity,
		     unsigned int Extra, CPredicate Start,
		     CPredicate Cont,CPredicate Cut, UInt flags){
  Yap_InitCPredBack_(Name,Arity,Extra,Start,Cont,Cut,flags);
}
#else
Yap_InitCPredBackCut(char *Name, unsigned long int Arity,
		     unsigned int Extra, CPredicate Start,
		     CPredicate Cont,CPredicate Cut, UInt flags){
  Yap_InitCPredBack(Name,Arity,Extra,Start,Cont,flags);
}
#endif /* CUT_C */

void
#ifdef CUT_C 
Yap_InitCPredBack_(char *Name, unsigned long int Arity,
		  unsigned int Extra, CPredicate Start,
		  CPredicate Cont, CPredicate Cut, UInt flags)
#else
Yap_InitCPredBack(char *Name, unsigned long int Arity,
		  unsigned int Extra, CPredicate Start,
		  CPredicate Cont, UInt flags)
#endif 
{
  CACHE_REGS
  PredEntry      *pe = NULL;
  Atom            atom = NIL;
  Functor           f = NULL;

  while (atom == NIL) {
    atom = Yap_FullLookupAtom(Name);
    if (atom == NIL && !Yap_growheap(FALSE, 0L, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  }
  if (Arity)  {
    while (!f) {
      f = Yap_MkFunctor(atom,Arity);
      if (!f && !Yap_growheap(FALSE, 0L, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
	return;
      }
    }
  }
  while (pe == NULL) {
    if (Arity)
      pe = RepPredProp(PredPropByFunc(f,CurrentModule));
    else
      pe = RepPredProp(PredPropByAtom(atom,CurrentModule));
    if (!pe && !Yap_growheap(FALSE, sizeof(PredEntry), NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"while initialising %s", Name);
      return;
    }
  } 
  if (pe->cs.p_code.FirstClause != NIL)
    {
      flags = update_flags_from_prolog(flags, pe);      
#ifdef CUT_C
      CleanBack(pe, Start, Cont, Cut);
#else
      CleanBack(pe, Start, Cont);
#endif /*CUT_C*/
    }
  else {
    StaticClause *cl;
    yamop      *code = ((StaticClause *)NULL)->ClCode;
    if (flags &  UserCPredFlag) 
      pe->PredFlags = UserCPredFlag | BackCPredFlag| CompiledPredFlag | StandardPredFlag | flags;
    else
      pe->PredFlags = CompiledPredFlag | StandardPredFlag | BackCPredFlag;

#ifdef YAPOR
    pe->PredFlags |= SequentialPredFlag;
#endif /* YAPOR */
    
#ifdef CUT_C
    cl = (StaticClause *)Yap_AllocCodeSpace((CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),OtapFs),l));
#else
    cl = (StaticClause *)Yap_AllocCodeSpace((CELL)NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),l));
#endif
    
    if (cl == NULL) {
      Yap_Error(OUT_OF_HEAP_ERROR,TermNil,"No Heap Space in InitCPredBack");
      return;
    }
    cl->ClFlags = StaticMask;
    cl->ClNext = NULL;
#ifdef CUT_C
    Yap_ClauseSpace += (CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),OtapFs),l);
    cl->ClSize = 
      (CELL)NEXTOP(NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),OtapFs),e);
#else
    Yap_ClauseSpace += (CELL)NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),l);
    cl->ClSize = 
      (CELL)NEXTOP(NEXTOP(NEXTOP(code,OtapFs),OtapFs),e);
#endif
    cl->usc.ClPred = pe;

    code = cl->ClCode;
    pe->cs.p_code.TrueCodeOfPred = pe->CodeOfPred =
      pe->cs.p_code.FirstClause = pe->cs.p_code.LastClause = code;
    if (flags & UserCPredFlag)
      pe->OpcodeOfPred = code->opc = Yap_opcode(_try_userc);
    else
      pe->OpcodeOfPred = code->opc = Yap_opcode(_try_c);
    code->u.OtapFs.f = Start;
    code->u.OtapFs.p = pe;
    code->u.OtapFs.s = Arity;
    code->u.OtapFs.extra = Extra;
#ifdef YAPOR
    INIT_YAMOP_LTT(code, 2);
    PUT_YAMOP_SEQ(code);
#endif /* YAPOR */
    code = NEXTOP(code,OtapFs);
    if (flags & UserCPredFlag)
      code->opc = Yap_opcode(_retry_userc);
    else
      code->opc = Yap_opcode(_retry_c);
    code->u.OtapFs.f = Cont;
    code->u.OtapFs.p = pe;
    code->u.OtapFs.s = Arity;
    code->u.OtapFs.extra = Extra;
#ifdef YAPOR
    INIT_YAMOP_LTT(code, 1);
    PUT_YAMOP_SEQ(code);
#endif /* YAPOR */
    code = NEXTOP(code,OtapFs);
#ifdef CUT_C
    if (flags & UserCPredFlag)
      code->opc = Yap_opcode(_cut_userc);
    else
      code->opc = Yap_opcode(_cut_c);
    code->u.OtapFs.f = Cut;
    code->u.OtapFs.p = pe;
    code->u.OtapFs.s = Arity;
    code->u.OtapFs.extra = Extra;
    code = NEXTOP(code,OtapFs);
#endif /* CUT_C */
    code->opc = Yap_opcode(_Ystop);
    code->u.l.l = cl->ClCode;
  }
}


static void 
InitStdPreds(void)
{
  Yap_InitCPreds();
  Yap_InitBackCPreds();
}

static void
InitFlags(void)
{
  /* note that Yap_heap_regs must be set first */

#if USE_GMP
  yap_flags[YAP_INT_BOUNDED_FLAG] = 0;
#else
  yap_flags[YAP_INT_BOUNDED_FLAG] = 1;
#endif
  yap_flags[MAX_ARITY_FLAG] = -1;
  yap_flags[INTEGER_ROUNDING_FLAG] = 0;
  yap_flags[YAP_MAX_INTEGER_FLAG] = (Int)(~((CELL)1 << (sizeof(Int)*8-1)));
  yap_flags[YAP_MIN_INTEGER_FLAG] = (Int)(((CELL)1 << (sizeof(Int)*8-1)));
  yap_flags[CHAR_CONVERSION_FLAG] = 1;
  yap_flags[YAP_DOUBLE_QUOTES_FLAG] = 1;
  yap_flags[YAP_TO_CHARS_FLAG] = ISO_TO_CHARS;
  yap_flags[LANGUAGE_MODE_FLAG] = 0;
  yap_flags[STRICT_ISO_FLAG] = FALSE;
  yap_flags[SOURCE_MODE_FLAG] = FALSE;
  yap_flags[CHARACTER_ESCAPE_FLAG] = SICSTUS_CHARACTER_ESCAPES;
  yap_flags[WRITE_QUOTED_STRING_FLAG] = FALSE;
  /* we do not garantee safe assert in parallel mode */
  yap_flags[ALLOW_ASSERTING_STATIC_FLAG] = TRUE;
  yap_flags[GENERATE_DEBUG_INFO_FLAG] = TRUE;
  /* current default */
  yap_flags[INDEXING_MODE_FLAG] = INDEX_MODE_MULTI;
  yap_flags[TABLING_MODE_FLAG] = 0;
  yap_flags[QUIET_MODE_FLAG] = FALSE;
  yap_flags[INDEXING_TERM_DEPTH_FLAG] = 0L;
}

static void
InitPredHash(void)
{
  UInt i;

  PredHash = (PredEntry **)Yap_AllocAtomSpace(sizeof(PredEntry **) * PredHashInitialSize);
  PredHashTableSize = PredHashInitialSize;
  if (PredHash == NULL) {
    Yap_Error(FATAL_ERROR,MkIntTerm(0),"allocating initial predicate hash table");
  }
  for (i = 0; i < PredHashTableSize; ++i) {
    PredHash[i] = NULL;
  }
  INIT_RWLOCK(PredHashRWLock);
}

static void
InitEnvInst(yamop start[2], yamop **instp, op_numbers opc, PredEntry *pred)
{
  yamop *ipc = start;

  /* make it look like the instruction is preceeded by a call instruction */
  ipc->opc = Yap_opcode(_call);
  ipc->u.Osbpp.s = -Signed(RealEnvSize);
  ipc->u.Osbpp.bmap = NULL;
  ipc->u.Osbpp.p = pred;
  ipc->u.Osbpp.p0 = pred;
  ipc = NEXTOP(ipc, Osbpp);
  ipc->opc = Yap_opcode(opc);
  *instp = ipc;
}

static void
InitOtaplInst(yamop start[1], OPCODE opc, PredEntry *pe)
{
  yamop *ipc = start;

  /* this is a place holder, it should not really be used */
  ipc->opc = Yap_opcode(opc);
  ipc->u.Otapl.s = 0;
  ipc->u.Otapl.p = pe;
  ipc->u.Otapl.d = NULL;
#ifdef YAPOR
  INIT_YAMOP_LTT(ipc, 1);
#endif /* YAPOR */
#ifdef TABLING
  ipc->u.Otapl.te = NULL;
#endif /* TABLING */
}

static void 
InitDBErasedMarker(void)
{
  Yap_heap_regs->db_erased_marker =
    (DBRef)Yap_AllocCodeSpace(sizeof(DBStruct));
  Yap_LUClauseSpace += sizeof(DBStruct);
  Yap_heap_regs->db_erased_marker->id = FunctorDBRef;
  Yap_heap_regs->db_erased_marker->Flags = ErasedMask;
  Yap_heap_regs->db_erased_marker->Code = NULL;
  Yap_heap_regs->db_erased_marker->DBT.DBRefs = NULL;
  Yap_heap_regs->db_erased_marker->Parent = NULL;
}

static void 
InitLogDBErasedMarker(void)
{
  Yap_heap_regs->logdb_erased_marker =
    (LogUpdClause *)Yap_AllocCodeSpace(sizeof(LogUpdClause)+(UInt)NEXTOP((yamop*)NULL,e));
  Yap_LUClauseSpace += sizeof(LogUpdClause)+(UInt)NEXTOP((yamop*)NULL,e);
  Yap_heap_regs->logdb_erased_marker->Id = FunctorDBRef;
  Yap_heap_regs->logdb_erased_marker->ClFlags = ErasedMask|LogUpdMask;
  Yap_heap_regs->logdb_erased_marker->ClSource = NULL;
  Yap_heap_regs->logdb_erased_marker->ClRefCount = 0;
  Yap_heap_regs->logdb_erased_marker->ClPred = PredLogUpdClause;
  Yap_heap_regs->logdb_erased_marker->ClExt = NULL;
  Yap_heap_regs->logdb_erased_marker->ClPrev = NULL;
  Yap_heap_regs->logdb_erased_marker->ClNext = NULL;
  Yap_heap_regs->logdb_erased_marker->ClSize = (UInt)NEXTOP(((LogUpdClause *)NULL)->ClCode,e);
  Yap_heap_regs->logdb_erased_marker->ClCode->opc = Yap_opcode(_op_fail);
  INIT_CLREF_COUNT(Yap_heap_regs->logdb_erased_marker);
}

#define SWIAtomToAtom(X) SWI_Atoms[(X)>>1]

static void 
InitSWIAtoms(void)
{
  int i=0, j=0;
#include "iswiatoms.h"
  Yap_InitSWIHash();
}

static void 
InitAtoms(void)
{
  int i;
  AtomHashTableSize = MaxHash;
  HashChain = (AtomHashEntry *)Yap_AllocAtomSpace(sizeof(AtomHashEntry) * MaxHash);
  if (HashChain == NULL) {
    Yap_Error(FATAL_ERROR,MkIntTerm(0),"allocating initial atom table");
  }
  for (i = 0; i < MaxHash; ++i) {
    INIT_RWLOCK(HashChain[i].AERWLock);
    HashChain[i].Entry = NIL;
  }
  NOfAtoms = 0;
#if THREADS
  SF_STORE->AtFoundVar = Yap_LookupAtom("**");
  Yap_ReleaseAtom(AtomFoundVar);
  SF_STORE->AtFreeTerm = Yap_LookupAtom("?");
  Yap_ReleaseAtom(AtomFreeTerm);
  SF_STORE->AtNil = Yap_LookupAtom("[]");
  SF_STORE->AtDot = Yap_LookupAtom(".");
#else
  Yap_LookupAtomWithAddress("**",&(SF_STORE->AtFoundVar));
  Yap_ReleaseAtom(AtomFoundVar);
  Yap_LookupAtomWithAddress("?",&(SF_STORE->AtFreeTerm));
  Yap_ReleaseAtom(AtomFreeTerm);
  Yap_LookupAtomWithAddress("[]",&(SF_STORE->AtNil));
  Yap_LookupAtomWithAddress(".",&(SF_STORE->AtDot));
#endif
}

static void 
InitWideAtoms(void)
{
  int i;

  WideAtomHashTableSize = MaxWideHash;
  WideHashChain = (AtomHashEntry *)Yap_AllocAtomSpace(sizeof(AtomHashEntry) * MaxWideHash);
  if (WideHashChain == NULL) {
    Yap_Error(FATAL_ERROR,MkIntTerm(0),"allocating wide atom table");
  }
  for (i = 0; i < MaxWideHash; ++i) {
    INIT_RWLOCK(WideHashChain[i].AERWLock);
    WideHashChain[i].Entry = NIL;
  }
  NOfWideAtoms = 0;
}

static void 
InitInvisibleAtoms(void)
{
  /* initialise invisible chain */
  Yap_heap_regs->invisiblechain.Entry = NIL;
  INIT_RWLOCK(Yap_heap_regs->invisiblechain.AERWLock);
}


#ifdef YAPOR
void Yap_init_yapor_workers(void) {
  CACHE_REGS
  int proc;
#ifdef YAPOR_THREADS
  return;
#endif /* YAPOR_THREADS */
#ifdef YAPOR_COW
  GLOBAL_master_worker = getpid();
  if (GLOBAL_number_workers > 1) {
    int son;
    son = fork();
    if (son == -1)
      Yap_Error(FATAL_ERROR, TermNil, "fork error (Yap_init_yapor_workers)");
    if (son > 0) {
      /* I am the father, I must stay here and wait for my children to all die */
      struct sigaction sigact;
      sigact.sa_handler = SIG_DFL;
      sigemptyset(&sigact.sa_mask);
      sigact.sa_flags = SA_RESTART;
      sigaction(SIGINT, &sigact, NULL);
      pause();
      exit(0);
    } else
      GLOBAL_worker_pid(0) = getpid();
  }
#endif /* YAPOR_COW */
  for (proc = 1; proc < GLOBAL_number_workers; proc++) {
    int son;
    son = fork();
    if (son == -1)
      Yap_Error(FATAL_ERROR, TermNil, "fork error (Yap_init_yapor_workers)");
    if (son == 0) { 
      /* new worker */
      worker_id = proc;
      Yap_remap_yapor_memory();
      LOCAL = REMOTE(worker_id);
      memcpy(REMOTE(worker_id), REMOTE(0), sizeof(struct worker_local));
      InitWorker(worker_id);
      break;
    } else
      GLOBAL_worker_pid(proc) = son;
  }
}
#endif /* YAPOR */


#ifdef  THREADS
static void
InitThreadHandle(int wid)
{
  REMOTE_ThreadHandle(wid).in_use = FALSE;
  REMOTE_ThreadHandle(wid).zombie = FALSE;
  REMOTE_ThreadHandle(wid).local_preds = NULL;
#ifdef LOW_LEVEL_TRACER
  REMOTE_ThreadHandle(wid).thread_inst_count = 0LL;
#endif
  pthread_mutex_init(&(REMOTE_ThreadHandle(wid).tlock), NULL);  
  pthread_mutex_init(&(REMOTE_ThreadHandle(wid).tlock_status), NULL);  
  REMOTE_ThreadHandle(wid).tdetach = (CELL)0;
  REMOTE_ThreadHandle(wid).cmod = (CELL)0;
}

int
Yap_InitThread(int new_id)
{
  struct worker_local *new_s;
  if (new_id) {
    if (!(new_s = (struct worker_local *)calloc(sizeof(struct worker_local), 1)))
      return FALSE;
    Yap_local[new_id] = new_s;
  }
  InitWorker(new_id);
  return TRUE;
}
#endif

static void 
InitFirstWorkerThreadHandle(void)
{
#ifdef  THREADS
  CACHE_REGS
  LOCAL_ThreadHandle.id = 0;
  LOCAL_ThreadHandle.in_use = TRUE;
  LOCAL_ThreadHandle.default_yaam_regs = 
    &Yap_standard_regs;
  LOCAL_ThreadHandle.pthread_handle = pthread_self();
  pthread_mutex_init(&REMOTE_ThreadHandle(0).tlock, NULL);
  pthread_mutex_init(&REMOTE_ThreadHandle(0).tlock_status, NULL);
  LOCAL_ThreadHandle.tdetach = MkAtomTerm(AtomFalse);
#endif
}

static void
InitScratchPad(int wid)
{
  REMOTE_ScratchPad(wid).ptr = NULL;
  REMOTE_ScratchPad(wid).sz = SCRATCH_START_SIZE;
  REMOTE_ScratchPad(wid).msz = SCRATCH_START_SIZE;
}

void
Yap_CloseScratchPad(void)
{
  CACHE_REGS
  Yap_FreeCodeSpace(LOCAL_ScratchPad.ptr);
  LOCAL_ScratchPad.sz = SCRATCH_START_SIZE;
  LOCAL_ScratchPad.msz = SCRATCH_START_SIZE;
}

#include "iglobals.h"
#include "ilocals.h"


#if defined(YAPOR_COPY) || defined(YAPOR_COW) || defined(YAPOR_SBA)
struct global_data *Yap_global;
long Yap_worker_area_size;
#else
struct global_data Yap_Global;
#endif

#if defined(THREADS)
struct worker_local	*Yap_local[MAX_THREADS];
#elif defined(YAPOR)
struct worker_local	*Yap_local;
#else /* !THREADS && !YAPOR */
struct worker_local	Yap_local;
#endif

static void 
InitCodes(void)
{
  CACHE_REGS
#if THREADS
  int wid;
  for (wid = 1; wid < MAX_THREADS; wid++) {
    Yap_local[wid] = NULL;
  }
#endif
#include "ihstruct.h"
#if THREADS
  Yap_InitThread(0);
#endif /* THREADS */
  InitGlobal();
#if !THREADS
  InitWorker(0);
#endif /* THREADS */
  InitFirstWorkerThreadHandle();
  /* make sure no one else can use these two atoms */
  CurrentModule = 0;
  Yap_ReleaseAtom(AtomOfTerm(TermReFoundVar));
  /* make sure we have undefp defined */
  /* predicates can only be defined after this point */
  {
    /* make sure we know about the module predicate */
    PredEntry *modp = RepPredProp(PredPropByFunc(FunctorModule,PROLOG_MODULE));
    modp->PredFlags |= MetaPredFlag;
  }
#ifdef YAPOR
  Yap_heap_regs->getwork_code->u.Otapl.p = RepPredProp(PredPropByAtom(AtomGetwork, PROLOG_MODULE));
  Yap_heap_regs->getwork_seq_code->u.Otapl.p = RepPredProp(PredPropByAtom(AtomGetworkSeq, PROLOG_MODULE));
#endif /* YAPOR */

}


static void 
InitVersion(void)
{
  Yap_PutValue(AtomVersionNumber,
	       MkAtomTerm(Yap_LookupAtom(YAP_SVERSION)));
#if defined MYDDAS_MYSQL || defined MYDDAS_ODBC
  Yap_PutValue(AtomMyddasVersionName,
	       MkAtomTerm(Yap_LookupAtom(MYDDAS_VERSION)));
#endif  
}

void
Yap_InitWorkspace(UInt Heap, UInt Stack, UInt Trail, UInt Atts, UInt max_table_size, 
                  int n_workers, int sch_loop, int delay_load)
{
  CACHE_REGS
  int             i;

  /* initialise system stuff */
#if PUSH_REGS
#ifdef THREADS
  if (!(Yap_local[0] = (struct worker_local *)calloc(sizeof(struct worker_local), 1)))
    return;
  pthread_key_create(&Yap_yaamregs_key, NULL);
  pthread_setspecific(Yap_yaamregs_key, (const void *)&Yap_standard_regs);
  GLOBAL_master_thread = pthread_self();
#else
  /* In this case we need to initialise the abstract registers */
  Yap_regp = &Yap_standard_regs;
  /* the emulator will eventually copy them to its own local
     register array, but for now they exist */
#endif
#endif /* PUSH_REGS */

#ifdef THREADS
  Yap_regp = ((REGSTORE *)pthread_getspecific(Yap_yaamregs_key));
  LOCAL = REMOTE(0);
#endif /* THREADS */
#if defined(YAPOR_COPY) || defined(YAPOR_COW) || defined(YAPOR_SBA)
  LOCAL = REMOTE(0);
#endif /* YAPOR_COPY || YAPOR_COW || YAPOR_SBA */
  if (Heap < MinHeapSpace)
    Heap = MinHeapSpace;
  Heap = AdjustPageSize(Heap * K);
  /* sanity checking for data areas */
  if (Trail < MinTrailSpace)
    Trail = MinTrailSpace;
  Trail = AdjustPageSize(Trail * K);
  if (Stack < MinStackSpace)
    Stack = MinStackSpace;
  Stack = AdjustPageSize(Stack * K);
  if (!Atts)
    Atts = 2048*sizeof(CELL);
  else
    Atts = AdjustPageSize(Atts * K);
#if defined(YAPOR) || defined(THREADS)
  worker_id = 0;
#endif /* YAPOR || THREADS */
#ifdef YAPOR
  if (n_workers > MAX_WORKERS)
    Yap_Error(INTERNAL_ERROR, TermNil, "excessive number of workers");
#ifdef YAPOR_COPY
  INFORMATION_MESSAGE("YapOr: copy model with %d worker%s", n_workers, n_workers == 1 ? "":"s");
#elif YAPOR_COW
  INFORMATION_MESSAGE("YapOr: acow model with %d worker%s", n_workers, n_workers == 1 ? "":"s");
#elif YAPOR_SBA
  INFORMATION_MESSAGE("YapOr: sba model with %d worker%s", n_workers, n_workers == 1 ? "":"s");
#elif YAPOR_THREADS
  INFORMATION_MESSAGE("YapOr: threads model with %d worker%s", n_workers, n_workers == 1 ? "":"s");
#endif /* YAPOR_COPY - YAPOR_COW - YAPOR_SBA - YAPOR_THREADS */
#endif /* YAPOR */
#if defined(YAPOR_COPY) || defined(YAPOR_COW) || defined(YAPOR_SBA)
  Yap_init_yapor_stacks_memory(Trail, Heap, Stack+Atts, n_workers);
#else
  Yap_InitMemory(Trail, Heap, Stack+Atts);
#endif
#if defined(YAPOR) || defined(TABLING)
  Yap_init_global_optyap_data(max_table_size, n_workers, sch_loop, delay_load);
#endif /* YAPOR || TABLING */

  Yap_AttsSize = Atts;
  Yap_InitTime();
  /* InitAbsmi must be done before InitCodes */
  /* This must be done before initialising predicates */
  for (i = 0; i <= NUMBER_OF_YAP_FLAGS; i++) {
    yap_flags[i] = 0;
  }
#ifdef MPW
  Yap_InitAbsmi(REGS, FunctorList);
#else
  Yap_InitAbsmi();
#endif
  InitCodes();
  InitOps();
  InitDebug();
  InitVersion();
  Yap_InitSysPath();
  InitStdPreds();
  /* make sure tmp area is available */
  {
    Yap_ReleasePreAllocCodeSpace(Yap_PreAllocCodeSpace());
  }
}

int
Yap_HaltRegisterHook (HaltHookFunc f, void * env)
{
  struct halt_hook *h;

  if (!(h = (struct halt_hook *)Yap_AllocCodeSpace(sizeof(struct halt_hook))))
    return FALSE;
  h->environment = env;
  h->hook = f;
  LOCK(GLOBAL_BGL);
  h->next = GLOBAL_HaltHooks;
  GLOBAL_HaltHooks = h;
  UNLOCK(GLOBAL_BGL);
  return TRUE;
}

static void
run_halt_hooks(int code)
{
  struct halt_hook *hooke = GLOBAL_HaltHooks;

  while (hooke) {
    hooke->hook(code, hooke->environment);
    hooke = hooke->next;
  }
}

void
Yap_exit (int value)
{
  CACHE_REGS
  void closeFiles(int all);
#if defined(YAPOR_COPY) || defined(YAPOR_COW) || defined(YAPOR_SBA)
  Yap_unmap_yapor_memory();
#endif /* YAPOR_COPY || YAPOR_COW || YAPOR_SBA */

  if (! (LOCAL_PrologMode & BootMode) ) {
#ifdef LOW_PROF
    remove("PROFPREDS");
    remove("PROFILING");
#endif
#if defined MYDDAS_MYSQL || defined MYDDAS_ODBC
    Yap_MYDDAS_delete_all_myddas_structs();
#endif
    run_halt_hooks(value);
    Yap_ShutdownLoadForeign();
  }
  closeFiles(TRUE);
  exit(value);
}

