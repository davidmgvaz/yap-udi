/* yap2swi.h  */
/*
 * Project: SWI emulation for Yap Prolog
 * Author: Steve Moyle and Vitor Santos Costa
 * Email:  steve.moyle@comlab.ox.ac.uk
 * Date:   21 January 2002

 * Copyright (c) 2002 Steve Moyle and Vitor Santos Costa.  All rights reserved.
 

*/

#ifndef _FLI_H_INCLUDED
#define _FLI_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//=== includes ===============================================================
#ifdef          _YAP_NOT_INSTALLED_
#include	"config.h"

#ifdef __cplusplus
}
#endif

#if USE_GMP
#include <gmp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include	"YapInterface.h"
#else
#include	<Yap/src/config.h>
#if USE_GMP
#include <gmp.h>
#endif
#include	<Yap/YapInterface.h>
#endif
#include	<stdarg.h>
#include	<stdlib.h>
#include        <stddef.h>
#include        <wchar.h>
#if HAVE_TIME_H
#include <time.h>
#endif

#ifndef X_API
#if (defined(_MSC_VER) || defined(__MINGW32__)) && defined(PL_KERNEL)
#define X_API __declspec(dllexport)
#else
#define X_API
#endif
#endif


		 /*******************************
		 *	       EXPORT		*
		 *******************************/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
See SWI-Prolog.h, containing the same code   for  an explanation on this
stuff.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef _PL_EXPORT_DONE
#define _PL_EXPORT_DONE

#if (defined(__WINDOWS__) || defined(__CYGWIN__) || defined(__MINGW32__)) && !defined(__LCC__)
#define HAVE_DECLSPEC
#endif

#ifdef HAVE_DECLSPEC
# ifdef PL_KERNEL
#define PL_EXPORT(type)		__declspec(dllexport) type
#define PL_EXPORT_DATA(type)	__declspec(dllexport) type
#define install_t	 	void
# else
#  ifdef __BORLANDC__
#define PL_EXPORT(type)	 	type _stdcall
#define PL_EXPORT_DATA(type)	extern type
#  else
#define PL_EXPORT(type)	 	extern type
#define PL_EXPORT_DATA(type)	__declspec(dllimport) type
#  endif
#define install_t	 	__declspec(dllexport) void
# endif
#else /*HAVE_DECLSPEC*/
#define PL_EXPORT(type)	 	extern type
#define PL_EXPORT_DATA(type)	extern type
#define install_t	 	void
#endif /*HAVE_DECLSPEC*/
#endif /*_PL_EXPORT_DONE*/


                 /*******************************
                 *        GCC ATTRIBUTES        *
                 *******************************/

#if __GNUC__ >= 4
#define WUNUSED __attribute__((warn_unused_result))
#else
#define WUNUSED
#endif


                 /*******************************
                 *             TYPES            *
                 *******************************/


#ifdef __WINDOWS__
#ifndef INT64_T_DEFINED
#define INT64_T_DEFINED 1
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#if (_MSC_VER < 1300) && !defined(__MINGW32__)
typedef long intptr_t;
typedef unsigned long uintptr_t;
#endif
#endif
#else
#include <inttypes.h>			/* more portable than stdint.h */
#endif

typedef	uintptr_t    term_t;
typedef	void *module_t;
typedef	void *record_t;
typedef uintptr_t	atom_t;
typedef	YAP_Term    *predicate_t;
typedef struct  open_query_struct *qid_t;
typedef uintptr_t    functor_t;
typedef int     (*PL_agc_hook_t)(atom_t);
typedef unsigned long	foreign_t;	/* return type of foreign functions */
typedef wchar_t pl_wchar_t;             /* wide character support */
#include <inttypes.h>			/* more portable than stdint.h */
#if  !defined(_MSC_VER) 
typedef uintptr_t	PL_fid_t;	/* opaque foreign context handle */
#endif

#define O_STRING 1

typedef void *pl_function_t;

#define fid_t PL_fid_t			/* avoid AIX name-clash */

typedef struct _PL_extension
{ const char 		*predicate_name;	/* Name of the predicate */
  short		arity;			/* Arity of the predicate */
  pl_function_t	function;		/* Implementing functions */
  short		flags;			/* Or of PL_FA_... */
} PL_extension;

typedef struct
{ unsigned long	    local_size;		/* Stack sizes */
  unsigned long	    global_size;
  unsigned long	    trail_size;
  unsigned long	    argument_size;
  char *	    alias;		/* alias name */
  int		  (*cancel)(int id);	/* cancel function */
  void *	    reserved[5];	/* reserved for extensions */
} PL_thread_attr_t;

typedef void *PL_engine_t;


#define PL_FA_NOTRACE		(0x01)	/* foreign cannot be traced */
#define PL_FA_TRANSPARENT	(0x02)	/* foreign is module transparent */
#define PL_FA_NONDETERMINISTIC	(0x04)	/* foreign is non-deterministic */
#define PL_FA_VARARGS		(0x08)	/* call using t0, ac, ctx */
#define PL_FA_CREF		(0x10)	/* Internal: has clause-reference */
#define PL_FA_ISO		(0x20)	/* Internal: ISO core predicate */

/* begin from pl-itf.h */
#define PL_VARIABLE      (1)            /* nothing */
#define PL_ATOM          (2)            /* const char * */
#define PL_INTEGER       (3)            /* int */
#define PL_FLOAT         (4)            /* double */
#define PL_STRING        (5)            /* const char * */
#define PL_TERM          (6)

                                        /* PL_unify_term() */
#define PL_FUNCTOR       (10)           /* functor_t, arg ... */
#define PL_LIST          (11)           /* length, arg ... */
#define PL_CHARS         (12)           /* const char * */
#define PL_POINTER       (13)           /* void * */
                                        /* PlArg::PlArg(text, type) */
#define PL_CODE_LIST     (14)           /* [ascii...] */
#define PL_CHAR_LIST     (15)           /* [h,e,l,l,o] */
#define PL_BOOL          (16)           /* PL_set_feature() */
#define PL_FUNCTOR_CHARS (17)           /* PL_unify_term() */
#define PL_PREDICATE_INDICATOR (18)    /* predicate_t (Procedure) */
#define PL_SHORT         (19)           /* short */
#define PL_INT           (20)           /* int */
#define PL_LONG          (21)           /* long */
#define PL_DOUBLE        (22)           /* double */
#define PL_NCHARS	 (23)		/* unsigned, const char * */
#define PL_UTF8_CHARS	 (24)		/* const char * */
#define PL_UTF8_STRING	 (25)		/* const char * */
#define PL_INT64	 (26)		/* int64_t */
#define PL_NUTF8_CHARS	 (27)		/* unsigned, const char * */
#define PL_NUTF8_CODES	 (29)		/* unsigned, const char * */
#define PL_NUTF8_STRING	 (30)		/* unsigned, const char * */
#define PL_NWCHARS	 (31)		/* unsigned, const wchar_t * */
#define PL_NWCODES	 (32)		/* unsigned, const wchar_t * */
#define PL_NWSTRING	 (33)		/* unsigned, const wchar_t * */
#define PL_MBCHARS	 (34)		/* const char * */
#define PL_MBCODES	 (35)		/* const char * */
#define PL_MBSTRING	 (36)		/* const char * */
#define PL_INTPTR        (37)           /* intptr_t */
#define PL_CHAR          (38)           /* int */
#define PL_CODE          (39)           /* int */
#define PL_BYTE          (40)           /* int */
					/* PL_skip_list() */
#define PL_PARTIAL_LIST	 (41)		/* a partial list */
#define PL_CYCLIC_TERM	 (42)		/* a cyclic list/term */
#define PL_NOT_A_LIST	 (43)		/* Object is not a list */

#define CVT_ATOM	0x0001
#define CVT_STRING	0x0002
#define CVT_LIST	0x0004
#define CVT_INTEGER	0x0008
#define CVT_FLOAT	0x0010
#define CVT_VARIABLE	0x0020
#define CVT_NUMBER	(CVT_INTEGER|CVT_FLOAT)
#define CVT_ATOMIC	(CVT_NUMBER|CVT_ATOM|CVT_STRING)
#define CVT_WRITE	0x0040		/* as of version 3.2.10 */
#define CVT_WRITE_CANONICAL	0x0080		/* as of version 3.2.10 */
#define CVT_ALL		(CVT_ATOMIC|CVT_LIST)
#define CVT_MASK	0x00ff

#define CVT_EXCEPTION	0x10000
#define CVT_VARNOFAIL	0x20000		/* return 2 if argument is unbound */

#define BUF_DISCARDABLE	0x0000
#define BUF_RING	0x0100
#define BUF_MALLOC	0x0200

#define PL_ENGINE_MAIN      ((PL_engine_t)0x1)
#define PL_ENGINE_CURRENT   ((PL_engine_t)0x2)

#define PL_ENGINE_SET   0               /* engine set successfully */
#define PL_ENGINE_INVAL 2               /* engine doesn't exist */
#define PL_ENGINE_INUSE 3               /* engine is in use */

#define	PL_ACTION_TRACE		1	/* switch to trace mode */
#define PL_ACTION_DEBUG		2	/* switch to debug mode */
#define PL_ACTION_BACKTRACE	3	/* show a backtrace (stack dump) */
#define PL_ACTION_BREAK		4	/* create a break environment */
#define PL_ACTION_HALT		5	/* halt Prolog execution */
#define PL_ACTION_ABORT		6	/* generate a Prolog abort */
					/* 7: Obsolete PL_ACTION_SYMBOLFILE */
#define PL_ACTION_WRITE		8	/* write via Prolog i/o buffer */
#define PL_ACTION_FLUSH		9	/* Flush Prolog i/o buffer */
#define PL_ACTION_GUIAPP	10	/* Win32: set when this is a gui */
#define PL_ACTION_ATTACH_CONSOLE 11	/* MT: Attach a console */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Foreign language interface definitions.  Note that these macros MUST  be
consistent  with  the  definitions  in  pl-itf.h, which is included with
users foreign language code.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define NOTRACE PL_FA_NOTRACE
#define META    PL_FA_TRANSPARENT
#define NDET	PL_FA_NONDETERMINISTIC
#define VA	PL_FA_VARARGS
#define CREF	PL_FA_CREF
#define ISO	PL_FA_ISO

typedef enum
{ FRG_FIRST_CALL = 0,		/* Initial call */
  FRG_CUTTED     = 1,		/* Context was cutted */
  FRG_REDO	 = 2		/* Normal redo */
} frg_code;

struct foreign_context
{ uintptr_t		context;	/* context value */
  frg_code		control;	/* FRG_* action */
  struct PL_local_data *engine;		/* invoking engine */
};

typedef struct foreign_context *control_t;

#define PRED_IMPL(name, arity, fname, flags) \
        foreign_t \
        pl_ ## fname ## _va(term_t PL__t0, int PL__ac, control_t PL__ctx)

#define CTX_CNTRL ForeignControl(PL__ctx)
#define CTX_PTR   ForeignContextPtr(PL__ctx)
#define CTX_INT   ForeignContextInt(PL__ctx)
#define CTX_ARITY PL__ac

#define BeginPredDefs(id) \
        const PL_extension PL_predicates_from_ ## id[] = {
#define PRED_DEF(name, arity, fname, flags) \
  { name, arity, pl_ ## fname ## _va, (flags)|PL_FA_VARARGS },
#define EndPredDefs \
        { NULL, 0, NULL, 0 } \
        };

#define FRG_REDO_MASK	0x00000003L
#define FRG_REDO_BITS	2
#define REDO_INT	0x02		/* Returned an integer */
#define REDO_PTR	0x03		/* returned a pointer */

#define ForeignRedoIntVal(v)	(((uintptr_t)(v)<<FRG_REDO_BITS)|REDO_INT)
#define ForeignRedoPtrVal(v)	(((uintptr_t)(v))|REDO_PTR)

#define ForeignRedoInt(v)	return ForeignRedoIntVal(v)
#define ForeignRedoPtr(v)	return ForeignRedoPtrVal(v)

#define ForeignControl(h)	((h)->control)
#define ForeignContextInt(h)	((intptr_t)(h)->context)
#define ForeignContextPtr(h)	((void *)(h)->context)
#define ForeignEngine(h)	((h)->engine)

#define FRG(n, a, f, flags) { n, a, f, flags }
#define LFRG(n, a, f, flags) { n, a, f, flags }

/* end from pl-itf.h */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Output   representation   for   PL_get_chars()     and    friends.   The
prepresentation type REP_FN is for   PL_get_file_name()  and friends. On
Windows we use UTF-8 which is translated   by the `XOS' layer to Windows
UNICODE file functions.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define REP_ISO_LATIN_1 0x0000		/* output representation */
#define REP_UTF8 	0x1000
#define REP_MB		0x2000
#ifdef __WINDOWS__
#define REP_FN		REP_UTF8
#else
#define REP_FN		REP_MB
#endif

#define PL_DIFF_LIST	0x20000		/* PL_unify_chars() */

#ifdef SIO_MAGIC			/* defined from <SWI-Stream.h> */

#define FF_NOCREATE	 0x4000		/* Fail if flag is non-existent */
#define FF_MASK		 0xf000

		 /*******************************
		 *	  STREAM SUPPORT	*
		 *******************************/

/* Make IOSTREAM known to Prolog */
#define PL_open_stream  PL_unify_stream	/* compatibility */
PL_EXPORT(int)  	PL_unify_stream(term_t t, IOSTREAM *s);
PL_EXPORT(int)  	PL_get_stream_handle(term_t t, IOSTREAM **s);
PL_EXPORT(int) 		PL_release_stream(IOSTREAM *s);
PL_EXPORT(IOSTREAM *)	PL_open_resource(module_t m,
					 const char *name,
					 const char *rc_class,
					 const char *mode);

PL_EXPORT(IOSTREAM *)*_PL_streams(void);	/* base of streams */
#ifndef PL_KERNEL
#define Suser_input  (_PL_streams()[0])
#define Suser_output (_PL_streams()[1])
#define Suser_error  (_PL_streams()[2])
#endif

#define PL_WRT_QUOTED		0x01	/* quote atoms */
#define PL_WRT_IGNOREOPS	0x02	/* ignore list/operators */
#define PL_WRT_NUMBERVARS	0x04	/* print $VAR(N) as a variable */
#define PL_WRT_PORTRAY		0x08	/* call portray */
#define PL_WRT_CHARESCAPES	0x10	/* Output ISO escape sequences */
#define PL_WRT_BACKQUOTED_STRING 0x20	/* Write strings as `...` */
					/* Write attributed variables */
#define PL_WRT_ATTVAR_IGNORE	0x040	/* Default: just write the var */
#define PL_WRT_ATTVAR_DOTS	0x080	/* Write as Var{...} */
#define PL_WRT_ATTVAR_WRITE	0x100	/* Write as Var{Attributes} */
#define PL_WRT_ATTVAR_PORTRAY	0x200	/* Use Module:portray_attrs/2 */
#define PL_WRT_ATTVAR_MASK \
	(PL_WRT_ATTVAR_IGNORE | \
	 PL_WRT_ATTVAR_DOTS | \
	 PL_WRT_ATTVAR_WRITE | \
	 PL_WRT_ATTVAR_PORTRAY)
#define PL_WRT_BLOB_PORTRAY	0x400	/* Use portray to emit non-text blobs */

PL_EXPORT(int) PL_write_term(IOSTREAM *s,
			     term_t term,
			     int precedence,
			     int flags);

					/* PL_ttymode() results */
#define PL_NOTTY	0		/* -tty in effect */
#define PL_RAWTTY	1		/* get_single_char/1 */
#define PL_COOKEDTTY	2		/* normal input */

PL_EXPORT(int)		PL_ttymode(IOSTREAM *s);

#endif /*SIO_MAGIC*/

PL_EXPORT(int) PL_chars_to_term(const char *chars,
				 term_t term);


		 /*******************************
		 *	     CALL-BACK		*
		 *******************************/

#ifdef PL_KERNEL
#define PL_Q_DEBUG		0x01	/* = TRUE for backward compatibility */
#endif
#define PL_Q_NORMAL		0x02	/* normal usage */
#define PL_Q_NODEBUG		0x04	/* use this one */
#define PL_Q_CATCH_EXCEPTION	0x08	/* handle exceptions in C */
#define PL_Q_PASS_EXCEPTION	0x10	/* pass to parent environment */
#ifdef PL_KERNEL
#define PL_Q_DETERMINISTIC	0x20	/* call was deterministic */
#endif

#define PL_fail		return FALSE	/* fail */
#define PL_succeed	return TRUE	/* success */

extern X_API PL_agc_hook_t PL_agc_hook(PL_agc_hook_t);
extern X_API char* PL_atom_chars(atom_t);
extern X_API char* PL_atom_nchars(atom_t, size_t *);
extern X_API term_t PL_copy_term_ref(term_t);
extern X_API term_t PL_new_term_ref(void);
extern X_API term_t PL_new_term_refs(int);
extern X_API void PL_reset_term_refs(term_t);
/* begin PL_get_* functions =============================*/
extern X_API int PL_get_arg(int, term_t, term_t);
extern X_API int _PL_get_arg(int, term_t, term_t);
extern X_API int PL_get_atom(term_t, atom_t *);
extern X_API int PL_get_atom_chars(term_t, char **);
extern X_API int PL_get_atom_nchars(term_t, size_t *, char **);
extern X_API int PL_get_bool(term_t, int *);
extern X_API int PL_get_chars(term_t, char **, unsigned);
extern X_API int PL_get_nchars(term_t, size_t *, char **, unsigned);
extern X_API int PL_get_wchars(term_t, size_t *, wchar_t **, unsigned);
extern X_API int PL_get_functor(term_t, functor_t *);
extern X_API int PL_get_float(term_t, double *);
extern X_API int PL_get_head(term_t, term_t);
extern X_API int PL_get_int64(term_t, int64_t *);
extern X_API int PL_get_integer(term_t, int *);
extern X_API int PL_get_list(term_t, term_t, term_t);
extern X_API int PL_get_long(term_t, long *);
extern X_API int PL_get_list_chars(term_t, char **, unsigned);
extern X_API int PL_get_list_nchars(term_t, size_t *, char **, unsigned);
extern X_API int PL_get_module(term_t, module_t *);
extern X_API module_t PL_context(void);
extern X_API int PL_strip_module(term_t, module_t *, term_t);
extern X_API atom_t PL_module_name(module_t);
extern X_API module_t PL_new_module(atom_t);
extern X_API int PL_get_name_arity(term_t, atom_t *, int *);
extern X_API int PL_get_nil(term_t);
extern X_API int PL_get_pointer(term_t, void **);
extern X_API int PL_get_intptr(term_t, intptr_t *);
extern X_API int PL_get_uintptr(term_t, uintptr_t *);
extern X_API int PL_get_tail(term_t, term_t);
/* end PL_get_* functions  =============================*/
/* begin PL_new_* functions =============================*/
extern X_API atom_t PL_new_atom(const char *);
extern X_API atom_t PL_new_atom_nchars(size_t, const char *);
extern X_API atom_t PL_new_atom_wchars(size_t, const pl_wchar_t *);
extern X_API char *PL_atom_nchars(atom_t, size_t *);
extern X_API pl_wchar_t *PL_atom_wchars(atom_t, size_t *);
extern X_API functor_t PL_new_functor(atom_t, int);
extern X_API atom_t PL_functor_name(functor_t);
extern X_API int PL_functor_arity(functor_t);
/* end PL_new_* functions =============================*/
/* begin PL_put_* functions =============================*/
extern X_API int PL_cons_functor(term_t, functor_t,...);
extern X_API int PL_cons_functor_v(term_t, functor_t,term_t);
extern X_API int PL_cons_list(term_t, term_t, term_t);
extern X_API int PL_put_atom(term_t, atom_t);
extern X_API int PL_put_atom_chars(term_t, const char *);
extern X_API int PL_put_atom_nchars(term_t, size_t ,const char *);
extern X_API int PL_put_float(term_t, double);
extern X_API int PL_put_functor(term_t, functor_t t);
extern X_API int PL_put_int64(term_t, int64_t);
extern X_API int PL_put_integer(term_t, long);
extern X_API int PL_put_list(term_t);
extern X_API int PL_put_list_chars(term_t, const char *);
extern X_API void PL_put_nil(term_t);
extern X_API int PL_put_pointer(term_t, void *);
extern X_API int PL_put_string_chars(term_t, const char *);
extern X_API int PL_put_string_nchars(term_t, size_t, const char *);
extern X_API int PL_put_term(term_t, term_t);
extern X_API int PL_put_variable(term_t);
extern X_API  int PL_compare(term_t, term_t);
/* end PL_put_* functions =============================*/
/* begin PL_unify_* functions =============================*/
extern X_API  int PL_unify(term_t, term_t);
extern X_API  int PL_unify_atom(term_t, atom_t);
extern X_API  int PL_unify_arg(int, term_t, atom_t);
extern X_API  int PL_unify_atom_chars(term_t, const char *);
extern X_API  int PL_unify_atom_nchars(term_t, size_t len, const char *);
extern X_API  int PL_unify_float(term_t, double);
extern X_API  int PL_unify_functor(term_t, functor_t);
extern X_API  int PL_unify_int64(term_t, int64_t);
extern X_API  int PL_unify_integer(term_t, long);
extern X_API  int PL_unify_list(term_t, term_t, term_t);
extern X_API  int PL_unify_list_chars(term_t, const char *);
extern X_API  int PL_unify_list_ncodes(term_t, size_t, const char *);
extern X_API  int PL_unify_nil(term_t);
extern X_API  int PL_unify_pointer(term_t, void *);
extern X_API  int PL_unify_bool(term_t, int);
extern X_API  int PL_unify_string_chars(term_t, const char *);
extern X_API  int PL_unify_string_nchars(term_t, size_t, const char *);
extern X_API  int PL_unify_term(term_t,...);
extern X_API  int PL_unify_chars(term_t, int, size_t, const char *);
extern X_API  int PL_unify_chars_diff(term_t, term_t, int, size_t, const char *);
		 /*******************************
		 *	       LISTS		*
		 *******************************/

PL_EXPORT(int)		PL_skip_list(term_t list, term_t tail, size_t *len);


extern X_API  int PL_unify_wchars(term_t, int, size_t, const pl_wchar_t *);
extern X_API  int PL_unify_wchars_diff(term_t, term_t, int, size_t, const pl_wchar_t *);
extern X_API  int PL_chars_to_term(const char *,term_t);
/* begin PL_is_* functions =============================*/
extern X_API  int PL_is_atom(term_t);
extern X_API  int PL_is_atomic(term_t);
extern X_API  int PL_is_compound(term_t);
extern X_API  int PL_is_float(term_t);
extern X_API  int PL_is_functor(term_t, functor_t);
extern X_API  int PL_is_ground(term_t);
extern X_API  int PL_is_callable(term_t);
extern X_API  int PL_is_integer(term_t);
extern X_API  int PL_is_list(term_t);
extern X_API  int PL_is_number(term_t);
extern X_API  int PL_is_string(term_t);
extern X_API  int PL_is_variable(term_t);
extern X_API  int PL_term_type(term_t);
extern X_API  int PL_is_inf(term_t);
/* end PL_is_* functions =============================*/
extern X_API void PL_halt(int);
extern X_API  int  PL_initialise(int, char **);
extern X_API  int  PL_is_initialised(int *, char ***);
extern X_API void PL_close_foreign_frame(fid_t);
extern X_API void PL_discard_foreign_frame(fid_t);
extern X_API void PL_rewind_foreign_frame(fid_t);
extern X_API fid_t PL_open_foreign_frame(void);
extern X_API int PL_raise_exception(term_t);
extern X_API int PL_throw(term_t);
extern X_API void PL_clear_exception(void);
extern X_API void PL_register_atom(atom_t);
extern X_API void PL_unregister_atom(atom_t);
extern X_API predicate_t PL_pred(functor_t, module_t);
extern X_API predicate_t PL_predicate(const char *, int, const char *);
#define GP_NAMEARITY	0x100		/* or'ed mask */
extern X_API int PL_unify_predicate(term_t head, predicate_t pred, int how);
extern X_API void PL_predicate_info(predicate_t, atom_t *, int *, module_t *);
extern X_API qid_t PL_open_query(module_t, int, predicate_t, term_t);
extern X_API int PL_next_solution(qid_t);
extern X_API void PL_cut_query(qid_t);
extern X_API void PL_close_query(qid_t);
extern X_API int PL_toplevel(void);
extern X_API term_t PL_exception(qid_t);
extern X_API term_t PL_exception(qid_t);
extern X_API int PL_call_predicate(module_t, int, predicate_t, term_t);
extern X_API int PL_call(term_t, module_t);
extern X_API void PL_register_foreign(const char *, int, pl_function_t, int);
extern X_API void PL_register_foreign_in_module(const char *, const char *, int, pl_function_t, int);
extern X_API void PL_register_extensions(const PL_extension *);
extern X_API void PL_register_extensions_in_module(const char *module, const PL_extension *);
extern X_API void PL_load_extensions(const PL_extension *);
extern X_API int PL_handle_signals(void);
extern X_API int  PL_thread_self(void);
extern X_API int  PL_unify_thread_id(term_t, int);
extern X_API int PL_thread_attach_engine(const PL_thread_attr_t *);
extern X_API int PL_thread_destroy_engine(void);
extern X_API int PL_thread_at_exit(void (*)(void *), void *, int);
extern X_API PL_engine_t PL_create_engine(const PL_thread_attr_t *);
extern X_API int PL_destroy_engine(PL_engine_t);
extern X_API int PL_set_engine(PL_engine_t,PL_engine_t *);
extern X_API int PL_get_string(term_t, char **, size_t *);
extern X_API int PL_get_string_chars(term_t, char **, size_t *);
extern X_API record_t PL_record(term_t);
extern X_API int PL_recorded(record_t, term_t);
extern X_API record_t PL_duplicate_record(record_t);
extern X_API void PL_erase(record_t);
/* only partial implementation, does not guarantee export between different architectures and versions of YAP */
extern X_API char *PL_record_external(term_t, size_t *);
extern X_API int PL_recorded_external(char *, term_t);
extern X_API int PL_erase_external(char *);
extern X_API int PL_action(int,...);
extern X_API void PL_on_halt(void (*)(int, void *), void *);
extern X_API void *PL_malloc(size_t);
extern X_API void *PL_realloc(void*,size_t);
extern X_API void PL_free(void *);
extern X_API int  PL_eval_expression_to_int64_ex(term_t t, int64_t *val);
extern X_API void  PL_cleanup_fork(void);
extern X_API int PL_get_signum_ex(term_t sig, int *n);

extern X_API size_t PL_utf8_strlen(const char *s, size_t len);

extern X_API int PL_unify_list_codes(term_t l, const char *chars);

PL_EXPORT(void)		PL_add_to_protocol(const char *buf, size_t count);

#define PL_SIGSYNC	0x00010000	/* call handler synchronously */
#define PL_SIGNOFRAME	0x00020000	/* Do not create a Prolog frame */

extern X_API void (*PL_signal(int sig, void (*func)(int)))(int);
extern X_API void  PL_fatal_error(const char *msg);

extern X_API int Sprintf(const char * fm,...);
extern X_API int Sdprintf(const char *,...);

extern char *PL_prompt_string(int fd);

                 /*******************************
                 *        FILENAME SUPPORT      *
                 *******************************/

#define PL_FILE_ABSOLUTE        0x01    /* return absolute path */
#define PL_FILE_OSPATH          0x02    /* return path in OS notation */
#define PL_FILE_SEARCH          0x04    /* use file_search_path */
#define PL_FILE_EXIST           0x08    /* demand file to exist */
#define PL_FILE_READ            0x10    /* demand read-access */
#define PL_FILE_WRITE           0x20    /* demand write-access */
#define PL_FILE_EXECUTE         0x40    /* demand execute-access */
#define PL_FILE_NOERRORS        0x80    /* do not raise exceptions */

PL_EXPORT(int)          PL_get_file_name(term_t n, char **name, int flags);
PL_EXPORT(int)          PL_get_file_nameW(term_t n, wchar_t **name, int flags);
PL_EXPORT(void)         PL_changed_cwd(void); /* foreign code changed CWD */
PL_EXPORT(const char *) PL_cwd(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
NOTE: the functions in this section are   not  documented, as as yet not
adviced for public usage.  They  are   intended  to  provide an abstract
interface for the GNU readline  interface   as  defined in pl-rl.c. This
abstract interface is necessary to make an embeddable system without the
readline overhead.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
					/* PL_dispatch() modes */
#define PL_DISPATCH_NOWAIT    0		/* Dispatch only once */
#define PL_DISPATCH_WAIT      1		/* Dispatch till input available */
#define PL_DISPATCH_INSTALLED 2		/* dispatch function installed? */

typedef int  (*PL_dispatch_hook_t)(int fd);

extern X_API int PL_dispatch(int fd, int wait);
PL_EXPORT(PL_dispatch_hook_t) 	PL_dispatch_hook(PL_dispatch_hook_t);
PL_EXPORT(void)		PL_add_to_protocol(const char *buf, size_t count);
PL_EXPORT(char *)	PL_prompt_string(int fd);
PL_EXPORT(void)		PL_write_prompt(int dowrite);
PL_EXPORT(void)		PL_prompt_next(int fd);
PL_EXPORT(char *)	PL_atom_generator(const char *prefix, int state);
PL_EXPORT(pl_wchar_t*)  PL_atom_generator_w(const pl_wchar_t *pref,
					    pl_wchar_t *buffer,
					    size_t buflen,
					    int state);

		 /*******************************
		 *	 WINDOWS MESSAGES	*
		 *******************************/

#if defined(_MSC_VER) || defined(__MINGW32__)	/* <windows.h> is included */
#define PL_MSG_EXCEPTION_RAISED -1
#define PL_MSG_IGNORED 0
#define PL_MSG_HANDLED 1

#include <windows.h>

PL_EXPORT(LRESULT)	PL_win_message_proc(HWND hwnd,
					    UINT message,
					    WPARAM wParam,
					    LPARAM lParam);
#endif /*_WINDOWS_*/


		/********************************
		*         QUERY PROLOG          *
		*********************************/

#define PL_QUERY_ARGC		1	/* return main() argc */
#define PL_QUERY_ARGV		2	/* return main() argv */
					/* 3: Obsolete PL_QUERY_SYMBOLFILE */
					/* 4: Obsolete PL_QUERY_ORGSYMBOLFILE*/
#define PL_QUERY_GETC		5	/* Read character from terminal */
#define PL_QUERY_MAX_INTEGER	6	/* largest integer */
#define PL_QUERY_MIN_INTEGER	7	/* smallest integer */
#define PL_QUERY_MAX_TAGGED_INT	8	/* largest tagged integer */
#define PL_QUERY_MIN_TAGGED_INT	9	/* smallest tagged integer */
#define PL_QUERY_VERSION        10	/* 207006 = 2.7.6 */
#define PL_QUE_MAX_THREADS	11	/* maximum thread count */
#define PL_QUERY_ENCODING	12	/* I/O encoding */
#define PL_QUERY_USER_CPU	13	/* User CPU in milliseconds */
#define PL_QUERY_HALTING	14	/* If TRUE, we are in PL_cleanup() */

X_API intptr_t		PL_query(int);	/* get information from Prolog */

		 /*******************************
		 *	      ERRORS		*
		 *******************************/

PL_EXPORT(int) 		PL_get_atom_ex(term_t t, atom_t *a);
PL_EXPORT(int) 		PL_get_integer_ex(term_t t, int *i);
PL_EXPORT(int) 		PL_get_long_ex(term_t t, long *i);
PL_EXPORT(int) 		PL_get_int64_ex(term_t t, int64_t *i);
PL_EXPORT(int) 		PL_get_intptr_ex(term_t t, intptr_t *i);
PL_EXPORT(int) 		PL_get_size_ex(term_t t, size_t *i);
PL_EXPORT(int) 		PL_get_bool_ex(term_t t, int *i);
PL_EXPORT(int) 		PL_get_float_ex(term_t t, double *f);
PL_EXPORT(int) 		PL_get_char_ex(term_t t, int *p, int eof);
PL_EXPORT(int) 		PL_unify_bool_ex(term_t t, int val);
PL_EXPORT(int) 		PL_get_pointer_ex(term_t t, void **addrp);
PL_EXPORT(int) 		PL_unify_list_ex(term_t l, term_t h, term_t t);
PL_EXPORT(int) 		PL_unify_nil_ex(term_t l);
PL_EXPORT(int) 		PL_get_list_ex(term_t l, term_t h, term_t t);
PL_EXPORT(int) 		PL_get_nil_ex(term_t l);

PL_EXPORT(int)		PL_instantiation_error(term_t culprit);
PL_EXPORT(int)		PL_representation_error(const char *resource);
PL_EXPORT(int)		PL_type_error(const char *expected, term_t culprit);
PL_EXPORT(int)		PL_domain_error(const char *expected, term_t culprit);
PL_EXPORT(int)		PL_existence_error(const char *type, term_t culprit);
PL_EXPORT(int)		PL_permission_error(const char *operation,
					    const char *type, term_t culprit);
PL_EXPORT(int)		PL_resource_error(const char *resource);


		 /*******************************
		 *	       BLOBS		*
		 *******************************/

#define PL_BLOB_MAGIC_B	0x75293a00	/* Magic to validate a blob-type */
#define PL_BLOB_VERSION 1		/* Current version */
#define PL_BLOB_MAGIC	(PL_BLOB_MAGIC_B|PL_BLOB_VERSION)

#define PL_BLOB_UNIQUE	0x01		/* Blob content is unique */
#define PL_BLOB_TEXT	0x02		/* blob contains text */
#define PL_BLOB_NOCOPY	0x04		/* do not copy the data */
#define PL_BLOB_WCHAR	0x08		/* wide character string */

typedef struct PL_blob_t
{ uintptr_t		magic;		/* PL_BLOB_MAGIC */
  uintptr_t		flags;		/* PL_BLOB_* */
  char *		name;		/* name of the type */
  int			(*release)(atom_t a);
  int			(*compare)(atom_t a, atom_t b);
#ifdef SIO_MAGIC
  int			(*write)(IOSTREAM *s, atom_t a, int flags);
#else
  int			(*write)(void *s, atom_t a, int flags);
#endif
  void			(*acquire)(atom_t a);
#ifdef SIO_MAGIC
  int			(*save)(atom_t a, IOSTREAM *s);
  atom_t		(*load)(IOSTREAM *s);
#else
  int			(*save)(atom_t a, void*);
  atom_t		(*load)(void *s);
#endif
					/* private */
  void *		reserved[10];	/* for future extension */
  int			registered;	/* Already registered? */
  int			rank;		/* Rank for ordering atoms */
  struct PL_blob_t *    next;		/* next in registered type-chain */
  atom_t		atom_name;	/* Name as atom */
} PL_blob_t;

PL_EXPORT(int)		PL_is_blob(term_t t, PL_blob_t **type);
PL_EXPORT(int)		PL_unify_blob(term_t t, void *blob, size_t len,
				      PL_blob_t *type);
PL_EXPORT(int)		PL_put_blob(term_t t, void *blob, size_t len,
				    PL_blob_t *type);
PL_EXPORT(int)		PL_get_blob(term_t t, void **blob, size_t *len,
				    PL_blob_t **type);

PL_EXPORT(void*)	PL_blob_data(atom_t a,
				     size_t *len,
				     struct PL_blob_t **type);

PL_EXPORT(void)		PL_register_blob_type(PL_blob_t *type);
PL_EXPORT(PL_blob_t*)	PL_find_blob_type(const char* name);
PL_EXPORT(PL_blob_t*)	YAP_find_blob_type(YAP_Atom at);
PL_EXPORT(int)		PL_unregister_blob_type(PL_blob_t *type);
PL_EXPORT(int)		PL_raise(int sig);


#if USE_GMP

PL_EXPORT(int)		PL_get_mpz(term_t t, mpz_t mpz);
PL_EXPORT(int)		PL_unify_mpz(term_t t, mpz_t mpz);
PL_EXPORT(int)		PL_get_mpq(term_t t, mpq_t mpz);
PL_EXPORT(int)		PL_unify_mpq(term_t t, mpq_t mpz);

#endif

extern X_API  const char *PL_cwd(void);

void swi_install(void);

X_API int PL_warning(const char *msg, ...);


		/********************************
		* NON-DETERMINISTIC CALL/RETURN *
		*********************************/

/*  Note 1: Non-deterministic foreign functions may also use the deterministic
    return methods PL_succeed and PL_fail.

    Note 2: The argument to PL_retry is a sizeof(ptr)-2 bits signed
    integer (use type intptr_t).
*/

#define PL_FIRST_CALL		(0)
#define PL_CUTTED		(1) /* deprecated */
#define PL_PRUNED		(1)
#define PL_REDO			(2)

#define PL_retry(n)		return _PL_retry(n)
#define PL_retry_address(a)	return _PL_retry_address(a)

PL_EXPORT(foreign_t)	_PL_retry(intptr_t);
PL_EXPORT(foreign_t)	_PL_retry_address(void *);
PL_EXPORT(int)	 	PL_foreign_control(control_t);
PL_EXPORT(intptr_t)	PL_foreign_context(control_t);
PL_EXPORT(void *)	PL_foreign_context_address(control_t);

typedef struct SWI_IO {
  functor_t f; 
  void *get_c;
  void *put_c;
  void *get_w;
  void *put_w;
  void *flush_s;
  void *close_s;
  void *get_stream_handle;
  void *get_stream_position;
} swi_io_struct;

/*  SWI stream info */
PL_EXPORT(void)         PL_YAP_InitSWIIO(struct SWI_IO *swio);

#ifdef __cplusplus
}
#endif

#endif /* _FLI_H_INCLUDED */

