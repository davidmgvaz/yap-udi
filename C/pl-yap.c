
/* YAP support for some low-level SWI stuff */

#define PL_KERNEL 1

#include <stdio.h>
#include "Yap.h"
#include "Yatom.h"
#include "pl-incl.h"
#if HAVE_MATH_H
#include <math.h>
#endif

#define	Quote_illegal_f		1
#define	Ignore_ops_f		2
#define	Handle_vars_f		4
#define	Use_portray_f		8
#define	To_heap_f	       16
#define	Unfold_cyclics_f       32

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define LOCK()   PL_LOCK(L_PLFLAG)
#define UNLOCK() PL_UNLOCK(L_PLFLAG)

int fileerrors;

PL_local_data_t lds;

gds_t gds;

static atom_t
uncachedCodeToAtom(int chrcode)
{ if ( chrcode < 256 )
  { char tmp[2];

    tmp[0] = chrcode;
    tmp[1] = '\0';
    return lookupAtom(tmp, 1);
  } else
  { pl_wchar_t tmp[2];

    tmp[0] = chrcode;
    tmp[1] = '\0';

    return (atom_t)YAP_LookupWideAtom(tmp);
  }
}


atom_t
codeToAtom(int chrcode)
{ atom_t a;

  if ( chrcode == EOF )
    return ATOM_end_of_file;

  assert(chrcode >= 0);

  if ( chrcode < (1<<15) )
  { int page  = chrcode / 256;
    int entry = chrcode % 256;
    atom_t *pv;

    if ( !(pv=GD->atoms.for_code[page]) )
    { pv = PL_malloc(256*sizeof(atom_t));
      
      memset(pv, 0, 256*sizeof(atom_t));
      GD->atoms.for_code[page] = pv;
    }

    if ( !(a=pv[entry]) )
    { a = pv[entry] = uncachedCodeToAtom(chrcode);
    }
  } else
  { a = uncachedCodeToAtom(chrcode);
  }
  
  return a;
}

word
globalString(size_t size, char *s)
{
  return Yap_MkBlobStringTerm(s, size);
}

word
globalWString(size_t size, wchar_t *s)
{
  return Yap_MkBlobWideStringTerm(s, size);
}

int
PL_rethrow(void)
{ GET_LD

  if ( LD->exception.throw_environment )
    longjmp(LD->exception.throw_environment->exception_jmp_env, 1);

  fail;
}

int
saveWakeup(wakeup_state *state, int forceframe ARG_LD)
{
  return 0;
}

void
restoreWakeup(wakeup_state *state ARG_LD)
{
}

int
callProlog(module_t module, term_t goal, int flags, term_t *ex )
{ GET_LD
  term_t g = PL_new_term_ref();
  functor_t fd;
  predicate_t proc;

  if ( ex )
    *ex = 0;

  PL_strip_module(goal, &module, g);
  if ( !PL_get_functor(g, &fd) )
  { PL_error(NULL, 0, NULL, ERR_TYPE, ATOM_callable, goal);
    if ( ex )
      *ex = exception_term;

    fail;
  }
  
  proc = PL_pred(fd, module);
  
  { int arity = arityFunctor(fd);
    term_t args = PL_new_term_refs(arity);
    qid_t qid;
    int n, rval;

    for(n=0; n<arity; n++)
      _PL_get_arg(n+1, g, args+n);

    qid  = PL_open_query(module, flags, proc, args);
    rval = PL_next_solution(qid);
    if ( !rval && ex )
      *ex = PL_exception(qid);
    PL_cut_query(qid);

    return rval;
  }
}

extern YAP_Term Yap_InnerEval(YAP_Term t);

inline static YAP_Term
Yap_Eval(YAP_Term t)
{
  if (t == 0L || ( !YAP_IsVarTerm(t) && (YAP_IsIntTerm(t) || YAP_IsFloatTerm(t)) ))
    return t;
  return Yap_InnerEval(t);
}

IOENC
Yap_DefaultEncoding(void)
{
  GET_LD
  return LD->encoding;
}

void
Yap_SetDefaultEncoding(IOENC new_encoding)
{
  GET_LD
  LD->encoding = new_encoding;
}

int
valueExpression(term_t t, Number r ARG_LD)
{
  YAP_Term t0 = Yap_Eval(YAP_GetFromSlot(t));
  if (YAP_IsIntTerm(t0)) {
    r->type = V_INTEGER;
    r->value.i = YAP_IntOfTerm(t0);
    return 1;
  }
  if (YAP_IsFloatTerm(t0)) {
    r->type = V_FLOAT;
    r->value.f = YAP_FloatOfTerm(t0);
    return 1;
  }
#ifdef O_GMP
  if (YAP_IsBigNumTerm(t0)) {
    r->type = V_MPZ;
    mpz_init(r->value.mpz);
    YAP_BigNumOfTerm(t0, r->value.mpz);
    return 1;
  }
  if (YAP_IsRationalTerm(t0)) {
    r->type = V_MPQ;
    mpq_init(r->value.mpq);
    YAP_RationalOfTerm(t0, r->value.mpq);
    return 1;
  }
#endif
  return 0;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
toIntegerNumber(Number n, int flags)

Convert a number to an integer. Default,   only rationals that happen to
be integer are converted. If   TOINT_CONVERT_FLOAT  is present, floating
point  numbers  are  converted  if  they  represent  integers.  If  also
TOINT_TRUNCATE is provided non-integer floats are truncated to integers.

Note that if a double is  out  of   range  for  int64_t,  it never has a
fractional part.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int
double_in_int64_range(double x)
{ int k;
  double y = frexp(x, &k);

  if ( k < 8*(int)sizeof(int64_t) ||
       (y == -0.5 && k == 8*(int)sizeof(int64_t)) )
    return TRUE;

  return FALSE;
}

int
toIntegerNumber(Number n, int flags)
{ 
switch(n->type)
  { case V_INTEGER:
      succeed;
#ifdef O_GMP
    case V_MPZ:
      succeed;
    case V_MPQ:				/* never from stacks iff integer */
      if ( mpz_cmp_ui(mpq_denref(n->value.mpq), 1L) == 0 )
      { mpz_clear(mpq_denref(n->value.mpq));
	n->value.mpz[0] = mpq_numref(n->value.mpq)[0];
	n->type = V_MPZ;
	succeed;
      }
      fail;
#endif
    case V_FLOAT:
      if ( (flags & TOINT_CONVERT_FLOAT) )
      { if ( double_in_int64_range(n->value.f) )
	{ int64_t l = (int64_t)n->value.f;
	  
	  if ( (flags & TOINT_TRUNCATE) ||
	       (double)l == n->value.f )
	  { n->value.i = l;
	    n->type = V_INTEGER;
	    
	    return TRUE;
	  }
	  return FALSE;
#ifdef O_GMP
	} else
	{ mpz_init_set_d(n->value.mpz, n->value.f);
	  n->type = V_MPZ;
	  
	  return TRUE;
#endif
	}
      }
      return FALSE;
  }
  assert(0);
  fail;
} 


int
_PL_unify_atomic(term_t t, PL_atomic_t a)
{
  GET_LD
  return PL_unify_atom(t, a);
}

int
_PL_unify_string(term_t t, word w)
{
  CACHE_REGS  
  return Yap_unify(Yap_GetFromSlot(t PASS_REGS), w);
}

word lookupAtom(const char *s, size_t len)
{
  YAP_Atom at;

  /* dirty trick to ensure s is null terminated */
  char *st = (char *)s;
  st[len] = '\0';
  if (len >= strlen(s)) {
    at = YAP_LookupAtom(st);
  } else {
    char * buf = PL_malloc(len+1);

    if (!buf)
      return 0;
    strncpy(buf,s,len);
    at = YAP_LookupAtom(buf);
    PL_free(buf);
  }  
  Yap_AtomIncreaseHold(at);  
  return (word)at;
}

atom_t lookupUCSAtom(const pl_wchar_t *s, size_t len)
{
  YAP_Atom at;

  if (len >= wcslen(s)) {
    at = YAP_LookupWideAtom(s);
  } else {
    pl_wchar_t * buf = PL_malloc((len+1)*sizeof(pl_wchar_t));

    if (!buf)
      return 0;
    wcsncpy(buf,s,len);
    at = YAP_LookupWideAtom(buf);
    PL_free(buf);
  }
  Yap_AtomIncreaseHold(at);
  return (atom_t)at;
}



		 /*******************************
		 *	       OPTIONS		*
		 *******************************/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Variable argument list:

	atom_t	name
	int	type	OPT_ATOM, OPT_STRING, OPT_BOOL, OPT_INT, OPT_LONG
	pointer	value
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define MAXOPTIONS 32

typedef union
{ bool *b;				/* boolean value */
  long *l;				/* long value */
  int  *i;				/* integer value */
  char **s;				/* string value */
  word *a;				/* atom value */
  term_t *t;				/* term-reference */
  void *ptr;				/* anonymous pointer */
} optvalue;


int
get_atom_ptr_text(Atom a, PL_chars_t *text)
{ 
  YAP_Atom ya = (YAP_Atom)a;
  if (YAP_IsWideAtom(ya)) {
    pl_wchar_t *name = (pl_wchar_t *)YAP_WideAtomName(ya);
    text->text.w   = name;
    text->length   = wcslen(name);
    text->encoding = ENC_WCHAR;
  } else
    { char *name = (char *)YAP_AtomName(ya);
    text->text.t   = name;
    text->length   = strlen(name);
    text->encoding = ENC_ISO_LATIN_1;
  }
  text->storage   = PL_CHARS_HEAP;
  text->canonical = TRUE;

  succeed;
}


int
get_atom_text(atom_t atom, PL_chars_t *text)
{ Atom a = (Atom)atomValue(atom);

  return get_atom_ptr_text(a, text);
}

int
get_string_text(word w, PL_chars_t *text ARG_LD)
{
  CELL fl = RepAppl(w)[1];
  if (fl == BLOB_STRING) {
    text->text.t = Yap_BlobStringOfTerm(w);
    text->encoding = ENC_ISO_LATIN_1;
    text->length = strlen(text->text.t);
  } else {
    text->text.w = Yap_BlobWideStringOfTerm(w);
    text->encoding = ENC_WCHAR;
    text->length = wcslen(text->text.w);
  }
  text->storage = PL_CHARS_STACK;
  text->canonical = TRUE;
  return TRUE;
}

void
PL_get_number(term_t l, number *n) {
  GET_LD
  YAP_Term t = valHandle(l);
  if (YAP_IsIntTerm(t)) {
    n->type = V_INTEGER;
    n->value.i = YAP_IntOfTerm(t);
#ifdef O_GMP
  } else if (YAP_IsBigNumTerm(t)) {
    n->type = V_MPZ;
    mpz_init(n->value.mpz);
    YAP_BigNumOfTerm(t, n->value.mpz);
  } else {
    n->type = V_MPQ;
    mpq_init(n->value.mpq);
    YAP_RationalOfTerm(t, &n->value.mpq);
#endif
  }
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Formatting a float. This is very  complicated   as  we must write floats
such that it can be read as a float. This means using the conventions of
the C locale and if the float happens to be integer as <int>.0.

Switching the locale is no option as  locale handling is not thread-safe
and may have unwanted  consequences  for   embedding.  There  is  a intptr_t
discussion on the very same topic on  the Python mailinglist. Many hacks
are proposed, none is very satisfactory.   Richard  O'Keefe suggested to
use ecvt(), fcvt() and gcvt(). These  are   not  thread-safe.  The GNU C
library provides *_r() variations that  can   do  the  trick. An earlier
patch used localeconv() to find the  decimal   point,  but  this is both
complicated and not thread-safe.

Finally, with help of Richard we decided  to replace the first character
that is not a digit nor [eE], as this must be the decimal point. 
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define isDigit(c)	((c) >= '0' && (c) <= '9')

intptr_t
lengthList(term_t list, int errors)
{ GET_LD
  intptr_t length = 0;
  Word l = YAP_AddressFromSlot(list);
  Word tail;

  length = skip_list(l, &tail PASS_LD);

  if ( isNil(*tail) )
    return length;

  if ( errors )
    PL_error(NULL, 0, NULL, ERR_TYPE, ATOM_list, wordToTermRef(l));

  return isVar(*tail) ? -2 : -1;
}

int raiseStackOverflow(int overflow)
{
  return overflow;
}

		 /*******************************
		 *	    FEATURES		*
		 *******************************/

int
PL_set_prolog_flag(const char *name, int type, ...)
{ va_list args;
  int rval = TRUE;
  int flags = (type & FF_MASK);

  initPrologFlagTable();

  va_start(args, type);
  switch(type & ~FF_MASK)
  { case PL_BOOL:
    { int val = va_arg(args, int);

      setPrologFlag(name, FT_BOOL|flags, val, 0);
      break;
    }
    case PL_ATOM:
    { const char *v = va_arg(args, const char *);
#ifndef __YAP_PROLOG__
      if ( !GD->initialised )
	initAtoms();
#endif
      setPrologFlag(name, FT_ATOM|flags, v);
      break;
    }
    case PL_INTEGER:
    { intptr_t v = va_arg(args, intptr_t);
      setPrologFlag(name, FT_INTEGER|flags, v);
      break;
    }
    default:
      rval = FALSE;
  }

  va_end(args);
  return rval;
}



int
PL_unify_chars(term_t t, int flags, size_t len, const char *s)
{ PL_chars_t text;
  term_t tail;
  int rc;

  if ( len == (size_t)-1 )
    len = strlen(s);

  text.text.t    = (char *)s;
  text.encoding  = ((flags&REP_UTF8) ? ENC_UTF8 : \
		    (flags&REP_MB)   ? ENC_ANSI : ENC_ISO_LATIN_1);
  text.storage   = PL_CHARS_HEAP;
  text.length    = len;
  text.canonical = FALSE;

  flags &= ~(REP_UTF8|REP_MB|REP_ISO_LATIN_1);

  if ( (flags & PL_DIFF_LIST) )
  { tail = t+1;
    flags &= (~PL_DIFF_LIST);
  } else
  { tail = 0;
  }

  rc = PL_unify_text(t, tail, &text, flags);
  PL_free_text(&text);

  return rc;
}

X_API int PL_handle_signals(void)
{
  GET_LD
  if ( !LD || LD->critical || !LD->signal.pending )
    return 0;
  if (LD->signal.pending == 2) {
    Yap_Error(PURE_ABORT, TermNil, "abort from console");
  }
  //  fprintf(stderr,"PL_handle_signals not implemented\n");
  return 1;
}

void
outOfCore()
{ fprintf(stderr,"Could not allocate memory: %s", OsError());
  exit(1);
}

int
priorityOperator(Module m, atom_t atom)
{
  YAP_Term mod = (YAP_Term)m;
  if (!m) 
    mod = YAP_CurrentModule();
  return YAP_MaxOpPriority(YAP_AtomFromSWIAtom(atom), mod);
}

int
currentOperator(Module m, atom_t name, int kind, int *type, int *priority)
{
  YAP_Term mod = (YAP_Term)m;
  int opkind, yap_type;

  if (!m) 
    mod = YAP_CurrentModule();
  switch (kind) {
  case OP_PREFIX:
    opkind = 2;
    break;
  case OP_INFIX:
    opkind = 0;
    break;
  case OP_POSTFIX:
  default:
    opkind = 1;
  }
  if (!YAP_OpInfo(YAP_AtomFromSWIAtom(name), mod, opkind, &yap_type, priority)) {
    return FALSE;
  }
  switch(yap_type) {
  case 1:
    *type = OP_XFX;
    break;
  case 2:
    *type = OP_XFY;
    break;
  case 3:
    *type = OP_YFX;
    break;
  case 4:
    *type = OP_XFX;
    break;
  case 5:
    *type = OP_XF;
    break;
  case 6:
    *type = OP_YF;
    break;
  case 7:
    *type = OP_FX;
    break;    
  default:
    *type = OP_FY;
    break;
  }
  return 1;
}

int
numberVars(term_t t, nv_options *opts, int n ARG_LD) {

  return Yap_NumberVars(YAP_GetFromSlot(t), n, opts->singletons);
}

		 /*******************************
		 *	     PROMOTION		*
		 *******************************/

static int
check_float(double f)
{
#ifdef HAVE_FPCLASSIFY
  switch(fpclassify(f))
  { case FP_NAN:
      return PL_error(NULL, 0, NULL, ERR_AR_UNDEF);
      break;
    case FP_INFINITE:
      return PL_error(NULL, 0, NULL, ERR_AR_OVERFLOW);
      break;
  }
#else
#ifdef HAVE_FPCLASS
  switch(fpclass(f))
  { case FP_SNAN:
    case FP_QNAN:
      return PL_error(NULL, 0, NULL, ERR_AR_UNDEF);
      break;
    case FP_NINF:
    case FP_PINF:
      return PL_error(NULL, 0, NULL, ERR_AR_OVERFLOW);
      break;
    case FP_NDENORM:			/* pos/neg denormalized non-zero */
    case FP_PDENORM:
    case FP_NNORM:			/* pos/neg normalized non-zero */
    case FP_PNORM:
    case FP_NZERO:			/* pos/neg zero */
    case FP_PZERO:
      break;
  }
#else
#ifdef HAVE__FPCLASS
  switch(_fpclass(f))
  { case _FPCLASS_SNAN:
    case _FPCLASS_QNAN:
      return PL_error(NULL, 0, NULL, ERR_AR_UNDEF);
      break;
    case _FPCLASS_NINF:
    case _FPCLASS_PINF:
      return PL_error(NULL, 0, NULL, ERR_AR_OVERFLOW);
      break;
  }
#else
#ifdef HAVE_ISNAN
  if ( isnan(f) )
    return PL_error(NULL, 0, NULL, ERR_AR_UNDEF);
#endif
#ifdef HAVE_ISINF
  if ( isinf(f) )
    return PL_error(NULL, 0, NULL, ERR_AR_OVERFLOW);
#endif
#endif /*HAVE__FPCLASS*/
#endif /*HAVE_FPCLASS*/
#endif /*HAVE_FPCLASSIFY*/
  return TRUE;
}


int
promoteToFloatNumber(Number n)
{ switch(n->type)
  { case V_INTEGER:
      n->value.f = (double)n->value.i;
      n->type = V_FLOAT;
      break;
#ifdef O_GMP
    case V_MPZ:
    { double val = mpz_get_d(n->value.mpz);

      if ( !check_float(val) )
	return FALSE;

      clearNumber(n);
      n->value.f = val;
      n->type = V_FLOAT;
      break;
    }
    case V_MPQ:
    { double val = mpq_get_d(n->value.mpq);

      if ( !check_float(val) )
	return FALSE;

      clearNumber(n);
      n->value.f = val;
      n->type = V_FLOAT;
      break;
    }
#endif
    case V_FLOAT:
      break;
  }

  return TRUE;
}



int
PL_get_list_nchars(term_t l, size_t *length, char **s, unsigned int flags)
{ Buffer b;
  CVT_result result;

  if ( (b = codes_or_chars_to_buffer(l, flags, FALSE, &result)) )
  { char *r;
    size_t len = entriesBuffer(b, char);

    if ( length )
      *length = len;
    addBuffer(b, EOS, char);
    r = baseBuffer(b, char);

    if ( flags & BUF_MALLOC )
    { *s = PL_malloc(len+1);
      memcpy(*s, r, len+1);
      unfindBuffer(flags);
    } else
      *s = r;

    succeed;
  }

  fail;
}

int
PL_get_list_chars(term_t l, char **s, unsigned flags)
{ return PL_get_list_nchars(l, NULL, s, flags);
}


int
PL_unify_wchars_diff(term_t t, term_t tail, int flags,
		     size_t len, const pl_wchar_t *s)
{ PL_chars_t text;
  int rc;

  if ( len == (size_t)-1 )
    len = wcslen(s);

  text.text.w    = (pl_wchar_t *)s;
  text.encoding  = ENC_WCHAR;
  text.storage   = PL_CHARS_HEAP;
  text.length    = len;
  text.canonical = FALSE;

  rc = PL_unify_text(t, tail, &text, flags);
  PL_free_text(&text);

  return rc;
}

int
PL_get_wchars(term_t l, size_t *length, pl_wchar_t **s, unsigned flags)
{ GET_LD
  PL_chars_t text;

  if ( !PL_get_text(l, &text, flags) )
    return FALSE;

  PL_promote_text(&text);
  PL_save_text(&text, flags);

  if ( length )
    *length = text.length;
  *s = text.text.w;

  return TRUE;
}


int
PL_get_nchars(term_t l, size_t *length, char **s, unsigned flags)
{ GET_LD
  PL_chars_t text;

  if ( !PL_get_text(l, &text, flags) )
    return FALSE;

  if ( PL_mb_text(&text, flags) )
  { PL_save_text(&text, flags);

    if ( length )
      *length = text.length;
    *s = text.text.t;

    return TRUE;
  } else
  { PL_free_text(&text);

    return FALSE;
  }
}


int
PL_get_chars(term_t t, char **s, unsigned flags)
{ return PL_get_nchars(t, NULL, s, flags);
}

char   *Yap_TermToString(Term t, char *s, size_t sz, size_t *length, int *encoding, int flags);

char *
Yap_TermToString(Term t, char *s, size_t sz, size_t *length, int *encoding, int flags)
{
  CACHE_REGS
  Int l;

  Yap_StartSlots( PASS_REGS1 );
  l = Yap_InitSlot(t  PASS_REGS );

  { IOENC encodings[3];
    IOENC *enc;
    char *r, buf[256];

    encodings[0] = ENC_ISO_LATIN_1;
    encodings[1] = ENC_WCHAR;
    encodings[2] = ENC_UNKNOWN;

    for(enc = encodings; *enc != ENC_UNKNOWN; enc++)
      { 
	int64_t size;
	IOSTREAM *fd;

	if (s) {
	  r = s;
	} else {
	  r = buf;
	}
	fd = Sopenmem(&r, &sz, "w");
	fd->encoding = *enc;
	if ( PL_write_term(fd, l, 1200, flags) &&
	     Sputcode(EOS, fd) >= 0 &&
	     Sflush(fd) >= 0 )
	  { *encoding = *enc;
	    size = Stell64(fd);
	    if ( *enc == ENC_ISO_LATIN_1 )
	      { 
		*length = size-1;
	      } else
	      { 
		*length = (size/sizeof(pl_wchar_t))-1;
	      }
	    /* found, just check if using local space */
	    if (r == buf) {
	      char *bf = malloc(*length+1);
	      if (!bf)
		return NULL;
	      strncpy(bf,buf,*length+1);
	      r = bf;
	    }
	    /* go away */
	    Yap_CloseSlots( PASS_REGS1 );
	    return r;
	  } else
	  { Sclose(fd);
	  }
      }
    /* failed */
    if ( r != s && r != buf ) {
      Sfree(r);
    }
  }
  Yap_CloseSlots( PASS_REGS1 );
  return NULL;
}


X_API int
PL_ttymode(IOSTREAM *s)
{ GET_LD

  if ( s == Suser_input )
  { if ( !truePrologFlag(PLFLAG_TTY_CONTROL) ) /* -tty in effect */
      return PL_NOTTY;
    if ( ttymode == TTY_RAW )		/* get_single_char/1 and friends */
      return PL_RAWTTY;
    return PL_COOKEDTTY;		/* cooked (readline) input */
  } else
    return PL_NOTTY;
}

char *
PL_prompt_string(int fd)
{ if ( fd == 0 )
  { atom_t a = PrologPrompt();          /* TBD: deal with UTF-8 */
    
    if ( a )
    {     
      Atom at = YAP_AtomFromSWIAtom(a);
      if (!IsWideAtom(at)  && !IsBlob(at)) {
	return RepAtom(at)->StrOfAE;
      }
    }
  }

  return NULL;
}


X_API void
PL_prompt_next(int fd)
{ GET_LD

  if ( fd == 0 )
    LD->prompt.next = TRUE;
}

/* just a stub for now */
int
warning(const char *fm, ...)
{  va_list args;

  va_start(args, fm);
  fprintf(stderr,"warning: %s\n", fm);
  va_end(args);

  return TRUE;
}

#if defined(HAVE_SELECT) && !defined(__WINDOWS__) && !defined(__CYGWIN__)

#ifdef __WINDOWS__
#include <winsock2.h>
#endif

static int
input_on_fd(int fd)
{ fd_set rfds;
  struct timeval tv;

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  return select(fd+1, &rfds, NULL, NULL, &tv) != 0;
}

#else
#define input_on_fd(fd) 1
#endif


PL_dispatch_hook_t
PL_dispatch_hook(PL_dispatch_hook_t hook)
{ PL_dispatch_hook_t old = GD->foreign.dispatch_events;

  GD->foreign.dispatch_events = hook;
  return old;
}


X_API int
PL_dispatch(int fd, int wait)
{ if ( wait == PL_DISPATCH_INSTALLED )
    return GD->foreign.dispatch_events ? TRUE : FALSE;

  if ( GD->foreign.dispatch_events && PL_thread_self() == 1 )
  { if ( wait == PL_DISPATCH_WAIT )
    { while( !input_on_fd(fd) )
      { if ( PL_handle_signals() < 0 )
	  return FALSE;
	(*GD->foreign.dispatch_events)(fd);
      }
    } else
    { (*GD->foreign.dispatch_events)(fd);
      if ( PL_handle_signals() < 0 )
	  return FALSE;
    }
  }

  return TRUE;
}

/* SWI: int PL_get_atom(term_t t, YAP_Atom *a)
   YAP: YAP_Atom YAP_AtomOfTerm(Term) */
int PL_get_atom__LD(term_t ts, atom_t *a ARG_LD)
{
  CACHE_REGS
  YAP_Term t = Yap_GetFromSlot(ts PASS_REGS);
  if ( !IsAtomTerm(t))
    return 0;
  *a = YAP_SWIAtomFromAtom(AtomOfTerm(t));
  return 1;
}

void PL_put_term__LD(term_t d, term_t s ARG_LD)
{
  CACHE_REGS
  Yap_PutInSlot(d,Yap_GetFromSlot(s PASS_REGS) PASS_REGS);
}

term_t PL_new_term_ref__LD(ARG1_LD)
{
  CACHE_REGS
  term_t to = Yap_NewSlots(1 PASS_REGS);
  return to;
}

int PL_is_variable__LD(term_t ts ARG_LD)
{
  CACHE_REGS
  YAP_Term t = Yap_GetFromSlot(ts PASS_REGS);
  return YAP_IsVarTerm(t);
}

int PL_unify_atom__LD(term_t t, atom_t at ARG_LD)
{
  CACHE_REGS
  YAP_Term cterm = MkAtomTerm(YAP_AtomFromSWIAtom(at));
  return YAP_Unify(Yap_GetFromSlot(t PASS_REGS),cterm);
}

/* SWI: int PL_unify_integer(term_t ?t, long n)
   YAP long int  unify(YAP_Term* a, Term* b) */
int PL_unify_integer__LD(term_t t, intptr_t i ARG_LD)
{	
  CACHE_REGS
  Term iterm = MkIntegerTerm(i);
  return Yap_unify(Yap_GetFromSlot(t PASS_REGS),iterm);
}


#ifdef _WIN32

#include <windows.h>



#if O_PLMT
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PL_w32thread_raise(DWORD id, int sig)
    Sets the signalled mask for a specific Win32 thread. This is a
    partial work-around for the lack of proper asynchronous signal
    handling in the Win32 platform.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int thread_highest_id = 1;

X_API int
PL_w32thread_raise(DWORD id, int sig)
{ int i;

  if ( sig < 0 || sig > MAXSIGNAL )
    return FALSE;			/* illegal signal */

  LOCK();
  for(i = 1; i <= thread_highest_id; i++)
  { PL_thread_info_t *info = GD->thread.threads[i];

    if ( info && info->w32id == id && info->thread_data )
      { Yap_signal(sig); //raiseSignal(info->thread_data, sig);
      if ( info->w32id )
	PostThreadMessage(info->w32id, WM_SIGNALLED, 0, 0L);
      UNLOCK();
      DEBUG(1, Sdprintf("Signalled %d to thread %d\n", sig, i));
      return TRUE;
    }
  }
  UNLOCK();

  return FALSE;				/* can't find thread */
}

#else

int
PL_w32thread_raise(DWORD id, int sig)
{ return PL_raise(sig);
}

#endif
#endif /*__WINDOWS__*/


X_API int
PL_raise(int sig)
{
  if (sig < SIG_PROLOG_OFFSET) {
    Yap_signal(YAP_INT_SIGNAL);
    return 1;
  } else if (sig == SIG_PLABORT) {
    YAP_signal(0x40); /* YAP_INT_SIGNAL */
    return 1;
  }
  return 0;
}

extern size_t PL_utf8_strlen(const char *s, size_t len);

X_API size_t
PL_utf8_strlen(const char *s, size_t len)
{ return utf8_strlen(s, len);
}

void
PL_add_to_protocol(const char *buf, size_t n)
{ protocol(buf, n);
}

void
PL_license(const char *license, const char *module)
{
  /* unimplemented */
}


bool
systemMode(bool accept)
{
 return FALSE;
}

term_t
Yap_fetch_module_for_format(term_t args, YAP_Term *modp) {
  YAP_Term nmod;
  YAP_Term nt = YAP_StripModule(YAP_GetFromSlot(args), &nmod);
  *modp = YAP_SetCurrentModule(nmod);
  if (!nt) {
    return args;
  }
  return YAP_InitSlot(nt);
}

extern word pl_readline(term_t flag);

word
pl_readline(term_t flag)
{
  return 0;
}

static Term
StreamPosition(IOSTREAM *st)
{ GET_LD
  Term t[4];
  if (!st)
    st = Suser_input;
  t[0] = MkIntegerTerm(st->posbuf.charno);
  t[1] = MkIntegerTerm(st->posbuf.lineno);
  t[2] = MkIntegerTerm(st->posbuf.linepos);
  t[3] = MkIntegerTerm(st->posbuf.byteno);
  return Yap_MkApplTerm(FunctorStreamPos,4,t);
}

extern Term Yap_StreamPosition(IOSTREAM *st);

Term
Yap_StreamPosition(IOSTREAM *st)
{
  return StreamPosition(st);
}

IOSTREAM   *STD_PROTO(Yap_Scurin, (void));

IOSTREAM *
Yap_Scurin(void)
{
  GET_LD
  return Scurin;
}

int
isWideAtom(atom_t atom)
{
  Atom a = (Atom)atomValue(atom);
  return IsWideAtom(a);
}

wchar_t *
nameOfWideAtom(atom_t atom)
{
  Atom a = (Atom)atomValue(atom);
  return RepAtom(a)->WStrOfAE;
}


#if THREADS

#define COUNT_MUTEX_INITIALIZER(name) \
 { PTHREAD_MUTEX_INITIALIZER, \
   name, \
   0L \
 }

static int
recursive_attr(pthread_mutexattr_t **ap)
{ static int done;
  static pthread_mutexattr_t attr;
  int rc;

  if ( done )
  { *ap = &attr;
    return 0;
  }

  PL_LOCK(L_THREAD);
  if ( done )
  { PL_UNLOCK(L_THREAD);

    *ap = &attr;
    return 0;
  }
  if ( (rc=pthread_mutexattr_init(&attr)) )
    goto error;
#ifdef HAVE_PTHREAD_MUTEXATTR_SETTYPE
  if ( (rc=pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) )
    goto error;
#else
#ifdef HAVE_PTHREAD_MUTEXATTR_SETKIND_NP
  if ( (rc=pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_RECURSIVE_NP)) )
    goto error;
#endif
#endif

  done = TRUE;
  PL_UNLOCK(L_THREAD);
  *ap = &attr;

  return 0;

error:
  PL_UNLOCK(L_THREAD);
  return rc;
}

int
recursiveMutexInit(recursiveMutex *m)
{
  int rc;
  pthread_mutexattr_t *attr;

  if ( (rc=recursive_attr(&attr)) )
    return rc;

  return pthread_mutex_init(m, attr);

}

counting_mutex _PL_mutexes[] =
{ COUNT_MUTEX_INITIALIZER("L_MISC"),
  COUNT_MUTEX_INITIALIZER("L_ALLOC"),
  COUNT_MUTEX_INITIALIZER("L_ATOM"),
  COUNT_MUTEX_INITIALIZER("L_FLAG"),
  COUNT_MUTEX_INITIALIZER("L_FUNCTOR"),
  COUNT_MUTEX_INITIALIZER("L_RECORD"),
  COUNT_MUTEX_INITIALIZER("L_THREAD"),
  COUNT_MUTEX_INITIALIZER("L_PREDICATE"),
  COUNT_MUTEX_INITIALIZER("L_MODULE"),
  COUNT_MUTEX_INITIALIZER("L_TABLE"),
  COUNT_MUTEX_INITIALIZER("L_BREAK"),
  COUNT_MUTEX_INITIALIZER("L_FILE"),
  COUNT_MUTEX_INITIALIZER("L_PLFLAG"),
  COUNT_MUTEX_INITIALIZER("L_OP"),
  COUNT_MUTEX_INITIALIZER("L_INIT"),
  COUNT_MUTEX_INITIALIZER("L_TERM"),
  COUNT_MUTEX_INITIALIZER("L_GC"),
  COUNT_MUTEX_INITIALIZER("L_AGC"),
  COUNT_MUTEX_INITIALIZER("L_FOREIGN"),
  COUNT_MUTEX_INITIALIZER("L_OS")
};

#endif
