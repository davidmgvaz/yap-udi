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
* File:		save.c							 *
* Last rev:								 *
* mods:									 *
* comments:	saving and restoring a Prolog computation		 *
*									 *
*************************************************************************/
#ifdef SCCS
static char     SccsId[] = "@(#)save.c	1.3 3/15/90";
#endif

#include "SWI-Stream.h"
#if _MSC_VER || defined(__MINGW32__)
#include <windows.h>
#include <psapi.h>
#endif
#include "absmi.h"
#include "alloc.h"
#if USE_DL_MALLOC
#include "dlmalloc.h"
#endif
#include "yapio.h"
#include "sshift.h"
#include "Foreign.h"
#if HAVE_STRING_H
#include <string.h>
#endif
#if !HAVE_STRNCAT
#define strncat(X,Y,Z) strcat(X,Y)
#endif
#if !HAVE_STRNCPY
#define strncpy(X,Y,Z) strcpy(X,Y)
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "iopreds.h"

/*********  hack for accesing several kinds of terms. Should be cleaned **/

static char StartUpFile[] = "startup.yss";

static char end_msg[256] ="*** End of YAP saved state *****";

/* SWI IO, must be restarted after restore */
void initIO(void);

#ifdef DEBUG

/*
 * 
 #FOR DEBUGGING define DEBUG_RESTORE0 to check the file stuff,
 #define DEBUG_RESTORE1 to see if it is able to prepare the chain,
 #define DEBUG_RESTORE2 to see how things are going,
 #define DEBUG_RESTORE3 to check if the atom chain is still a working chain,
 * define DEBUG_RESTORE4 if you want to set the output for some
 * particular file,
 * define DEBUG_RESTORE5 if you want to see how the stacks are being
 * cleaned up,
 * define DEBUG_RESTORE6 if you want to follow the execution in 
 *
 * Also a file is defined where you can write things, by default stderr 
 *
 * Good Luck 
 */

#endif

STATIC_PROTO(int   myread, (int, char *, Int));
STATIC_PROTO(Int   mywrite, (int, char *, Int));
STATIC_PROTO(int   open_file, (char *, int));
STATIC_PROTO(int   close_file, (void));
STATIC_PROTO(Int   putout, (CELL));
STATIC_PROTO(Int   putcellptr, (CELL *));
STATIC_PROTO(CELL  get_cell, (void));
STATIC_PROTO(CELL  *get_cellptr, ( /* CELL * */ void));
STATIC_PROTO(int   put_info, (int, int CACHE_TYPE));
STATIC_PROTO(int   save_regs, (int CACHE_TYPE));
STATIC_PROTO(int   save_code_info, (void));
STATIC_PROTO(int   save_heap, (void));
STATIC_PROTO(int   save_stacks, (int CACHE_TYPE));
STATIC_PROTO(int   save_crc, (void));
STATIC_PROTO(Int   do_save, (int CACHE_TYPE));
STATIC_PROTO(Int   p_save2, ( CACHE_TYPE1 ));
STATIC_PROTO(Int   p_save_program, ( CACHE_TYPE1 ));
STATIC_PROTO(int   check_header, (CELL *, CELL *, CELL *, CELL * CACHE_TYPE));
STATIC_PROTO(int   get_heap_info, (CACHE_TYPE1));
STATIC_PROTO(int   get_regs, (int CACHE_TYPE));
STATIC_PROTO(int   get_insts, (OPCODE []));
STATIC_PROTO(int   get_hash, (void));
STATIC_PROTO(int   CopyCode, ( CACHE_TYPE1 ));
STATIC_PROTO(int   CopyStacks, ( CACHE_TYPE1 ));
STATIC_PROTO(int   get_coded, (int, OPCODE [] CACHE_TYPE));
STATIC_PROTO(void  restore_codes, (void));
STATIC_PROTO(void  RestoreDB, (DBEntry * CACHE_TYPE));
STATIC_PROTO(void  RestoreDBTerm, (DBTerm *, int CACHE_TYPE));
STATIC_PROTO(void  CleanClauses, (yamop *, yamop *,PredEntry * CACHE_TYPE));
STATIC_PROTO(void  rehash, (CELL *, int, int CACHE_TYPE));
STATIC_PROTO(void  CleanCode, (PredEntry * CACHE_TYPE));
STATIC_PROTO(void  RestoreEntries, (PropEntry *, int CACHE_TYPE));
STATIC_PROTO(void  RestoreFreeSpace, ( CACHE_TYPE1 ));
STATIC_PROTO(void  restore_heap, (void));
#ifdef DEBUG_RESTORE3
STATIC_PROTO(void  ShowAtoms, (void));
STATIC_PROTO(void  ShowEntries, (PropEntry *));
#endif
STATIC_PROTO(int   OpenRestore, (char *, char *, CELL *, CELL *, CELL *, CELL *, IOSTREAM **));
STATIC_PROTO(void  CloseRestore, (void));
#ifndef _WIN32
STATIC_PROTO(int  check_opcodes, (OPCODE []));
#endif
STATIC_PROTO(void  RestoreHeap, (OPCODE [] CACHE_TYPE));
STATIC_PROTO(Int  p_restore, ( CACHE_TYPE1 ));
STATIC_PROTO(void  restore_heap_regs, ( CACHE_TYPE1 ));
STATIC_PROTO(void  restore_regs, (int CACHE_TYPE));
#ifdef MACYAP
STATIC_PROTO(void NewFileInfo, (long, long));
extern int      DefVol;
#endif

#ifdef _WIN32
#if HAVE_IO_H
#include <io.h>
#endif
#endif

#ifdef LIGHT

#include <unix.h>
#include <strings.h>

void 
LightBug(char *);

static void 
LightBug(s)
	char           *s;
{
}

#endif				/* LIGHT */

static Int
do_system_error(yap_error_number etype, const char *msg)
{
  CACHE_REGS
#if HAVE_SNPRINTF
#if HAVE_STRERROR
  snprintf(LOCAL_ErrorSay,MAX_ERROR_MSG_SIZE,"%s (%s when reading %s)", msg, strerror(errno), LOCAL_FileNameBuf);
#else
  snprintf(LOCAL_ErrorSay,MAX_ERROR_MSG_SIZE,"%s, (system error %d when reading %s)",msg,errno,LOCAL_FileNameBuf);  
#endif
#else
#if HAVE_STRERROR
  sprintf(LOCAL_ErrorSay,"%s, (%s when reading %s)",msg,strerror(errno),LOCAL_FileNameBuf);
#else
  sprintf(LOCAL_ErrorSay,"%s, (system error %d when reading %s)",msg,errno,LOCAL_FileNameBuf);  
#endif
#endif
  LOCAL_ErrorMessage = LOCAL_ErrorSay;
  LOCAL_Error_TYPE = etype;
  return -1;
}


inline static
int myread(int fd, char *buffer, Int len) {
  ssize_t nread;

  while (len > 0) {
    nread = read(fd, buffer,  (int)len);
    if (nread < 1) {
      return do_system_error(PERMISSION_ERROR_INPUT_PAST_END_OF_STREAM,"bad read on saved state");
    }
    buffer += nread;
    len -= nread;
  }
  return len;
}

inline static
Int
mywrite(int fd, char *buff, Int len) {
  ssize_t nwritten;

  while (len > 0) {
    nwritten = write(fd, buff, (size_t)len);
    if (nwritten < 0) {
      return do_system_error(SYSTEM_ERROR,"bad write on saved state");
    }
    buff += nwritten;
    len -= nwritten;
  }
  return len;
}

#define FullSaved		1

/* Where the code was before */



typedef CELL   *CELLPOINTER;

static int      splfild = 0;

#ifdef DEBUG

#ifdef DEBUG_RESTORE4
static FILE    *errout;
#else
#define errout GLOBAL_stderr
#endif

#endif				/* DEBUG */

static Int      OldHeapUsed;

static CELL     which_save;

/* Open a file to read or to write */
static int 
open_file(char *my_file, int flag)
{
  int splfild;

#ifdef M_WILLIAMS
  if (flag & O_CREAT)
    splfild = creat(my_file, flag);
  else
    splfild = open(my_file, flag);
  if (splfild < 0) {
#else
#ifdef O_BINARY
#if _MSC_VER
    if ((splfild = _open(my_file, flag | O_BINARY), _S_IREAD | _S_IWRITE) < 0)
#else
    if ((splfild = open(my_file, flag | O_BINARY, 0775)) < 0)
#endif
#else  /* O_BINARY */
    if ((splfild = open(my_file, flag, 0755)) < 0)
#endif  /* O_BINARY */
#endif 	/* M_WILLIAMS */
      {
	splfild = 0;	/* We do not have an open file */
	return -1;
      }
#ifdef undf0
      fprintf(errout, "Opened file %s\n", my_file);
#endif
      return splfild;
}

static int 
close_file(void)
{
  if (splfild == 0)
    return 0;
  if (close(splfild) < 0)
    return do_system_error(SYSTEM_ERROR,"bad close on saved state");
  splfild = 0;
  return 1;
}

/* stores a cell in a file */
static Int 
putout(CELL l)
{
  return mywrite(splfild, (char *) &l, sizeof(CELL));
}

/* stores a pointer to a cell in a file */
static Int 
putcellptr(CELL *l)
{
  return mywrite(splfild, (char *) &l, sizeof(CELLPOINTER));
}

/* gets a cell from a file */
static CELL 
get_cell(void)
{
  CELL            l;
  myread(splfild, (char *) &l, Unsigned(sizeof(CELL)));
  return (l);
}

/* gets a cell from a file */
static CELL 
get_header_cell(void)
{
  CELL l;
  int count = 0, n;
  while (count < sizeof(CELL)) {
    if ((n = read(splfild, &l, sizeof(CELL)-count)) < 0) {
      do_system_error(PERMISSION_ERROR_INPUT_PAST_END_OF_STREAM,"failed to read saved state header");
      return 0L;
    }
    count += n;
  }
  return l;
}

/* gets a pointer to cell from a file */
static CELL    *
get_cellptr(void)
{
  CELL           *l;

  if (myread(splfild, (char *) &l, Unsigned(sizeof(CELLPOINTER))) < 0)
    return NULL;
  return (l);
}

/*
 * writes the header (at the moment YAPV*), info about what kind of saved
 * set, the work size, and the space ocuppied 
 */
static int 
put_info(int info, int mode USES_REGS)
{
  char     msg[256];

  sprintf(msg, "#!/bin/sh\nexec_dir=${YAPBINDIR:-%s}\nexec $exec_dir/yap $0 \"$@\"\n%cYAP-%s", YAP_BINDIR, 1, YAP_SVERSION);
  if (mywrite(splfild, msg, strlen(msg) + 1))
    return -1;
  if (putout(Unsigned(info)) < 0)
    return -1;
  /* say whether we just saved the heap or everything */
  if (putout(mode) < 0)
    return -1;
  /* current state of stacks, to be used by SavedInfo */
  /* space available in heap area */
  if (putout(Unsigned(LOCAL_GlobalBase)-Unsigned(Yap_HeapBase)) < 0)
    return -1;
  /* space available for stacks */
  if (putout(Unsigned(LOCAL_LocalBase)-Unsigned(LOCAL_GlobalBase)) < 0)
    return -1;
  /* space available for trail */
  if (putout(Unsigned(LOCAL_TrailTop)-Unsigned(LOCAL_TrailBase)) < 0)
    return -1;
  /* Space used in heap area */
  if (putout(Unsigned(HeapTop)-Unsigned(Yap_HeapBase)) < 0)
    return -1;
  /* Space used for local stack */
  if (putout(Unsigned(LCL0)-Unsigned(ASP)) < 0)
    return -1;
  /* Space used for global stack */
  if (putout(Unsigned(H) - Unsigned(LOCAL_GlobalBase)) < 0)
    return -1;
  /* Space used for trail */
  if (putout(Unsigned(TR) - Unsigned(LOCAL_TrailBase)) < 0)
    return -1;
  return 0;
}

static int
save_regs(int mode USES_REGS)
{
  /* save all registers */
  if (putout((CELL)compile_arrays) < 0)
    return -1;    
  if (mode == DO_EVERYTHING) {
    if (putcellptr((CELL *)CP) < 0)
      return -1;
    if (putcellptr(ENV) < 0)
      return -1;
    if (putcellptr(ASP) < 0)
      return -1;
    /* if (putout((CELL)N) < 0)
       return -1; */
    if (putcellptr(H0) < 0)
      return -1;
    if (putcellptr(LCL0) < 0)
      return -1;
    if (putcellptr(H) < 0)
      return -1;
    if (putcellptr(HB) < 0)
      return -1;
    if (putcellptr((CELL *)B) < 0)
      return -1;
    if (putcellptr((CELL *)TR) < 0)
      return -1;
    if (putcellptr(YENV) < 0)
      return -1;
    if (putcellptr(S) < 0)
      return -1;
    if (putcellptr((CELL *)P) < 0)
      return -1;
    if (putout(CreepFlag) < 0)
      return -1;
    if (putcellptr((CELL *)EX) < 0)
      return -1;
#if defined(YAPOR_SBA) || defined(TABLING)
    if (putcellptr(H_FZ) < 0)
      return -1;
    if (putcellptr((CELL *)B_FZ) < 0)
      return -1;
    if (putcellptr((CELL *)TR_FZ) < 0)
      return -1;
#endif /* YAPOR_SBA || TABLING */
  }
  if (putout(CurrentModule) < 0)
    return -1;
  if (mode == DO_EVERYTHING) {
#ifdef COROUTINING
    if (putout(LOCAL_WokenGoals) < 0)
      return -1;
#endif
#ifdef  DEPTH_LIMIT
    if (putout(DEPTH) < 0)
      return -1;
#endif
    if (putout(LOCAL_GcGeneration) < 0)
      return -1;
    if (putout(LOCAL_GcPhase) < 0)
      return -1;
    if (putout(LOCAL_GcCurrentPhase) < 0)
      return -1;
  }
  /* The operand base */
  if (putcellptr(CellPtr(XREGS)) < 0)
    return -1;
  if (putout(which_save) < 0)
    return -1;
  /* Now start by saving the code */
  /* the heap boundaries */
  if (putcellptr(CellPtr(Yap_HeapBase)) < 0)
    return -1;
  if (putcellptr(CellPtr(HeapTop)) < 0)
    return -1;
  /* and the space it ocuppies */
  if (putout(Unsigned(Yap_heap_regs->heap_used)) < 0)
    return -1;
  /* Then the start of the free code */
  if (putcellptr(CellPtr(FreeBlocks)) < 0)
    return -1;
  if (putcellptr(CellPtr(AuxBase)) < 0)
    return -1;
  if (putcellptr(AuxSp) < 0)
    return -1;
  if (putcellptr(CellPtr(AuxTop)) < 0)
    return -1;
  if (putcellptr(CellPtr(LOCAL_ScratchPad.ptr)) < 0)
    return -1;
  if (putout(LOCAL_ScratchPad.sz) < 0)
    return -1;
  if (putout(LOCAL_ScratchPad.msz) < 0)
    return -1;
  if (mode == DO_EVERYTHING) {
    /* put the old trail base, just in case it moves again */
    if (putout(ARG1) < 0)
      return -1;
    if (which_save == 2) {
      if (putout(ARG2) < 0)
	return -1;
    }
    if (putcellptr(CellPtr(LOCAL_TrailBase)) < 0)
      return -1;
  }
  return 0;
}

static int
save_code_info(void)
{

  /* First the instructions */
  {
    op_numbers i;

    OPCODE my_ops[_std_top+1];
    for (i = _Ystop; i <= _std_top; ++i)
      my_ops[i] = Yap_opcode(i);
    if (mywrite(splfild, (char *)my_ops, sizeof(OPCODE)*(_std_top+1)) < 0)
      return -1;
  }
  /* and the current character codes */
  if (mywrite(splfild, Yap_chtype, NUMBER_OF_CHARS) < 0)
    return -1;
  return 0;
}

static int
save_heap(void)
{
#ifdef USE_SYSTEM_MALLOC
  return -1;
#endif
  int j;
  /* Then save the whole heap */
  Yap_ResetConsultStack();
  j = Unsigned(HeapTop) - Unsigned(Yap_HeapBase);
  /* store 10 more cells because of the memory manager */
  if (mywrite(splfild, (char *) Yap_HeapBase, j) < 0)
    return -1;
  return 0;
}

static int
save_stacks(int mode USES_REGS)
{
  int j;
  
  switch (mode) {
  case DO_EVERYTHING:
    /* Now, go and save the state */
    /* Save the local stack */
    j = Unsigned(LCL0) - Unsigned(ASP);
    if (mywrite(splfild, (char *) ASP, j) < 0)
      return -1;
    /* Save the global stack */
    j = Unsigned(H) - Unsigned(LOCAL_GlobalBase);
    if (mywrite(splfild, (char *) LOCAL_GlobalBase, j) < 0)
      return -1;
    /* Save the trail */
    j = Unsigned(TR) - Unsigned(LOCAL_TrailBase);
    if (mywrite(splfild, (char *) LOCAL_TrailBase, j) < 0)
      return -1;
    break;
  case DO_ONLY_CODE:
    {
      tr_fr_ptr tr_ptr = TR; 
      while (tr_ptr != (tr_fr_ptr)LOCAL_TrailBase) {
	CELL val = TrailTerm(tr_ptr-1);
	if (IsVarTerm(val)) {
	  CELL *d1 = VarOfTerm(val);
	  if (d1 < (CELL *)HeapTop) {
	    if (putout(val) < 0)
	      return -1;
	  }
	} else if (IsPairTerm(val)) {
	  CELL *d1 = RepPair(val);
	  if (d1 < (CELL *)HeapTop) {
	    if (putout(val) < 0)
	      return -1;
	  }
	}
	tr_ptr--;
      }
    }
    if (putcellptr(NULL) < 0)
      return -1;
    break;
  }
  return 0;
}

static int
save_crc(void)
{
  /* Save a CRC */
  return mywrite(splfild, end_msg, 256);
}

static Int
do_save(int mode USES_REGS) {
  extern void Scleanup(void);
  Term t1 = Deref(ARG1);

  if (Yap_HoleSize) {
    Yap_Error(SYSTEM_ERROR,MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)),
	      "restore/1: address space has holes of size %ld, cannot save", (long int)Yap_HoleSize);
    return FALSE;
  }
  if (!Yap_GetName(LOCAL_FileNameBuf, YAP_FILENAME_MAX, t1)) {
    Yap_Error(TYPE_ERROR_LIST,t1,"save/1");
    return FALSE;
  }
  Scleanup();
  Yap_CloseStreams(TRUE);
  if ((splfild = open_file(LOCAL_FileNameBuf, O_WRONLY | O_CREAT)) < 0) {
    Yap_Error(SYSTEM_ERROR,MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)),
	  "restore/1, open(%s)", strerror(errno));
    return(FALSE);
  }
  if (put_info(FullSaved, mode PASS_REGS) < 0)
    return -1;
  if (save_regs(mode PASS_REGS) < 0)
    return -1;
  if (save_code_info() < 0)
    return -1;
  if (save_heap() < 0)
    return -1;
  if (save_stacks(mode PASS_REGS) < 0)
    return -1;
  if (save_crc() < 0)
    return -1;
  close_file();
  return (TRUE);
}

/* Saves a complete prolog environment */
static Int 
p_save2( USES_REGS1 )
{
  Int res;

  Term t;
#ifdef YAPOR
  if (GLOBAL_number_workers != 1) {
    Yap_Error(SYSTEM_ERROR,TermNil,
	       "cannot perform save: more than a worker/thread running");
    return(FALSE);
  }
#endif /* YAPOR */
#ifdef THREADS
  if (GLOBAL_NOfThreads != 1) {
    Yap_Error(SYSTEM_ERROR,TermNil,
	       "cannot perform save: more than a worker/thread running");
    return(FALSE);
  }
#endif /* THREADS */
  /* avoid double saves */
  if (IsNonVarTerm(t = Deref(ARG2)))
    return TRUE;
  if (!Yap_unify(ARG2,MkIntTerm(1)))
    return FALSE;
  which_save = 2;
  Yap_StartSlots( PASS_REGS1 );
  res = do_save(DO_EVERYTHING PASS_REGS);
  Yap_CloseSlots( PASS_REGS1 );
  return res;
}

/* Just save the program, not the stacks */
static Int 
p_save_program( USES_REGS1 )
{
  which_save = 0;
  return do_save(DO_ONLY_CODE PASS_REGS);
}

/* Now, to restore the saved code */

/* First check out if we are dealing with a valid file */
static int 
check_header(CELL *info, CELL *ATrail, CELL *AStack, CELL *AHeap USES_REGS)
{
  char pp[256];
  char msg[256];
  CELL hp_size, gb_size, lc_size, tr_size, mode;
  int n;

  /* make sure we always check if there are enough bytes */
  /* skip the first line */
  pp[0] = '\0';
  do {
    if ((n = read(splfild, pp, 1)) <= 0) {
      do_system_error(PERMISSION_ERROR_INPUT_PAST_END_OF_STREAM,"failed to scan first line from saved state");
      return FAIL_RESTORE;
    }
  } while (pp[0] != 1);
  /* now check the version */
  sprintf(msg, "YAP-%s", YAP_SVERSION);
  {
    int count = 0, n, to_read = Unsigned(strlen(msg) + 1);
    while (count < to_read) {
      if ((n = read(splfild, pp, to_read-count)) <= 0) {
	do_system_error(PERMISSION_ERROR_INPUT_PAST_END_OF_STREAM,"failed to scan version info from saved state");
	return FAIL_RESTORE;
      }
      count += n;
    }
  }
  if (strcmp(pp, msg) != 0) {
    LOCAL_ErrorMessage = LOCAL_ErrorSay;
    strncpy(LOCAL_ErrorMessage, "saved state ", MAX_ERROR_MSG_SIZE);
    strncat(LOCAL_ErrorMessage, LOCAL_FileNameBuf, MAX_ERROR_MSG_SIZE);
    strncat(LOCAL_ErrorMessage, " failed to match version ID", MAX_ERROR_MSG_SIZE);
    LOCAL_Error_TYPE = CONSISTENCY_ERROR;
    return FAIL_RESTORE;
  }
  /* check info on header */
  /* ignore info on saved state */
  *info = get_header_cell();
  if (LOCAL_ErrorMessage)
     return FAIL_RESTORE;
  /* check the restore mode */
  mode = get_header_cell();
  if (LOCAL_ErrorMessage)
     return FAIL_RESTORE;
  if (mode != DO_EVERYTHING && mode != DO_ONLY_CODE) {
    return FAIL_RESTORE;
  }
  /* ignore info on stacks size */
  *AHeap = get_header_cell();
  if (LOCAL_ErrorMessage) {
     return FAIL_RESTORE;
  }
  *AStack = get_header_cell();
  if (LOCAL_ErrorMessage) {
     return FAIL_RESTORE;
  }
  *ATrail = get_header_cell();
  if (LOCAL_ErrorMessage) {
     return FAIL_RESTORE;
  }
  /* now, check whether we got enough enough space to load the
     saved space */
  hp_size = get_cell();
  if (LOCAL_ErrorMessage)
     return FAIL_RESTORE;
  while (Yap_HeapBase != NULL &&
	 hp_size > Unsigned(HeapLim) - Unsigned(Yap_HeapBase)) {
    if(!Yap_growheap(FALSE, hp_size, NULL)) {
      return FAIL_RESTORE;
    }
  }
  if (mode == DO_EVERYTHING) {
    lc_size = get_cell();
    if (LOCAL_ErrorMessage)
      return FAIL_RESTORE;
    gb_size=get_cell();
    if (LOCAL_ErrorMessage)
      return FAIL_RESTORE;
    if (Yap_HeapBase != NULL && lc_size+gb_size > Unsigned(LOCAL_LocalBase) - Unsigned(LOCAL_GlobalBase)) {
      if (LOCAL_ErrorMessage != NULL)
	LOCAL_ErrorMessage = "could not allocate enough stack space";
      return FAIL_RESTORE;
    }
    if (Yap_HeapBase != NULL && (tr_size = get_cell()) > Unsigned(LOCAL_TrailTop) - Unsigned(LOCAL_TrailBase)) {
      if (LOCAL_ErrorMessage != NULL)
	LOCAL_ErrorMessage = "could not allocate enough trail space";
      return FAIL_RESTORE;
    }
  } else {
    /* skip cell size */
    get_header_cell();
    if (LOCAL_ErrorMessage)
      return FAIL_RESTORE;
    get_header_cell();
    if (LOCAL_ErrorMessage)
      return FAIL_RESTORE;
    get_header_cell();
    if (LOCAL_ErrorMessage)
      return FAIL_RESTORE;
  }
  return(mode);
}

/* Gets the state of the heap, and evaluates the related variables */
static int 
get_heap_info(USES_REGS1)
{
  LOCAL_OldHeapBase = (ADDR) get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  LOCAL_OldHeapTop = (ADDR) get_cellptr();

  if (LOCAL_ErrorMessage)
      return -1;
  OldHeapUsed = (Int) get_cell();
  if (LOCAL_ErrorMessage)
      return -1;
  FreeBlocks = (BlockHeader *) get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  AuxBase = (ADDR)get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  AuxSp = get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  AuxTop = (ADDR)get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  LOCAL_ScratchPad.ptr = (ADDR)get_cellptr();
  if (LOCAL_ErrorMessage)
      return -1;
  LOCAL_ScratchPad.sz = get_cell();
  if (LOCAL_ErrorMessage)
      return -1;
  LOCAL_ScratchPad.msz = get_cell();
  if (LOCAL_ErrorMessage)
      return -1;
  LOCAL_HDiff = Unsigned(Yap_HeapBase) - Unsigned(LOCAL_OldHeapBase);
  return 1;
}

/* Gets the register array */
/* Saves the old bases for the work areas */
/* and evaluates the difference from the old areas to the new ones */
static int 
get_regs(int flag USES_REGS)
{
  CELL           *NewGlobalBase = (CELL *)LOCAL_GlobalBase;
  CELL           *NewLCL0 = LCL0;
  CELL           *OldXREGS;

  /* Get regs */
  compile_arrays = (int)get_cell();
  if (LOCAL_ErrorMessage)
      return -1;
  if (flag == DO_EVERYTHING) {
    CP = (yamop *)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    ENV = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    ASP = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    /* N = get_cell(); */
    H0 = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    LCL0 = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    H = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    HB = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    B = (choiceptr)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    TR = (tr_fr_ptr)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    YENV = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    S = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    P = (yamop *)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    CreepFlag = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
    EX = (struct DB_TERM *)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
#if defined(YAPOR_SBA) || defined(TABLING)
    H_FZ = get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    B_FZ = (choiceptr)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    TR_FZ = (tr_fr_ptr)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
#endif /* YAPOR_SBA || TABLING */
  }
  CurrentModule = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
  if (flag == DO_EVERYTHING) {
#ifdef COROUTINING
    LOCAL_WokenGoals = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
#endif
#ifdef  DEPTH_LIMIT
    DEPTH = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
#endif
    LOCAL_GcGeneration = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
    LOCAL_GcPhase = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
    LOCAL_GcCurrentPhase = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
  }
  /* Get the old bases */
  OldXREGS = get_cellptr();
  if (LOCAL_ErrorMessage)
    return -1;
  which_save = get_cell();
  if (LOCAL_ErrorMessage)
    return -1;
  LOCAL_XDiff =  (CELL)XREGS - (CELL)OldXREGS;
  if (LOCAL_ErrorMessage)
    return -1;
  if (get_heap_info( PASS_REGS1 ) < 0)
    return -1;
  if (flag == DO_EVERYTHING) {
    ARG1 = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
    if (which_save == 2) {
      ARG2 = get_cell();
      if (LOCAL_ErrorMessage)
	return -1;
    }
    /* get old trail base */
    LOCAL_OldTrailBase = (ADDR)get_cellptr();
    if (LOCAL_ErrorMessage)
      return -1;
    /* Save the old register where we can easily access them */
    LOCAL_OldASP = ASP;
    LOCAL_OldLCL0 = LCL0;
    LOCAL_OldGlobalBase = (CELL *)LOCAL_GlobalBase;
    LOCAL_OldH = H;
    LOCAL_OldTR = TR;
    LOCAL_GDiff = Unsigned(NewGlobalBase) - Unsigned(LOCAL_GlobalBase);
    LOCAL_GDiff0 = 0;
    LOCAL_LDiff = Unsigned(NewLCL0) - Unsigned(LCL0);
    LOCAL_TrDiff = LOCAL_LDiff;
    LOCAL_GlobalBase = (ADDR)NewGlobalBase;
    LCL0 = NewLCL0;
  }
  return 1;
}

/* Get the old opcodes and place them in a hash table */
static int 
get_insts(OPCODE old_ops[])
{
  return myread(splfild, (char *)old_ops, sizeof(OPCODE)*(_std_top+1));
}

/* Get the old atoms hash table */
static int 
get_hash(void)
{
  return myread(splfild, Yap_chtype , NUMBER_OF_CHARS);
}

/* Copy all of the old code to the new Heap */
static int 
CopyCode( USES_REGS1 )
{
  if (myread(splfild, (char *) Yap_HeapBase, (Unsigned(LOCAL_OldHeapTop) - Unsigned(LOCAL_OldHeapBase))) < 0) {
    return -1;
  }
  return 1;
}

/* Copy the local and global stack and also the trail to their new home */
/* In REGS we still have nonadjusted values !! */
static int 
CopyStacks( USES_REGS1 )
{
  Int             j;
  char           *NewASP;

  j = Unsigned(LOCAL_OldLCL0) - Unsigned(ASP);
  NewASP = (char *) (Unsigned(ASP) + (Unsigned(LCL0) - Unsigned(LOCAL_OldLCL0)));
  if (myread(splfild, (char *) NewASP, j) < 0)
    return -1;
  j = Unsigned(H) - Unsigned(LOCAL_OldGlobalBase);
  if (myread(splfild, (char *) LOCAL_GlobalBase, j) < 0)
    return -1;
  j = Unsigned(TR) - Unsigned(LOCAL_OldTrailBase);
  if (myread(splfild, LOCAL_TrailBase, j))
    return -1;
  return 1;
}

/* Copy the local and global stack and also the trail to their new home */
/* In REGS we still have nonadjusted values !! */
static int
CopyTrailEntries( USES_REGS1 )
{
  CELL           entry, *Entries;

  Entries = (CELL *)LOCAL_TrailBase;
  do {
    *Entries++ = entry = get_cell();
    if (LOCAL_ErrorMessage)
      return -1;
  } while ((CODEADDR)entry != NULL);
  return 1;
}

/* get things which are saved in the file */
static int 
get_coded(int flag, OPCODE old_ops[] USES_REGS)
{
  char my_end_msg[256];
  
  if (get_regs(flag PASS_REGS) < 0)
    return -1;
  if (get_insts(old_ops) < 0)
    return -1;
  if (get_hash() < 0)
    return -1;
  if (CopyCode( PASS_REGS1 ) < 0)
    return -1;
  switch (flag) {
  case DO_EVERYTHING:
    if (CopyStacks( PASS_REGS1 ) < 0)
      return -1;
    break;
  case DO_ONLY_CODE:
    if (CopyTrailEntries( PASS_REGS1 ) < 0)
      return -1;
    break;
  }
  /* Check CRC */
  if (myread(splfild, my_end_msg, 256) < 0)
    return -1;
  if (strcmp(end_msg,my_end_msg) != 0) {
    LOCAL_ErrorMessage = "bad trailing CRC in saved state";
    return -1;
  }
  return 1;
}

/* restore some heap registers */
static void 
restore_heap_regs( USES_REGS1 )
{
  if (HeapTop) {
    HeapTop = AddrAdjust(HeapTop);
    *((YAP_SEG_SIZE *) HeapTop) = InUseFlag;
  }
  HeapMax = Yap_heap_regs->heap_used = OldHeapUsed;
  HeapLim = LOCAL_GlobalBase;
}

/* adjust abstract machine registers */
static void 
restore_regs(int flag USES_REGS)
{
  restore_heap_regs( PASS_REGS1 );
  if (CurrentModule) {
    CurrentModule = AtomTermAdjust(CurrentModule);;
  }
  if (flag == DO_EVERYTHING) {
    CP = PtoOpAdjust(CP);
    ENV = PtoLocAdjust(ENV);
    ASP = PtoLocAdjust(ASP);
    H = PtoGloAdjust(H);
    B = (choiceptr)PtoLocAdjust(CellPtr(B));
    TR = PtoTRAdjust(TR);
    P = PtoOpAdjust(P);
    HB = PtoLocAdjust(HB);
    YENV = PtoLocAdjust(YENV);
    S = PtoGloAdjust(S);
    if (EX) {
      EX = DBTermAdjust(EX);
      RestoreDBTerm(EX, TRUE PASS_REGS);
    }
    LOCAL_WokenGoals = AbsAppl(PtoGloAdjust(RepAppl(LOCAL_WokenGoals)));
  }
}

static void
recompute_mask(DBRef dbr)
{
  if (dbr->Flags & DBNoVars) {
    dbr->Mask = Yap_EvalMasks((Term) dbr->DBT.Entry, &(dbr->Key));
  } else if (dbr->Flags & DBComplex) {
    /* This is quite nasty, we want to recalculate the mask but
       we don't want to rebuild the whole term. We'll just build whatever we
       need to recompute the mask.
    */
    CELL *x = (CELL *)HeapTop, *tp;
    unsigned int Arity, i;
    Term out;
    char *tbase = CharP(dbr->DBT.Contents-1);

    if (IsPairTerm(dbr->DBT.Entry)) {

      out = AbsPair(x);
      Arity = 2;
      tp = (CELL *)(tbase + (CELL) RepPair(dbr->DBT.Entry));
    } else {
      Functor f;
    
      tp = (CELL *)(tbase + (CELL) RepAppl(dbr->DBT.Entry));
      f = (Functor)(*tp++);
      out = AbsAppl(x);
      Arity = ArityOfFunctor(f);
      *x++ = (CELL)f;
      if (Arity > 3) Arity = 3;
    }
    for (i = 0; i < Arity; i++) {
      register Term   tw = *tp++;
      if (IsVarTerm(tw)) {
	RESET_VARIABLE(x);
      } else if (IsApplTerm(tw)) {
	/* just fetch the functor from where it is in the data-base.
	   This guarantees we have access to references and friends. */
	CELL offset = (CELL)RepAppl(tw);
	if (offset > dbr->DBT.NOfCells*sizeof(CELL))
	  *x = tw;
	else
	  *x = AbsAppl((CELL *)(tbase + offset));
      } else if (IsAtomicTerm(tw)) {
	*x = tw;
      } else if (IsPairTerm(tw)) {
	*x = AbsPair(x);
      }
      x++;
    }
    dbr->Mask = Yap_EvalMasks(out, &(dbr->Key));
  }
}

#define HASH_SHIFT 6

/*
 * This is used to make an hash table correct, after displacing its elements,
 * HCEnd should point to an area of free space, usually in the heap. The
 * routine is very dependent on the hash function used, and it destroys the
 * previous "hit" order 
 */
static void 
rehash(CELL *oldcode, int NOfE, int KindOfEntries USES_REGS)
{
  register CELL  *savep, *basep;
  CELL           *oldp = oldcode;
  int             TableSize = NOfE - 1, NOfEntries;
  register int    i;
  int             hash;
  CELL            WorkTerm, failplace = 0;
  CELL           *Base = oldcode;

  if (LOCAL_HDiff == 0)
      return;
  basep = H;
  if (H + (NOfE*2) > ASP) {
    basep = (CELL *)TR;
    if (basep + (NOfE*2) > (CELL *)LOCAL_TrailTop) {
      if (!Yap_growtrail((ADDR)(basep + (NOfE*2))-LOCAL_TrailTop, TRUE)) {
	Yap_Error(OUT_OF_TRAIL_ERROR, TermNil,
	      "not enough space to restore hash tables for indexing");
	Yap_exit(1);
      }
    }
  }
  for (i = 0; i < NOfE; ++i) {
    if (*oldp == 0) {
      failplace = oldp[1];
      break;
    }
    oldp += 2;
  }
  savep = basep;
  oldp = oldcode;
  for (i = 0; i < NOfE; ++i) {
    if (*oldp != 0) {
      savep[0] = oldp[0];
      savep[1] = oldp[1];
      oldp[0] = 0;
      oldp[1] = failplace;
      savep += 2;
    }
    oldp += 2;
  }
  NOfEntries = (savep - basep)/2;
  savep = basep;
  for (i = 0; i < NOfEntries; ++i) {
    register Int    d;
    CELL *hentry;

    WorkTerm = savep[i*2];
    hash = (Unsigned(WorkTerm) >> HASH_SHIFT) & TableSize;
    hentry = Base + hash * 2;
    d = TableSize & (Unsigned(WorkTerm) | 1);
    while (*hentry) {
#ifdef DEBUG
#ifdef CLASHES
      ++clashes;
#endif /* CLASHES */
#endif /* DEBUG */
      hash = (hash + d) & TableSize;
      hentry = Base + hash * 2;
    }
    hentry[0] = WorkTerm;
    hentry[1] = savep[i*2+1];
  }
}

static void
RestoreSWIHash(void)
{
  Yap_InitSWIHash();
}


#include "rheap.h"

/* restore the atom entries which are invisible for the user */
static void 
RestoreIOStructures(void)
{
  Yap_InitStdStreams();
}

static void 
RestoreFreeSpace( USES_REGS1 )
{
#if USE_DL_MALLOC
  Yap_av = (struct malloc_state *)AddrAdjust((ADDR)Yap_av);
  Yap_RestoreDLMalloc();
  if (AuxSp != NULL) {
    if (AuxBase < LOCAL_OldHeapBase || AuxBase > LOCAL_OldHeapTop) {
      AuxSp = NULL;
      AuxBase = NULL;
      AuxTop = NULL;
    } else {
      AuxSp = PtoHeapCellAdjust(AuxSp);
      AuxBase = AddrAdjust(AuxBase);
      AuxTop = AddrAdjust(AuxTop);
      LOCAL_ScratchPad.ptr = AddrAdjust(LOCAL_ScratchPad.ptr);
    }
  }
#else
  /* restores the list of free space, with its curious structure */
  BlockHeader *bpt, *bsz;

  if (FreeBlocks != NULL)
    FreeBlocks = BlockAdjust(FreeBlocks);
  bpt = FreeBlocks;
  if (AuxSp != NULL)
    AuxSp = CellPtoHeapAdjust(AuxSp);
  if (AuxTop != NULL)
    AuxTop = AddrAdjust(AuxTop);
  while (bpt != NULL) {
    if (bpt->b_next != NULL) {
      bsz = bpt->b_next = BlockAdjust(bpt->b_next);
      while (bsz != NULL) {
	if (bsz->b_next_size != NULL)
	  bsz->b_next_size = BlockAdjust(bsz->b_next_size);
	if (bsz->b_next != NULL)
	  bsz->b_next = BlockAdjust(bsz->b_next);
	bsz = bsz->b_next;
      }
    }
    if (bpt->b_next_size != NULL)
      bpt->b_next_size = BlockAdjust(bpt->b_next_size);
    bpt = bpt->b_next_size;
  }
  *((YAP_SEG_SIZE *) HeapTop) = InUseFlag;
#endif
}

static void
RestoreAtomList(Atom atm USES_REGS)
{
  AtomEntry      *at;

  at = RepAtom(atm);
  if (EndOfPAEntr(at))
    return;
  do {
    RestoreAtom(at PASS_REGS);
    at = RepAtom(at->NextOfAE);
  } while (!EndOfPAEntr(at));
}


static void
RestoreHashPreds( USES_REGS1 )
{
  UInt size = PredHashTableSize;
  int malloced = FALSE;
  PredEntry **np;
  UInt i;
  PredEntry **oldp = PredHash;

  np = (PredEntry **) Yap_AllocAtomSpace(sizeof(PredEntry **)*size);
  if (!np) {
    if (!(np = (PredEntry **) malloc(sizeof(PredEntry **)*size))) {
	Yap_Error(FATAL_ERROR,TermNil,"Could not allocate space for pred table");
	return;
      }
    malloced = TRUE;
  }
  for (i = 0; i < size; i++) {
    np[i] = NULL;
  }
  for (i = 0; i < PredHashTableSize; i++) {
    PredEntry *p = oldp[i];

    if (p)
      p = PredEntryAdjust(p);
    while (p) {
      Prop nextp;
      UInt hsh;
      
      if (p->NextOfPE)
	p->NextOfPE = PropAdjust(p->NextOfPE);
      nextp = p->NextOfPE;
      CleanCode(p PASS_REGS);
      hsh = PRED_HASH(p->FunctorOfPred, p->ModuleOfPred, size);
      p->NextOfPE = AbsPredProp(np[hsh]);
      np[hsh] = p;
      p = RepPredProp(nextp);
    }
  }
  for (i = 0; i < size; i++) {
    PredHash[i] = np[i];
  }
  if (malloced)
    free((ADDR)np);
  else
    Yap_FreeAtomSpace((ADDR)np);
}

/*
 * This is the really tough part, to restore the whole of the heap 
 */
static void 
restore_heap(void)
{
  restore_codes();
  RestoreIOStructures();
}


#ifdef DEBUG_RESTORE3
static void 
ShowEntries(pp)
	PropEntry      *pp;
{
  while (!EndOfPAEntr(pp)) {
    fprintf(GLOBAL_stderr,"Estou a ver a prop %x em %x\n", pp->KindOfPE, pp);
    pp = RepProp(pp->NextOfPE);
  }
}

static void 
ShowAtoms()
{
  AtomHashEntry  *HashPtr = HashChain;
  register int    i;
  for (i = 0; i < AtomHashTableSize; ++i) {
    if (HashPtr->Entry != NIL) {
      AtomEntry      *at;
      at = RepAtom(HashPtr->Entry);
      do {
	fprintf(GLOBAL_stderr,"Passei ao %s em %x\n", at->StrOfAE, at);
	ShowEntries(RepProp(at->PropsOfAE));
      } while (!EndOfPAEntr(at = RepAtom(at->NextOfAE)));
    }
    HashPtr++;
  }
  HashPtr = WideHashChain;
  for (i = 0; i < WideAtomHashTableSize; ++i) {
    if (HashPtr->Entry != NIL) {
      AtomEntry      *at;
      at = RepAtom(HashPtr->Entry);
      do {
	fprintf(GLOBAL_stderr,"Passei ao %s em %x\n", at->StrOfAE, at);
	ShowEntries(RepProp(at->PropsOfAE));
      } while (!EndOfPAEntr(at = RepAtom(at->NextOfAE)));
    }
    HashPtr++;
  }
}

#endif /* DEBUG_RESTORE3 */

#include <stdio.h>

static int
commit_to_saved_state(char *s, CELL *Astate, CELL *ATrail, CELL *AStack, CELL *AHeap) {
  CACHE_REGS
  int mode;

  if ((mode = check_header(Astate,ATrail,AStack,AHeap PASS_REGS)) == FAIL_RESTORE)
    return(FAIL_RESTORE);
  LOCAL_PrologMode = BootMode;
  if (Yap_HeapBase) {
    extern void Scleanup(void);
    if (!yap_flags[HALT_AFTER_CONSULT_FLAG] && !yap_flags[QUIET_MODE_FLAG]) {
      Yap_TrueFileName(s,LOCAL_FileNameBuf2, YAP_FILENAME_MAX);
      fprintf(stderr, "%% Restoring file %s\n", LOCAL_FileNameBuf2);
    }
    Scleanup();
    Yap_CloseStreams(TRUE);
  }
#ifdef DEBUG_RESTORE4
  /*
   * This should be another file, like the log file 
   */
  errout = GLOBAL_stderr;
#endif
  return mode;
}

static void
cat_file_name(char *s, char *prefix, char *name, unsigned int max_length)
{
  strncpy(s, prefix, max_length);
#if _MSC_VER || defined(__MINGW32__)
  strncat(s,"\\", max_length);
#else
  strncat(s,"/", max_length);
#endif
  strncat(s, name, max_length-1);
}

static int try_open(char *inpf, CELL *Astate, CELL *ATrail, CELL *AStack, CELL *AHeap, char *buf, IOSTREAM **streamp) {
  int mode;

  if (streamp) {
    if ((*streamp = Sopen_file(inpf, "rb"))) {
      return DO_ONLY_CODE;
    }
    return FAIL_RESTORE;
  }
  if ((splfild = open_file(inpf, O_RDONLY)) < 0) {
    return FAIL_RESTORE;
  }
  if (buf[0] == '\0')
    strncpy(buf, inpf, YAP_FILENAME_MAX);
  if ((mode = commit_to_saved_state(inpf,Astate,ATrail,AStack,AHeap)) != FAIL_RESTORE) {
    CACHE_REGS
    LOCAL_ErrorMessage = NULL;
    return mode;
  }
  return mode;
}

static int 
OpenRestore(char *inpf, char *YapLibDir, CELL *Astate, CELL *ATrail, CELL *AStack, CELL *AHeap, IOSTREAM **streamp)
{
  CACHE_REGS
  int mode = FAIL_RESTORE;
  char save_buffer[YAP_FILENAME_MAX+1];

  save_buffer[0] = '\0';
  //  LOCAL_ErrorMessage = NULL;
  if (inpf == NULL) {
      inpf = StartUpFile;
  }
  /* careful it starts from the root */
  if (inpf[0] != '/') {
#if __simplescalar__
    /* does not implement getcwd */
    strncpy(LOCAL_FileNameBuf,GLOBAL_pwd,YAP_FILENAME_MAX);
#elif HAVE_GETCWD
    if (getcwd (LOCAL_FileNameBuf, YAP_FILENAME_MAX) == NULL)
      LOCAL_FileNameBuf[0] = '\0';
#else
    if (getwd (LOCAL_FileNameBuf) == NULL)
      LOCAL_FileNameBuf[0] = '\0';
#endif
    strncat(LOCAL_FileNameBuf, "/", YAP_FILENAME_MAX-1);
    strncat(LOCAL_FileNameBuf, inpf, YAP_FILENAME_MAX-1);
  } else {
    strncat(LOCAL_FileNameBuf, inpf, YAP_FILENAME_MAX-1);
  }
  if (inpf != NULL && (splfild = open_file(inpf, O_RDONLY)) > 0) {
    if ((mode = try_open(inpf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
      return mode;
    }
  }
  if (!Yap_dir_separator(inpf[0]) && !Yap_volume_header(inpf)) {
    /*
      we have a relative path for the file, try to do somewhat better 
      using YAPLIBDIR or friends.
    */
    if (YapLibDir != NULL) {
      cat_file_name(LOCAL_FileNameBuf, Yap_LibDir, inpf, YAP_FILENAME_MAX);
      if ((mode = try_open(LOCAL_FileNameBuf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
	return mode;
      }
    } else {
      if ((mode = try_open(LOCAL_FileNameBuf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
	return mode;
      }
    }
#if HAVE_GETENV
    {
      char *yap_env = getenv("YAPLIBDIR");
      if (yap_env != NULL) {
	cat_file_name(LOCAL_FileNameBuf, yap_env, inpf, YAP_FILENAME_MAX);
	if ((mode = try_open(LOCAL_FileNameBuf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
	  return mode;
	}
      }
    }
#endif
    if (YAP_LIBDIR != NULL) {
      cat_file_name(LOCAL_FileNameBuf, YAP_LIBDIR, inpf, YAP_FILENAME_MAX);
      if ((splfild = open_file(LOCAL_FileNameBuf, O_RDONLY)) > 0) {
	if ((mode = try_open(LOCAL_FileNameBuf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
	  return mode;
	}
      }
    }
  }
#if _MSC_VER || defined(__MINGW32__)
  if ((inpf = Yap_RegistryGetString("startup"))) {
    if ((splfild = open_file(inpf, O_RDONLY)) > 0) {
      if ((mode = try_open(inpf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
	return mode;
      }
    }
  }
  {
    DWORD fatts;
    int buflen;
    char *pt;

    /* try to get it from current executable */
    if ((fatts = GetFileAttributes(LOCAL_FileNameBuf)) == 0xFFFFFFFFL ||
	!(fatts & FILE_ATTRIBUTE_DIRECTORY)) {
      /* couldn't find it where it was supposed to be,
	 let's try using the executable */
      if (!GetModuleFileNameEx( GetCurrentProcess(), NULL, LOCAL_FileNameBuf, YAP_FILENAME_MAX)) {
	/* do nothing */
	goto end;
      }
      buflen = strlen(LOCAL_FileNameBuf);
      pt = LOCAL_FileNameBuf+buflen;
      while (*--pt != '\\') {
	/* skip executable */
	if (pt == LOCAL_FileNameBuf) {
	  /* do nothing */
	  goto end;
	}
      }
      while (*--pt != '\\') {
	/* skip parent directory "bin\\" */
	if (pt == LOCAL_FileNameBuf) {
	  goto end;
	}
      }
      /* now, this is a possible location for the ROOT_DIR, let's look for a share directory here */
      pt[1] = '\0';
      strncat(LOCAL_FileNameBuf,"lib/Yap/startup.yss",YAP_FILENAME_MAX);
    }
    if ((mode = try_open(LOCAL_FileNameBuf,Astate,ATrail,AStack,AHeap,save_buffer,streamp)) != FAIL_RESTORE) {
      return mode;
    }
  }
 end:
#endif
  /* try to open from current directory */
  /* could not open file */
  if (LOCAL_ErrorMessage == NULL) {
    if (save_buffer[0]) {
      strncpy(LOCAL_FileNameBuf, save_buffer, YAP_FILENAME_MAX-1);
      do_system_error(PERMISSION_ERROR_OPEN_SOURCE_SINK,"incorrect saved state");
    } else {
      strncpy(LOCAL_FileNameBuf, inpf, YAP_FILENAME_MAX-1);
      do_system_error(PERMISSION_ERROR_OPEN_SOURCE_SINK,"could not open saved state");
    }
  }
  return FAIL_RESTORE;
}

IOSTREAM * 
Yap_OpenRestore(char *inpf, char *YapLibDir)
{
  IOSTREAM *stream = NULL;

  OpenRestore(inpf, YapLibDir, NULL, NULL, NULL, NULL, &stream);
  return stream;
}

static void 
CloseRestore(void)
{
 CACHE_REGS
#ifdef DEBUG_RESTORE3
  ShowAtoms();
#endif
  close_file();
  LOCAL_PrologMode = UserMode;
}

#if !defined(_WIN32)
static int 
check_opcodes(OPCODE old_ops[])
{
#if USE_THREADED_CODE
  int have_shifted = FALSE;
  op_numbers op = _Ystop;
  for (op = _Ystop; op < _std_top; op++) {
    if (Yap_opcode(op) != old_ops[op]) {
      have_shifted = TRUE;
      break;
    }
  }
  return have_shifted;
#else
  /* be conservative */
  return TRUE;
#endif
}
#endif

static void 
RestoreHeap(OPCODE old_ops[] USES_REGS)
{
  int heap_moved = (LOCAL_OldHeapBase != Yap_HeapBase ||
		    LOCAL_XDiff), opcodes_moved;
  Term mod = CurrentModule;

  CurrentModule = PROLOG_MODULE;
#if defined(_WIN32)
  /* It seems that under WIN32 opcodes may not have moved but the
     remaining code may have bmoved */ 
  opcodes_moved = TRUE;
#else
  opcodes_moved = check_opcodes(old_ops);
#endif
  /* opcodes_moved has side-effects and should be tried first */
  if (heap_moved) {
    opcodes_moved = TRUE;
    RestoreFreeSpace( PASS_REGS1 );
  }
  if (heap_moved || opcodes_moved) {
    restore_heap();
  }
  /* This must be done after restore_heap */
  Yap_InitAbsmi();
  if (opcodes_moved) {
    Yap_InitCPreds();
    Yap_InitBackCPreds();
  }
  if (!(Yap_ReInitConstExps() &&
	Yap_ReInitUnaryExps() &&
	Yap_ReInitBinaryExps())) {
    Yap_Error(SYSTEM_ERROR, TermNil, "arithmetic operator not in saved state");
  }
#ifdef DEBUG_RESTORE1
  fprintf(errout, "phase 1 done\n");
#endif
  CurrentModule = mod;
}

/*
 * This function is called to know about the parameters of the last saved
 * state 
 */
int 
Yap_SavedInfo(char *FileName, char *YapLibDir, CELL *ATrail, CELL *AStack, CELL *AHeap)
{
  return DO_ONLY_CODE;

  CELL MyTrail, MyStack, MyHeap, MyState;
  int             mode;

  mode = OpenRestore(FileName, YapLibDir, &MyState, &MyTrail, &MyStack, &MyHeap, NULL);
  if (mode == FAIL_RESTORE) {
    return -1;
  }
  close_file();
  if (! *AHeap)
    *AHeap = MyHeap / 1024;
  if (mode != DO_ONLY_CODE && *AStack)
    *AStack = MyStack / 1024;
  if (mode != DO_ONLY_CODE && *ATrail)
    *ATrail = MyTrail / 1024;
  return (MyState);
}

static void
UnmarkTrEntries( USES_REGS1 )
{
  CELL           entry, *Entries;

  /* initialise a choice point */
  B = (choiceptr)LCL0;
  B--;
  B->cp_ap = NOCODE;
  Entries = (CELL *)LOCAL_TrailBase;
  while ((entry = *Entries++) != (CELL)NULL) {
    if (!IsVarTerm(entry)) {
      if(IsPairTerm(entry)) {
	CELL *ent = CellPtoHeapAdjust(RepPair(entry));
	register CELL flags;

	flags = *ent;
	ResetFlag(InUseMask, flags);
	*ent = flags;
	if (FlagOn((DirtyMask|ErasedMask), flags)) {
	  if (FlagOn(DBClMask, flags)) {
	    Yap_ErDBE(DBStructFlagsToDBStruct(ent));
	  } else {
	    if (flags & LogUpdMask) {
	      if (flags & IndexMask) {
		if (FlagOn(ErasedMask, flags))
		  Yap_ErLogUpdIndex(ClauseFlagsToLogUpdIndex(ent));
		else
		  Yap_CleanUpIndex(ClauseFlagsToLogUpdIndex(ent));
	      } else {
		Yap_ErLogUpdCl(ClauseFlagsToLogUpdClause(ent));
	      }
	    } else {
	      Yap_ErCl(ClauseFlagsToDynamicClause(ent));
	    }
	  }
	}
#ifdef MULTI_ASSIGNMENT_VARIABLES
      } else /* if (IsApplTerm(d1)) */ {
	Entries += 2;
#endif
      }
    }
  }
  B = NULL;
}


int in_limbo = FALSE;

/* cleanup any records we had in the saved state. They are now inaccessible */
static void
FreeRecords(void) {
  struct record_list *ptr;

  ptr = Yap_Records;
  Yap_Records = NULL;
  while (ptr) {
    struct record_list *optr = ptr;
    Yap_ReleaseTermFromDB(ptr->dbrecord);
    ptr = ptr->next_rec;
    Yap_FreeCodeSpace((void *)optr);
  }
}

/*
 * This function is called when wanting only to restore the heap and
 * associated registers 
 */
static int 
Restore(char *s, char *lib_dir USES_REGS)
{
  int restore_mode;

  OPCODE old_ops[_std_top+1];
  CELL MyTrail, MyStack, MyHeap, MyState;

  if ((restore_mode = OpenRestore(s, lib_dir, &MyState, &MyTrail, &MyStack, &MyHeap, NULL)) == FAIL_RESTORE)
    return(FALSE);
  Yap_ShutdownLoadForeign();
  in_limbo = TRUE;
  if (get_coded(restore_mode, old_ops PASS_REGS) < 0)
     return FAIL_RESTORE;  
  restore_regs(restore_mode PASS_REGS);
  in_limbo = FALSE;
  /*#endif*/
  RestoreHeap(old_ops PASS_REGS);
  switch(restore_mode) {
  case DO_EVERYTHING:
    if (LOCAL_OldHeapBase != Yap_HeapBase ||
	LOCAL_OldLCL0 != LCL0 ||
	LOCAL_OldGlobalBase != (CELL *)LOCAL_GlobalBase ||
	LOCAL_OldTrailBase != LOCAL_TrailBase) {
      Yap_AdjustStacksAndTrail();
      if (which_save == 2) {
	Yap_AdjustRegs(2);
      } else {
	Yap_AdjustRegs(1);
      }
      break;
#ifdef DEBUG_RESTORE2
      fprintf(errout, "phase 2 done\n");
#endif
    }
    break;
  case DO_ONLY_CODE:
    UnmarkTrEntries( PASS_REGS1 );
    Yap_InitYaamRegs( 0 );
    break;
  }

  Yap_ReOpenLoadForeign();
  FreeRecords();
  /* restart IO */
  initIO();
  /* reset time */
  Yap_ReInitWallTime();
  Yap_InitSysPath();
#if USE_DL_MALLOC || USE_SYSTEM_MALLOC
  if (!AuxSp) {
    Yap_InitPreAllocCodeSpace( 0 );
  }
#endif
  CloseRestore();
  if (which_save == 2) {
    Yap_unify(ARG2, MkIntTerm(0));
  }
  return restore_mode;
}

int 
Yap_SavedStateRestore(char *s, char *lib_dir)
{
  CACHE_REGS
  return Restore(s, lib_dir PASS_REGS);
}

static Int 
p_restore( USES_REGS1 )
{
  int mode;
  char s[YAP_FILENAME_MAX+1];

  Term t1 = Deref(ARG1);
#ifdef YAPOR
  if (GLOBAL_number_workers != 1) {
    Yap_Error(SYSTEM_ERROR,TermNil,"cannot perform save: more than a worker/thread running");
    return(FALSE);
  }
#endif /* YAPOR */
#ifdef THREADS
  if (GLOBAL_NOfThreads != 1) {
    Yap_Error(SYSTEM_ERROR,TermNil,"cannot perform save: more than a worker/thread running");
    return(FALSE);
  }
#endif /* THREADS */
  if (!Yap_GetName(s, YAP_FILENAME_MAX, t1)) {
    Yap_Error(TYPE_ERROR_LIST,t1,"restore/1");
    return(FALSE);
  }
  if ((mode = Restore(s, NULL PASS_REGS)) == DO_ONLY_CODE) {
    Yap_RestartYap( 3 );
  }
  return(mode != FAIL_RESTORE);
}

void 
Yap_InitSavePreds(void)
{
  Yap_InitCPred("$save", 2, p_save2, SyncPredFlag);
  Yap_InitCPred("$save_program", 1, p_save_program, SyncPredFlag);
  Yap_InitCPred("$restore", 1, p_restore, SyncPredFlag);
}
