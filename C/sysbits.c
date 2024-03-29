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
* File:		sysbits.c						 *
* Last rev:	4/03/88							 *
* mods:									 *
* comments:	very much machine dependent routines			 *
*									 *
*************************************************************************/
#ifdef SCCS
static char SccsId[] = "%W% %G%";
#endif

/*
 * In this routine we shall try to include the inevitably machine dependant
 * routines. These include, for the moment : Time, A rudimentary form of
 * signal handling, OS calls,
 *
 * Vitor Santos Costa, February 1987
 *
 */

/* windows.h does not like absmi.h, this
   should fix it for now */
#include "absmi.h"
#include "yapio.h"
#include "alloc.h"
#include <math.h>
#if STDC_HEADERS
#include <stdlib.h>
#endif
#if HAVE_WINDOWS_H
#include <windows.h>
#endif
#if HAVE_SYS_TIME_H && !_MSC_VER
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_WAIT_H && !defined(__MINGW32__) && !_MSC_VER
#include <sys/wait.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if !HAVE_STRNCAT
#define strncat(X,Y,Z) strcat(X,Y)
#endif
#if !HAVE_STRNCPY
#define strncpy(X,Y,Z) strcpy(X,Y)
#endif
#if HAVE_GETPWNAM
#include <pwd.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if  _MSC_VER || defined(__MINGW32__)
#include <windows.h>
/* required for DLL compatibility */
#if HAVE_DIRECT_H
#include <direct.h>
#endif
#include <io.h>
#else
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#endif
/* CYGWIN seems to include this automatically */
#if HAVE_FENV_H && !defined(__CYGWIN__)
#include <fenv.h>
#endif
#if HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif


STATIC_PROTO (void InitTime, (int));
STATIC_PROTO (void InitWTime, (void));
STATIC_PROTO (Int p_sh, ( USES_REGS1 ));
STATIC_PROTO (Int p_shell, ( USES_REGS1 ));
STATIC_PROTO (Int p_system, ( USES_REGS1 ));
STATIC_PROTO (Int p_mv, ( USES_REGS1 ));
STATIC_PROTO (Int p_dir_sp, ( USES_REGS1 ));
STATIC_PROTO (void InitRandom, (void));
STATIC_PROTO (Int p_srandom, ( USES_REGS1 ));
STATIC_PROTO (Int p_alarm, ( USES_REGS1 ));
STATIC_PROTO (Int p_getenv, ( USES_REGS1 ));
STATIC_PROTO (Int p_putenv, ( USES_REGS1 ));
STATIC_PROTO (void  set_fpu_exceptions, (int));
#ifdef MACYAP
STATIC_PROTO (int chdir, (char *));
/* #define signal	skel_signal */
#endif /* MACYAP */


STD_PROTO (void exit, (int));

#ifdef _WIN32
void
Yap_WinError(char *yap_error)
{
  char msg[256];
  /* Error, we could not read time */
     FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL, GetLastError(), 
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, 256,
		  NULL);
    Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "%s at %s", msg, yap_error);
}
#endif /* _WIN32 */


#define is_valid_env_char(C) ( ((C) >= 'a' && (C) <= 'z') || ((C) >= 'A' && \
			       (C) <= 'Z') || (C) == '_' )


static int
is_directory(char *FileName)
{
#ifdef _WIN32
  DWORD dwAtts = GetFileAttributes(FileName);
  if (dwAtts == INVALID_FILE_ATTRIBUTES)
    return FALSE;
  return (dwAtts & FILE_ATTRIBUTE_DIRECTORY);
#elif HAVE_LSTAT 
  struct stat buf;

  if (lstat(FileName, &buf) == -1) {
    /* return an error number */
    return FALSE;
  }
  return S_ISDIR(buf.st_mode);
#else
  return FALSE;
#endif
}

static int
dir_separator (int ch)
{
#ifdef MAC
  return (ch == ':');
#elif ATARI || _MSC_VER
  return (ch == '\\');
#elif defined(__MINGW32__) || defined(__CYGWIN__)
  return (ch == '\\' || ch == '/');
#else
  return (ch == '/');
#endif
}

int
Yap_dir_separator (int ch)
{
  return dir_separator (ch);
}

#if _MSC_VER || defined(__MINGW32__)
#include <psapi.h>

char *libdir = NULL;
#endif

void
Yap_InitSysPath(void) {
  CACHE_REGS
  int len;
#if _MSC_VER || defined(__MINGW32__)
  int dir_done = FALSE;
  int commons_done = FALSE;
  {
    char *dir;
    if ((dir = Yap_RegistryGetString("library"))) {
      Yap_PutValue(AtomSystemLibraryDir,
		   MkAtomTerm(Yap_LookupAtom(dir)));
      dir_done = TRUE;
    }
    if ((dir = Yap_RegistryGetString("prolog_commons"))) {
      Yap_PutValue(AtomPrologCommonsDir,
		   MkAtomTerm(Yap_LookupAtom(dir)));
      commons_done = TRUE;
    }
  }
  if (dir_done && commons_done)
    return;
#endif
  strncpy(LOCAL_FileNameBuf, YAP_SHAREDIR, YAP_FILENAME_MAX);
#if _MSC_VER || defined(__MINGW32__)
  {
    DWORD fatts;
    int buflen;
    char *pt;

    if ((fatts = GetFileAttributes(LOCAL_FileNameBuf)) == 0xFFFFFFFFL ||
	!(fatts & FILE_ATTRIBUTE_DIRECTORY)) {
      /* couldn't find it where it was supposed to be,
	 let's try using the executable */
      if (!GetModuleFileNameEx( GetCurrentProcess(), NULL, LOCAL_FileNameBuf, YAP_FILENAME_MAX)) {
	Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "could not find executable name"); 
	/* do nothing */
	return;
      }
      buflen = strlen(LOCAL_FileNameBuf);
      pt = LOCAL_FileNameBuf+buflen;
      while (*--pt != '\\') {
	/* skip executable */
	if (pt == LOCAL_FileNameBuf) {
	  Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "could not find executable name");
	  /* do nothing */
	  return;
	}
      }
      while (*--pt != '\\') {
	/* skip parent directory "bin\\" */
	if (pt == LOCAL_FileNameBuf) {
	  Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "could not find executable name");
	  /* do nothing */
	}
      }
      /* now, this is a possible location for the ROOT_DIR, let's look for a share directory here */
      pt[1] = '\0';
      /* grosse */
      strncat(LOCAL_FileNameBuf,"lib\\Yap",YAP_FILENAME_MAX);
      libdir = Yap_AllocCodeSpace(strlen(LOCAL_FileNameBuf)+1);
      strncpy(libdir, LOCAL_FileNameBuf, strlen(LOCAL_FileNameBuf)+1);
      pt[1] = '\0';
      strncat(LOCAL_FileNameBuf,"share",YAP_FILENAME_MAX);
    }
  }
  strncat(LOCAL_FileNameBuf,"\\", YAP_FILENAME_MAX);
#else
  strncat(LOCAL_FileNameBuf,"/", YAP_FILENAME_MAX);
#endif
  len = strlen(LOCAL_FileNameBuf);
  strncat(LOCAL_FileNameBuf, "Yap", YAP_FILENAME_MAX);
#if _MSC_VER || defined(__MINGW32__)
  if (!dir_done) 
#endif
    {
      Yap_PutValue(AtomSystemLibraryDir,
		   MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)));
    }
#if _MSC_VER || defined(__MINGW32__)
  if (!commons_done) 
#endif
    {
      LOCAL_FileNameBuf[len] = '\0';
      strncat(LOCAL_FileNameBuf, "PrologCommons", YAP_FILENAME_MAX);
      Yap_PutValue(AtomPrologCommonsDir,
		   MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)));
    }
}

static Int
p_dir_sp ( USES_REGS1 )
{
#ifdef MAC
  Term t = MkIntTerm(':');
  Term t2 = MkIntTerm('/');
#elif ATARI || _MSC_VER || defined(__MINGW32__)
  Term t = MkIntTerm('\\');
  Term t2 = MkIntTerm('/');
#else
  Term t = MkIntTerm('/');
  Term t2 = MkIntTerm('/');
#endif

  return Yap_unify_constant(ARG1,t) || Yap_unify_constant(ARG1,t2) ;
}


void
Yap_InitPageSize(void)
{
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  Yap_page_size = si.dwPageSize;
#elif HAVE_UNISTD_H
#if defined(__FreeBSD__) || defined(__DragonFly__)
  Yap_page_size = getpagesize();
#elif defined(_AIX)
  Yap_page_size = sysconf(_SC_PAGE_SIZE);
#elif !defined(_SC_PAGESIZE)
  Yap_page_size = getpagesize();
#else
  Yap_page_size = sysconf(_SC_PAGESIZE);
#endif
#else
bla bla
#endif
}

#ifdef SIMICS
#ifdef HAVE_GETRUSAGE
#undef HAVE_GETRUSAGE
#endif
#ifdef HAVE_TIMES
#undef HAVE_TIMES
#endif
#endif /* SIMICS */

#ifdef _WIN32
#if HAVE_GETRUSAGE
#undef HAVE_GETRUSAGE
#endif
#endif

#if HAVE_GETRUSAGE

#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#if THREADS
#define StartOfTimes (*(LOCAL_ThreadHandle.start_of_timesp))
#define last_time    (*(LOCAL_ThreadHandle.last_timep))

#define StartOfTimes_sys (*(LOCAL_ThreadHandle.start_of_times_sysp))
#define last_time_sys    (*(LOCAL_ThreadHandle.last_time_sysp))

#else
/* since the point YAP was started */
static struct timeval StartOfTimes;

/* since last call to runtime */
static struct timeval last_time;

/* same for system time */
static struct timeval last_time_sys;
static struct timeval StartOfTimes_sys;
#endif

/* store user time in this variable */
static void
InitTime (int wid)
{
  struct rusage   rusage;

#if THREADS
  REMOTE_ThreadHandle(wid).start_of_timesp = (struct timeval *)malloc(sizeof(struct timeval));
  REMOTE_ThreadHandle(wid).last_timep = (struct timeval *)malloc(sizeof(struct timeval));
  REMOTE_ThreadHandle(wid).start_of_times_sysp = (struct timeval *)malloc(sizeof(struct timeval));
  REMOTE_ThreadHandle(wid).last_time_sysp = (struct timeval *)malloc(sizeof(struct timeval));
  getrusage(RUSAGE_SELF, &rusage);
  (*REMOTE_ThreadHandle(wid).last_timep).tv_sec =
    (*REMOTE_ThreadHandle(wid).start_of_timesp).tv_sec = 
    rusage.ru_utime.tv_sec;
  (*REMOTE_ThreadHandle(wid).last_timep).tv_usec =
    (*REMOTE_ThreadHandle(wid).start_of_timesp).tv_usec = 
    rusage.ru_utime.tv_usec;
  (*REMOTE_ThreadHandle(wid).last_time_sysp).tv_sec =
    (*REMOTE_ThreadHandle(wid).start_of_times_sysp).tv_sec = 
    rusage.ru_stime.tv_sec;
  (*REMOTE_ThreadHandle(wid).last_time_sysp).tv_usec =
    (*REMOTE_ThreadHandle(wid).start_of_times_sysp).tv_usec = 
    rusage.ru_stime.tv_usec;
#else
  getrusage(RUSAGE_SELF, &rusage);
  last_time.tv_sec =
    StartOfTimes.tv_sec = 
    rusage.ru_utime.tv_sec;
  last_time.tv_usec =
    StartOfTimes.tv_usec = 
    rusage.ru_utime.tv_usec;
  last_time_sys.tv_sec =
    StartOfTimes_sys.tv_sec = 
    rusage.ru_stime.tv_sec;
  last_time_sys.tv_usec =
    StartOfTimes_sys.tv_usec = 
    rusage.ru_stime.tv_usec;  
#endif
}


UInt
Yap_cputime ( void )
{
  CACHE_REGS
 struct rusage   rusage;

 getrusage(RUSAGE_SELF, &rusage);
 return((rusage.ru_utime.tv_sec - StartOfTimes.tv_sec)) * 1000 +
   ((rusage.ru_utime.tv_usec - StartOfTimes.tv_usec) / 1000);
}

void Yap_cputime_interval(Int *now,Int *interval)
{
  CACHE_REGS
  struct rusage   rusage;

  getrusage(RUSAGE_SELF, &rusage);
  *now = (rusage.ru_utime.tv_sec - StartOfTimes.tv_sec) * 1000 +
    (rusage.ru_utime.tv_usec - StartOfTimes.tv_usec) / 1000;
  *interval = (rusage.ru_utime.tv_sec - last_time.tv_sec) * 1000 +
    (rusage.ru_utime.tv_usec - last_time.tv_usec) / 1000;
  last_time.tv_usec = rusage.ru_utime.tv_usec;
  last_time.tv_sec = rusage.ru_utime.tv_sec;
}

void Yap_systime_interval(Int *now,Int *interval)
{
  CACHE_REGS
  struct rusage   rusage;

  getrusage(RUSAGE_SELF, &rusage);
  *now = (rusage.ru_stime.tv_sec - StartOfTimes_sys.tv_sec) * 1000 +
    (rusage.ru_stime.tv_usec - StartOfTimes_sys.tv_usec) / 1000;
  *interval = (rusage.ru_stime.tv_sec - last_time_sys.tv_sec) * 1000 +
    (rusage.ru_stime.tv_usec - last_time_sys.tv_usec) / 1000;
  last_time_sys.tv_usec = rusage.ru_stime.tv_usec;
  last_time_sys.tv_sec = rusage.ru_stime.tv_sec;
}

#elif defined(_WIN32)

#ifdef __GNUC__

/* This is stolen from the Linux kernel.
   The problem is that mingw32 does not seem to have acces to div */
#ifndef do_div
#define do_div(n,base) ({ \
	unsigned long __upper, __low, __high, __mod; \
	asm("":"=a" (__low), "=d" (__high):"A" (n)); \
	__upper = __high; \
	if (__high) { \
		__upper = __high % (base); \
		__high = __high / (base); \
	} \
	asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (base), "0" (__low), "1" (__upper)); \
	asm("":"=A" (n):"a" (__low),"d" (__high)); \
	__mod; \
})
#endif

#endif



#include <time.h>

static FILETIME StartOfTimes, last_time;

static FILETIME StartOfTimes_sys, last_time_sys;

static clock_t TimesStartOfTimes, Times_last_time;

/* store user time in this variable */
static void
InitTime (int)
{
  HANDLE hProcess = GetCurrentProcess();
  FILETIME CreationTime, ExitTime, KernelTime, UserTime;
  if (!GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime)) {
    /* WIN98 */
    clock_t t;
    t = clock ();
    Times_last_time = TimesStartOfTimes = t;
  } else {
    (*REMOTE_ThreadHandle(wid).last_timep).dwLowDateTime = 
      UserTime.dwLowDateTime;
    (*REMOTE_ThreadHandle(wid).last_timep).dwHighDateTime =
      UserTime.dwHighDateTime;
    (*REMOTE_ThreadHandle(wid).start_of_timesp).dwLowDateTime =
      UserTime.dwLowDateTime;
    (*REMOTE_ThreadHandle(wid).start_of_timesp).dwHighDateTime = 
      UserTime.dwHighDateTime;
    (*REMOTE_ThreadHandle(wid).last_time_sysp).dwLowDateTime =
      KernelTime.dwLowDateTime;
    (*REMOTE_ThreadHandle(wid).last_time_sysp).dwHighDateTime = 
      KernelTime.dwHighDateTime;
    (*REMOTE_ThreadHandle(wid).start_of_times_sysp).dwLowDateTime =
      KernelTime.dwLowDateTime;
    (*REMOTE_ThreadHandle(wid).start_of_times_sysp).dwHighDateTime = 
      KernelTime.dwHighDateTime;
  }
}

#ifdef __GNUC__
static unsigned long long int 
sub_utime(FILETIME t1, FILETIME t2)
{
  ULARGE_INTEGER u[2];
  memcpy((void *)u,(void *)&t1,sizeof(FILETIME));
  memcpy((void *)(u+1),(void *)&t2,sizeof(FILETIME));
  return 
    u[0].QuadPart - u[1].QuadPart;
}
#endif

UInt
Yap_cputime ( void )
{
  HANDLE hProcess = GetCurrentProcess();
  FILETIME CreationTime, ExitTime, KernelTime, UserTime;
  if (!GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime)) {
    clock_t t;
    t = clock ();
    return(((t - TimesStartOfTimes)*1000) / CLOCKS_PER_SEC);
  } else {
#ifdef __GNUC__
    unsigned long long int t =
      sub_utime(UserTime,StartOfTimes);
    do_div(t,10000);
    return((Int)t);
#endif
#ifdef _MSC_VER
    __int64 t = *(__int64 *)&UserTime - *(__int64 *)&StartOfTimes;
    return((Int)(t/10000));
#endif
  }
}

void Yap_cputime_interval(Int *now,Int *interval)
{
  HANDLE hProcess = GetCurrentProcess();
  FILETIME CreationTime, ExitTime, KernelTime, UserTime;
  if (!GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime)) {
    clock_t t;
    t = clock ();
    *now = ((t - TimesStartOfTimes)*1000) / CLOCKS_PER_SEC;
    *interval = (t - Times_last_time) * 1000 / CLOCKS_PER_SEC;
    Times_last_time = t;
  } else {
#ifdef __GNUC__
    unsigned long long int t1 =
      sub_utime(UserTime, StartOfTimes);
    unsigned long long int t2 =
      sub_utime(UserTime, last_time);
    do_div(t1,10000);
    *now = (Int)t1;
    do_div(t2,10000);
    *interval = (Int)t2;
#endif
#ifdef _MSC_VER
    __int64 t1 = *(__int64 *)&UserTime - *(__int64 *)&StartOfTimes;
    __int64 t2 = *(__int64 *)&UserTime - *(__int64 *)&last_time;
    *now = (Int)(t1/10000);
    *interval = (Int)(t2/10000);
#endif
    last_time.dwLowDateTime = UserTime.dwLowDateTime;
    last_time.dwHighDateTime = UserTime.dwHighDateTime;
  }
}

void Yap_systime_interval(Int *now,Int *interval)
{
  HANDLE hProcess = GetCurrentProcess();
  FILETIME CreationTime, ExitTime, KernelTime, UserTime;
  if (!GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime)) {
    *now = *interval = 0; /* not available */
  } else {
#ifdef __GNUC__
    unsigned long long int t1 =
      sub_utime(KernelTime, StartOfTimes_sys);
    unsigned long long int t2 =
      sub_utime(KernelTime, last_time_sys);
    do_div(t1,10000);
    *now = (Int)t1;
    do_div(t2,10000);
    *interval = (Int)t2;
#endif
#ifdef _MSC_VER
    __int64 t1 = *(__int64 *)&KernelTime - *(__int64 *)&StartOfTimes_sys;
    __int64 t2 = *(__int64 *)&KernelTime - *(__int64 *)&last_time_sys;
    *now = (Int)(t1/10000);
    *interval = (Int)(t2/10000);
#endif
    last_time_sys.dwLowDateTime = KernelTime.dwLowDateTime;
    last_time_sys.dwHighDateTime = KernelTime.dwHighDateTime;
  }
}

#elif HAVE_TIMES

#if defined(_WIN32)

#include <time.h>

#define TicksPerSec     CLOCKS_PER_SEC

#else

#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#endif

#if defined(__sun__) && (defined(__svr4__) || defined(__SVR4))

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#define TicksPerSec	CLK_TCK
#endif

#if defined(__alpha) || defined(__FreeBSD__) || defined(__linux__) || defined(__DragonFly__)

#if HAVE_TIME_H
#include <time.h>
#endif

#define TicksPerSec	sysconf(_SC_CLK_TCK)

#endif

#if !TMS_IN_SYS_TIME
#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#endif

static clock_t StartOfTimes, last_time;

static clock_t StartOfTimes_sys, last_time_sys;

/* store user time in this variable */
static void
InitTime (void)
{
  struct tms t;
  times (&t);
  (*REMOTE_ThreadHandle(wid).last_timep) = StartOfTimes = t.tms_utime;
  last_time_sys = StartOfTimes_sys = t.tms_stime;
}

UInt
Yap_cputime (void)
{
  struct tms t;
  times(&t);
  return((t.tms_utime - StartOfTimes)*1000 / TicksPerSec);
}

void Yap_cputime_interval(Int *now,Int *interval)
{
  struct tms t;
  times (&t);
  *now = ((t.tms_utime - StartOfTimes)*1000) / TicksPerSec;
  *interval = (t.tms_utime - last_time) * 1000 / TicksPerSec;
  last_time = t.tms_utime;
}

void Yap_systime_interval(Int *now,Int *interval)
{
  struct tms t;
  times (&t);
  *now = ((t.tms_stime - StartOfTimes_sys)*1000) / TicksPerSec;
  *interval = (t.tms_stime - last_time_sys) * 1000 / TicksPerSec;
  last_time_sys = t.tms_stime;
}

#else /* HAVE_TIMES */

#ifdef SIMICS

#include <sys/time.h>

/* since the point YAP was started */
static struct timeval StartOfTimes;

/* since last call to runtime */
static struct timeval last_time;

/* store user time in this variable */
static void
InitTime (int wid)
{
  struct timeval   tp;

  gettimeofday(&tp,NULL);
  (*REMOTE_ThreadHandle(wid).last_timep).tv_sec = (*REMOTE_ThreadHandle.start_of_timesp(wid)).tv_sec = tp.tv_sec;
  (*REMOTE_ThreadHandle(wid).last_timep).tv_usec = (*REMOTE_ThreadHandle.start_of_timesp(wid)).tv_usec = tp.tv_usec;
}


UInt
Yap_cputime (void)
{
  struct timeval   tp;

  gettimeofday(&tp,NULL);
  if (StartOfTimes.tv_usec > tp.tv_usec)
    return((tp.tv_sec - StartOfTimes.tv_sec - 1) * 1000 +
	   (StartOfTimes.tv_usec - tp.tv_usec) /1000);
  else
    return((tp.tv_sec - StartOfTimes.tv_sec)) * 1000 +
      ((tp.tv_usec - StartOfTimes.tv_usec) / 1000);
}

void Yap_cputime_interval(Int *now,Int *interval)
{
  struct timeval   tp;

  gettimeofday(&tp,NULL);
  *now = (tp.tv_sec - StartOfTimes.tv_sec) * 1000 +
    (tp.tv_usec - StartOfTimes.tv_usec) / 1000;
  *interval = (tp.tv_sec - last_time.tv_sec) * 1000 +
    (tp.tv_usec - last_time.tv_usec) / 1000;
  last_time.tv_usec = tp.tv_usec;
  last_time.tv_sec = tp.tv_sec;
}

void Yap_systime_interval(Int *now,Int *interval)
{
  *now =  *interval = 0; /* not available */
}

#endif /* SIMICS */

#ifdef COMMENTED_OUT
/* This code is not working properly. I left it here to help future ports */
#ifdef MPW

#include <files.h>
#include <Events.h>

#define TicksPerSec 60.0

static double
real_cputime ()
{
  return (((double) TickCount ()) / TicksPerSec);
}

#endif /* MPW */

#ifdef LATTICE

#include "osbind.h"

static long *ptime;

gettime ()
{
  *ptime = *(long *) 0x462;
}

static double
real_cputime ()
{
  long thetime;
  ptime = &thetime;
  xbios (38, gettime);
  return (((double) thetime) / (Getrez () == 2 ? 70 : 60));
}

#endif /* LATTICE */

#ifdef M_WILLIAMS

#include <osbind.h>
#include <xbios.h>

static long *ptime;

static long
readtime ()
{
  return (*((long *) 0x4ba));
}

static double
real_cputime ()
{
  long time;

  time = Supexec (readtime);
  return (time / 200.0);
}

#endif /* M_WILLIAMS */

#ifdef LIGHT

#undef FALSE
#undef TRUE

#include <FileMgr.h>

#define TicksPerSec 60.0

static double
real_cputime ()
{
  return (((double) TickCount ()) / TicksPerSec);
}

#endif /* LIGHT */

#endif /* COMMENTED_OUT */

#endif /* HAVE_GETRUSAGE */

#if HAVE_GETHRTIME

#if HAVE_TIME_H
#include <time.h>
#endif

/* since the point YAP was started */
static hrtime_t StartOfWTimes;

/* since last call to walltime */
#define  LastWtime (*(hrtime_t *)ALIGN_YAPTYPE(LastWtimePtr,hrtime_t))

static void
InitWTime (void)
{
  StartOfWTimes = gethrtime();
}

static void
InitLastWtime(void) {
  /* ask for twice the space in order to guarantee alignment */
  LastWtimePtr = (void *)Yap_AllocCodeSpace(2*sizeof(hrtime_t));
  LastWtime = StartOfWTimes;
}

Int
Yap_walltime (void)
{
  hrtime_t tp = gethrtime();
  /* return time in milliseconds */
  return((Int)((tp-StartOfWTimes)/((hrtime_t)1000000)));

}

void Yap_walltime_interval(Int *now,Int *interval)
{
  hrtime_t tp = gethrtime();
  /* return time in milliseconds */
  *now = (Int)((tp-StartOfWTimes)/((hrtime_t)1000000));
  *interval = (Int)((tp-LastWtime)/((hrtime_t)1000000));
  LastWtime = tp;
}


#elif HAVE_GETTIMEOFDAY

/* since the point YAP was started */
static struct timeval StartOfWTimes;

/* since last call to walltime */
#define LastWtime (*(struct timeval *)LastWtimePtr)

/* store user time in this variable */
static void
InitWTime (void)
{
  gettimeofday(&StartOfWTimes,NULL);
}

static void
InitLastWtime(void) {
  LastWtimePtr = (void *)Yap_AllocCodeSpace(sizeof(struct timeval));
  LastWtime.tv_usec = StartOfWTimes.tv_usec;
  LastWtime.tv_sec = StartOfWTimes.tv_sec;
}


Int
Yap_walltime (void)
{
  struct timeval   tp;

  gettimeofday(&tp,NULL);
  if (StartOfWTimes.tv_usec > tp.tv_usec)
    return((tp.tv_sec - StartOfWTimes.tv_sec - 1) * 1000 +
	   (StartOfWTimes.tv_usec - tp.tv_usec) /1000);
  else
    return((tp.tv_sec - StartOfWTimes.tv_sec)) * 1000 +
      ((tp.tv_usec - LastWtime.tv_usec) / 1000);
}

void Yap_walltime_interval(Int *now,Int *interval)
{
  struct timeval   tp;

  gettimeofday(&tp,NULL);
  *now = (tp.tv_sec - StartOfWTimes.tv_sec) * 1000 +
    (tp.tv_usec - StartOfWTimes.tv_usec) / 1000;
  *interval = (tp.tv_sec - LastWtime.tv_sec) * 1000 +
    (tp.tv_usec - LastWtime.tv_usec) / 1000;
  LastWtime.tv_usec = tp.tv_usec;
  LastWtime.tv_sec = tp.tv_sec;
}

#elif defined(_WIN32)

#include <sys/timeb.h>
#include <time.h>

/* since the point YAP was started */
static struct _timeb StartOfWTimes;

/* since last call to walltime */
#define LastWtime (*(struct timeb *)LastWtimePtr)

/* store user time in this variable */
static void
InitWTime (void)
{
  _ftime(&StartOfWTimes);
}

static void
InitLastWtime(void) {
  LastWtimePtr = (void *)Yap_AllocCodeSpace(sizeof(struct timeb));
  LastWtime.time = StartOfWTimes.time;
  LastWtime.millitm = StartOfWTimes.millitm;
}


Int
Yap_walltime (void)
{
  struct _timeb   tp;

  _ftime(&tp);
  if (StartOfWTimes.millitm > tp.millitm)
    return((tp.time - StartOfWTimes.time - 1) * 1000 +
	   (StartOfWTimes.millitm - tp.millitm));
  else
    return((tp.time - StartOfWTimes.time)) * 1000 +
      ((tp.millitm - LastWtime.millitm) / 1000);
}

void Yap_walltime_interval(Int *now,Int *interval)
{
  struct _timeb   tp;

  _ftime(&tp);
  *now = (tp.time - StartOfWTimes.time) * 1000 +
    (tp.millitm - StartOfWTimes.millitm);
  *interval = (tp.time - LastWtime.time) * 1000 +
    (tp.millitm - LastWtime.millitm) ;
  LastWtime.millitm = tp.millitm;
  LastWtime.time = tp.time;
}

#elif HAVE_TIMES

static clock_t StartOfWTimes;

#define LastWtime (*(clock_t *)LastWtimePtr)

/* store user time in this variable */
static void
InitWTime (void)
{
  StartOfWTimes = times(NULL);
}

static void
InitLastWtime(void) {
  LastWtimePtr = (void *)Yap_AllocCodeSpace(sizeof(clock_t));
  LastWtime = StartOfWTimes;
}

Int
Yap_walltime (void)
{
  clock_t t;
  t = times(NULL);
  return ((t - StartOfWTimes)*1000 / TicksPerSec));
}

void Yap_walltime_interval(Int *now,Int *interval)
{
  clock_t t;
  t = times(NULL);
  *now = ((t - StartOfWTimes)*1000) / TicksPerSec;
  *interval = (t - LastWtime) * 1000 / TicksPerSec;
}

#endif /* HAVE_TIMES */

#if HAVE_TIME_H
#include <time.h>
#endif

unsigned int current_seed;

static void
InitRandom (void)
{
  current_seed = (unsigned int) time (NULL);
#if HAVE_RANDOM
  srandom (current_seed);
#elif HAVE_RAND
  srand (current_seed);
#endif
}

STD_PROTO (extern int rand, (void));


double
Yap_random (void)
{
#if HAVE_RANDOM
/*  extern long random (); */
  return (((double) random ()) / 0x7fffffffL /* 2**31-1 */);
#elif HAVE_RAND
  return (((double) (rand ()) / RAND_MAX));
#else
  Yap_Error(SYSTEM_ERROR, TermNil,
	"random not available in this configuration");
  return (0.0);
#endif
}

static Int
p_srandom ( USES_REGS1 )
{
  register Term t0 = Deref (ARG1);
  if (IsVarTerm (t0)) {
    return(Yap_unify(ARG1,MkIntegerTerm((Int)current_seed)));
  }
  if(!IsNumTerm (t0))
    return (FALSE);
  if (IsIntTerm (t0))
    current_seed = (unsigned int) IntOfTerm (t0);
  else if (IsFloatTerm (t0))
    current_seed  = (unsigned int) FloatOfTerm (t0);
  else
    current_seed  = (unsigned int) LongIntOfTerm (t0);
#if HAVE_RANDOM
  srandom(current_seed);
#elif HAVE_RAND
  srand(current_seed);

#endif
  return (TRUE);
}

#if HAVE_SIGNAL

#include <signal.h>

#ifdef MPW
#define signal	sigset
#endif


#ifdef MSH

#define SIGFPE	SIGDIV

#endif

STATIC_PROTO (void InitSignals, (void));

#define PLSIG_PREPARED 0x01		/* signal is prepared */
#define PLSIG_THROW    0x02		/* throw signal(num, name) */
#define PLSIG_SYNC     0x04		/* call synchronously */
#define PLSIG_NOFRAME  0x08		/* Do not create a Prolog frame */

#define SIG_PROLOG_OFFSET	32	/* Start of Prolog signals */

#define SIG_EXCEPTION	  (SIG_PROLOG_OFFSET+0)
#ifdef O_ATOMGC
#define SIG_ATOM_GC	  (SIG_PROLOG_OFFSET+1)
#endif
#define SIG_GC		  (SIG_PROLOG_OFFSET+2)
#ifdef O_PLMT
#define SIG_THREAD_SIGNAL (SIG_PROLOG_OFFSET+3)
#endif
#define SIG_FREECLAUSES	  (SIG_PROLOG_OFFSET+4)
#define SIG_PLABORT	  (SIG_PROLOG_OFFSET+5)

static struct signame
{ int 	      sig;
  const char *name;
  int	      flags;
} signames[] =
{
#ifdef SIGHUP
  { SIGHUP,	"hup",    0},
#endif
  { SIGINT,	"int",    0},
#ifdef SIGQUIT
  { SIGQUIT,	"quit",   0},
#endif
  { SIGILL,	"ill",    0},
  { SIGABRT,	"abrt",   0},
  { SIGFPE,	"fpe",    PLSIG_THROW},
#ifdef SIGKILL
  { SIGKILL,	"kill",   0},
#endif
  { SIGSEGV,	"segv",   0},
#ifdef SIGPIPE
  { SIGPIPE,	"pipe",   0},
#endif
#ifdef SIGALRM
  { SIGALRM,	"alrm",   PLSIG_THROW},
#endif
  { SIGTERM,	"term",   0},
#ifdef SIGUSR1
  { SIGUSR1,	"usr1",   0},
#endif
#ifdef SIGUSR2
  { SIGUSR2,	"usr2",   0},
#endif
#ifdef SIGCHLD
  { SIGCHLD,	"chld",   0},
#endif
#ifdef SIGCONT
  { SIGCONT,	"cont",   0},
#endif
#ifdef SIGSTOP
  { SIGSTOP,	"stop",   0},
#endif
#ifdef SIGTSTP
  { SIGTSTP,	"tstp",   0},
#endif
#ifdef SIGTTIN
  { SIGTTIN,	"ttin",   0},
#endif
#ifdef SIGTTOU
  { SIGTTOU,	"ttou",   0},
#endif
#ifdef SIGTRAP
  { SIGTRAP,	"trap",   0},
#endif
#ifdef SIGBUS
  { SIGBUS,	"bus",    0},
#endif
#ifdef SIGSTKFLT
  { SIGSTKFLT,	"stkflt", 0},
#endif
#ifdef SIGURG
  { SIGURG,	"urg",    0},
#endif
#ifdef SIGIO
  { SIGIO,	"io",     0},
#endif
#ifdef SIGPOLL
  { SIGPOLL,	"poll",   0},
#endif
#ifdef SIGXCPU
  { SIGXCPU,	"xcpu",   PLSIG_THROW},
#endif
#ifdef SIGXFSZ
  { SIGXFSZ,	"xfsz",   PLSIG_THROW},
#endif
#ifdef SIGVTALRM
  { SIGVTALRM,	"vtalrm", PLSIG_THROW},
#endif
#ifdef SIGPROF
  { SIGPROF,	"prof",   0},
#endif
#ifdef SIGPWR
  { SIGPWR,	"pwr",    0},
#endif
  { SIG_EXCEPTION,     "prolog:exception",     0 },
#ifdef SIG_ATOM_GC
  { SIG_ATOM_GC,   "prolog:atom_gc",       0 },
#endif
  { SIG_GC,	       "prolog:gc",	       0 },
#ifdef SIG_THREAD_SIGNAL
  { SIG_THREAD_SIGNAL, "prolog:thread_signal", 0 },
#endif

  { -1,		NULL,     0}
};

/* SWI emulation */
int
Yap_signal_index(const char *name)
{ struct signame *sn = signames;
  char tmp[12];

  if ( strncmp(name, "SIG", 3) == 0 && strlen(name) < 12 ) 
    { char *p = (char *)name+3, *q = tmp;
      while ((*q++ = tolower(*p++))) {};
      name = tmp;
    }

  for( ; sn->name; sn++ )
  { if ( !strcmp(sn->name, name) )
      return sn->sig;
  }

  return -1;
}

#if (defined(__svr4__) || defined(__SVR4))

#if HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#if HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
#endif

STATIC_PROTO (void HandleSIGSEGV, (int, siginfo_t   *, ucontext_t *));
STATIC_PROTO (void HandleMatherr,  (int, siginfo_t   *, ucontext_t *));
STATIC_PROTO (void my_signal_info, (int, void (*)(int, siginfo_t  *, ucontext_t *)));
STATIC_PROTO (void my_signal, (int, void (*)(int, siginfo_t  *, ucontext_t *)));

/* This routine believes there is a continuous space starting from the
   HeapBase and ending on TrailTop */
static void
HandleSIGSEGV(int   sig,   siginfo_t   *sip, ucontext_t *uap)
{

#if !USE_SYSTEM_MALLOC
  if (
      sip->si_code != SI_NOINFO &&
      sip->si_code == SEGV_MAPERR &&
      (void *)(sip->si_addr) > (void *)(Yap_HeapBase) &&
      (void *)(sip->si_addr) < (void *)(LOCAL_TrailTop+K64)) {
    Yap_growtrail(K64, TRUE);
  }  else
#endif
    {
      Yap_Error(FATAL_ERROR, TermNil,
		"likely bug in YAP, segmentation violation at %p", sip->si_addr);
  }
}


static void
HandleMatherr(int  sig, siginfo_t *sip, ucontext_t *uap)
{
  CACHE_REGS
  yap_error_number error_no;

  /* reset the registers so that we don't have trash in abstract machine */

  switch(sip->si_code) {
  case FPE_INTDIV:
    error_no = EVALUATION_ERROR_ZERO_DIVISOR;
    break;
  case FPE_INTOVF:
    error_no = EVALUATION_ERROR_INT_OVERFLOW;
    break;
  case FPE_FLTDIV:
    error_no = EVALUATION_ERROR_ZERO_DIVISOR;
    break;
  case FPE_FLTOVF:
    error_no = EVALUATION_ERROR_FLOAT_OVERFLOW;
    break;
  case FPE_FLTUND:
    error_no = EVALUATION_ERROR_FLOAT_UNDERFLOW;
    break;
  case FPE_FLTRES:
  case FPE_FLTINV:
  case FPE_FLTSUB:
  default:
    error_no = EVALUATION_ERROR_UNDEFINED;
  }
  set_fpu_exceptions(0);
  Yap_Error(error_no, TermNil, "");
}


#if HAVE_SIGSEGV && !defined(THREADS) 
static void
my_signal_info(int sig, void (*handler)(int, siginfo_t  *, ucontext_t *))
{
  struct sigaction sigact;

  sigact.sa_handler = handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO;

  sigaction(sig,&sigact,NULL);
}
#endif

static void
my_signal(int sig, void (*handler)(int, siginfo_t *, ucontext_t *))
{
  struct sigaction sigact;

  sigact.sa_handler=handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(sig,&sigact,NULL);
}

#elif defined(__linux__)

STATIC_PROTO (RETSIGTYPE HandleMatherr, (int));
#if HAVE_SIGSEGV && !defined(THREADS) 
STATIC_PROTO (RETSIGTYPE HandleSIGSEGV, (int,siginfo_t *,void *));
STATIC_PROTO (void my_signal_info, (int, void (*)(int,siginfo_t *,void *)));
#endif
STATIC_PROTO (void my_signal, (int, void (*)(int)));

/******** Handling floating point errors *******************/


/* old code, used to work with matherror(), deprecated now:
  char err_msg[256];
  switch (x->type)
    {
    case DOMAIN:
    case SING:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, "%s", x->name);
      return(0);
    case OVERFLOW:
      Yap_Error(EVALUATION_ERROR_FLOAT_OVERFLOW, TermNil, "%s", x->name);
      return(0);
    case UNDERFLOW:
      Yap_Error(EVALUATION_ERROR_FLOAT_UNDERFLOW, TermNil, "%s", x->name);
      return(0);
    case PLOSS:
    case TLOSS:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, "%s(%g) = %g", x->name,
	       x->arg1, x->retval);
      return(0);
    default:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, NULL);
      return(0);
    }
  */


static RETSIGTYPE
HandleMatherr(int sig)
{
  CACHE_REGS
#if HAVE_FETESTEXCEPT

  /* This should work in Linux, but it doesn't seem to. */
  
  int raised = fetestexcept(FE_ALL_EXCEPT);

  if (raised & FE_OVERFLOW) {
    LOCAL_matherror  = EVALUATION_ERROR_FLOAT_OVERFLOW;
  } else if (raised & (FE_INVALID|FE_INEXACT)) {
    LOCAL_matherror  = EVALUATION_ERROR_UNDEFINED;
  } else if (raised & FE_DIVBYZERO) {
    LOCAL_matherror  = EVALUATION_ERROR_ZERO_DIVISOR;
  } else if (raised & FE_UNDERFLOW) {
    LOCAL_matherror  = EVALUATION_ERROR_FLOAT_UNDERFLOW;
  } else
#endif
    LOCAL_matherror  = EVALUATION_ERROR_UNDEFINED;
  /* something very bad happened on the way to the forum */
  set_fpu_exceptions(FALSE);
  Yap_Error(LOCAL_matherror , TermNil, "");
}

#if HAVE_SIGSEGV && !defined(THREADS) 
static void
my_signal_info(int sig, void (*handler)(int,siginfo_t *,void *))
{
  struct sigaction sigact;

  sigact.sa_sigaction = handler;
  sigemptyset(&sigact.sa_mask);
#if HAVE_SIGINFO
  sigact.sa_flags = SA_SIGINFO;
#else
  sigact.sa_flags = 0;
#endif

  sigaction(sig,&sigact,NULL);
}

static void
SearchForTrailFault(siginfo_t *siginfo)
{
  void *ptr = siginfo->si_addr;

  /* If the TRAIL is very close to the top of mmaped allocked space,
     then we can try increasing the TR space and restarting the
     instruction. In the worst case, the system will
     crash again
     */
#if  OS_HANDLES_TR_OVERFLOW && !USE_SYSTEM_MALLOC
  if ((ptr > (void *)LOCAL_TrailTop-1024  && 
       TR < (tr_fr_ptr) LOCAL_TrailTop+(64*1024))) {
    if (!Yap_growtrail(64*1024, TRUE)) {
      Yap_Error(OUT_OF_TRAIL_ERROR, TermNil, "YAP failed to reserve %ld bytes in growtrail", K64);
    }
    /* just in case, make sure the OS keeps the signal handler. */
    /*    my_signal_info(SIGSEGV, HandleSIGSEGV); */
  } else
#endif /* OS_HANDLES_TR_OVERFLOW */
    {
      Yap_Error(FATAL_ERROR, TermNil,
		"tried to access illegal address %p!!!!", ptr);
  }
}

static RETSIGTYPE
HandleSIGSEGV(int   sig, siginfo_t *siginfo, void *context)
{
  if (LOCAL_PrologMode & ExtendStackMode) {
    Yap_Error(FATAL_ERROR, TermNil, "OS memory allocation crashed at address %p, bailing out\n",LOCAL_TrailTop);
  }
  SearchForTrailFault(siginfo);
}
#endif

static void
my_signal(int sig, void (*handler)(int))
{
  struct sigaction sigact;

  sigact.sa_handler=handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;

  sigaction(sig,&sigact,NULL);
}

#else /* if not (defined(__svr4__) || defined(__SVR4)) */

STATIC_PROTO (RETSIGTYPE HandleMatherr, (int));
STATIC_PROTO (RETSIGTYPE HandleSIGSEGV, (int));
STATIC_PROTO (void my_signal_info, (int, void (*)(int)));
STATIC_PROTO (void my_signal, (int, void (*)(int)));

/******** Handling floating point errors *******************/


/* old code, used to work with matherror(), deprecated now:
  char err_msg[256];
  switch (x->type)
    {
    case DOMAIN:
    case SING:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, "%s", x->name);
      return(0);
    case OVERFLOW:
      Yap_Error(EVALUATION_ERROR_FLOAT_OVERFLOW, TermNil, "%s", x->name);
      return(0);
    case UNDERFLOW:
      Yap_Error(EVALUATION_ERROR_FLOAT_UNDERFLOW, TermNil, "%s", x->name);
      return(0);
    case PLOSS:
    case TLOSS:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, "%s(%g) = %g", x->name,
	       x->arg1, x->retval);
      return(0);
    default:
      Yap_Error(EVALUATION_ERROR_UNDEFINED, TermNil, NULL);
      return(0);
    }
  */


#if HAVE_FENV_H
#include <fenv.h>
#endif

static RETSIGTYPE
HandleMatherr(int sig)
{
#if HAVE_FETESTEXCEPT
  CACHE_REGS
  /* This should work in Linux, but it doesn't seem to. */
  
  int raised = fetestexcept(FE_ALL_EXCEPT);

  if (raised & FE_OVERFLOW) {
    LOCAL_matherror  = EVALUATION_ERROR_FLOAT_OVERFLOW;
  } else if (raised & (FE_INVALID|FE_INEXACT)) {
    LOCAL_matherror  = EVALUATION_ERROR_UNDEFINED;
  } else if (raised & FE_DIVBYZERO) {
    LOCAL_matherror  = EVALUATION_ERROR_ZERO_DIVISOR;
  } else if (raised & FE_UNDERFLOW) {
    LOCAL_matherror  = EVALUATION_ERROR_FLOAT_UNDERFLOW;
  } else
#endif
    LOCAL_matherror  = EVALUATION_ERROR_UNDEFINED;
  /* something very bad happened on the way to the forum */
  set_fpu_exceptions(FALSE);
  Yap_Error(LOCAL_matherror , TermNil, "");
}

static void
SearchForTrailFault(void)
{
  /* If the TRAIL is very close to the top of mmaped allocked space,
     then we can try increasing the TR space and restarting the
     instruction. In the worst case, the system will
     crash again
     */
#ifdef DEBUG
  /*  fprintf(stderr,"Catching a sigsegv at %p with %p\n", TR, TrailTop); */
#endif
#if  OS_HANDLES_TR_OVERFLOW && !USE_SYSTEM_MALLOC
  if ((TR > (tr_fr_ptr)LOCAL_TrailTop-1024  && 
       TR < (tr_fr_ptr)LOCAL_TrailTop+(64*1024))|| Yap_DBTrailOverflow()) {
    long trsize = K64;

    while ((CELL)TR > (CELL)LOCAL_TrailTop+trsize) {
      trsize += K64;
    }
    if (!Yap_growtrail(trsize, TRUE)) {
      Yap_Error(OUT_OF_TRAIL_ERROR, TermNil, "YAP failed to reserve %ld bytes in growtrail", K64);
    }
    /* just in case, make sure the OS keeps the signal handler. */
    /*    my_signal_info(SIGSEGV, HandleSIGSEGV); */
  } else
#endif /* OS_HANDLES_TR_OVERFLOW */
    Yap_Error(INTERNAL_ERROR, TermNil,
	  "likely bug in YAP, segmentation violation");
}

static RETSIGTYPE
HandleSIGSEGV(int   sig)
{
  CACHE_REGS
  if (LOCAL_PrologMode & ExtendStackMode) {
    Yap_Error(FATAL_ERROR, TermNil, "OS memory allocation crashed at address %p, bailing out\n",LOCAL_TrailTop);
  }
  SearchForTrailFault();
}

#if HAVE_SIGACTION

static void
my_signal_info(int sig, void (*handler)(int))
{
  struct sigaction sigact;

  sigact.sa_handler = handler;
  sigemptyset(&sigact.sa_mask);
#if HAVE_SIGINFO
  sigact.sa_flags = SA_SIGINFO;
#else
  sigact.sa_flags = 0;
#endif

  sigaction(sig,&sigact,NULL);
}

static void
my_signal(int sig, void (*handler)(int))
{
  struct sigaction sigact;

  sigact.sa_handler=handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;

  sigaction(sig,&sigact,NULL);
}

#else

static void
my_signal(int sig, void (*handler)(int))
{
  signal(sig, handler);
}

static void
my_signal_info(sig, handler)
int sig;
void (*handler)(int);
{
  if(signal(sig, handler) == SIG_ERR)
    exit(1);
}
#endif /* __linux__ */

#endif /* (defined(__svr4__) || defined(__SVR4)) */


static int
InteractSIGINT(int ch) {
  CACHE_REGS
  switch (ch) {
  case 'a':
    /* abort computation */
    if (LOCAL_PrologMode & (GCMode|ConsoleGetcMode|CritMode)) {
      LOCAL_PrologMode |= AbortMode;
      return -1;
    } else {
      Yap_Error(PURE_ABORT, TermNil, "abort from console");
    }
    LOCAL_PrologMode &= ~AsyncIntMode;
    Yap_RestartYap( 1 );
    return -1;
  case 'b':
    /* continue */
    Yap_signal (YAP_BREAK_SIGNAL);
    return 1;
  case 'c':
    /* continue */
    return 1;
  case 'd':
    Yap_signal (YAP_DEBUG_SIGNAL);
    /* enter debug mode */
    return 1;
  case 'e':
    /* exit */
    Yap_exit(0);
    return -1;
  case 'g':
    /* exit */
    Yap_signal (YAP_STACK_DUMP_SIGNAL);
    return -1;
  case 't':
    /* start tracing */
    Yap_signal (YAP_TRACE_SIGNAL);
    return 1;
#ifdef LOW_LEVEL_TRACER
  case 'T':
    toggle_low_level_trace();
    return 1;
#endif
  case 's':
    /* show some statistics */
    Yap_signal (YAP_STATISTICS_SIGNAL);
    return 1;
  case EOF:
    return(0);
    break;
  case 'h':
  case '?':
  default:
    /* show an helpful message */
    fprintf(GLOBAL_stderr, "Please press one of:\n");
    fprintf(GLOBAL_stderr, "  a for abort\n  c for continue\n  d for debug\n");
    fprintf(GLOBAL_stderr, "  e for exit\n  g for stack dump\n  s for statistics\n  t for trace\n");
    fprintf(GLOBAL_stderr, "  b for break\n");
    return(0);
  }
}

/*
  This function talks to the user about a signal. We assume we are in
  the context of the main Prolog thread (trivial in Unix, but hard in WIN32)
*/ 
static int
ProcessSIGINT(void)
{
  CACHE_REGS
  int ch, out;

  LOCAL_PrologMode |= AsyncIntMode;
  do {
    ch = Yap_GetCharForSIGINT();
  } while (!(out = InteractSIGINT(ch)));
  LOCAL_PrologMode &= ~AsyncIntMode;
  LOCAL_PrologMode &= ~InterruptMode;
  return(out);
}

#if !_MSC_VER && !defined(__MINGW32__)

#if HAVE_SIGNAL
static int             snoozing = FALSE;
#endif

/* This function is called from the signal handler to process signals.
   We assume we are within the context of the signal handler, whatever
   that might be
*/
static RETSIGTYPE
#if (defined(__svr4__) || defined(__SVR4))
HandleSIGINT (int sig, siginfo_t   *x, ucontext_t *y)
#else
HandleSIGINT (int sig)
#endif
{
  CACHE_REGS
    /* fprintf(stderr,"mode = %x\n",LOCAL_PrologMode); */
  my_signal(SIGINT, HandleSIGINT);
  /* do this before we act */
#if HAVE_ISATTY
  if (!isatty(0)) {
    Yap_Error(INTERRUPT_ERROR,MkIntTerm(SIGINT),NULL);
    return;
  }
#endif
  if (LOCAL_InterruptsDisabled) {
    return;
  }
  if (LOCAL_PrologMode & ConsoleGetcMode) {
    LOCAL_PrologMode |= InterruptMode;
    return;
  }
#ifdef HAVE_SETBUF
  /* make sure we are not waiting for the end of line */
  YP_setbuf (stdin, NULL);
#endif
  if (snoozing) {
    snoozing = FALSE;
    return;
  }
  ProcessSIGINT();
}
#endif

#if !defined(_WIN32)
/* this routine is called if the system activated the alarm */
static RETSIGTYPE
#if (defined(__svr4__) || defined(__SVR4))
HandleALRM (int s, siginfo_t   *x, ucontext_t *y)
#else
HandleALRM(int s)
#endif
{
  my_signal (SIGALRM, HandleALRM);
  /* force the system to creep */
  Yap_signal (YAP_ALARM_SIGNAL);
  /* now, say what is going on */
  Yap_PutValue(AtomAlarm, MkAtomTerm(AtomTrue));
}
#endif


#if !defined(_WIN32)
/* this routine is called if the system activated the alarm */
static RETSIGTYPE
#if (defined(__svr4__) || defined(__SVR4))
HandleVTALRM (int s, siginfo_t   *x, ucontext_t *y)
#else
HandleVTALRM(int s)
#endif
{
  my_signal (SIGVTALRM, HandleVTALRM);
  /* force the system to creep */
  Yap_signal (YAP_VTALARM_SIGNAL);
  /* now, say what is going on */
  Yap_PutValue(AtomAlarm, MkAtomTerm(AtomTrue));
}
#endif


/*
 * This function is called after a normal interrupt had been caught.
 * It allows 6 possibilities: abort, continue, trace, debug, help, exit.
 */

#if !defined(LIGHT) && !_MSC_VER && !defined(__MINGW32__) && !defined(LIGHT) 
static RETSIGTYPE
#if (defined(__svr4__) || defined(__SVR4))
ReceiveSignal (int s, siginfo_t   *x, ucontext_t *y)
#else
ReceiveSignal (int s)
#endif
{
  switch (s)
    {
#ifndef MPW
    case SIGFPE:
      set_fpu_exceptions(FALSE);
      Yap_Error (SYSTEM_ERROR, TermNil, "floating point exception ]");
      break;
#endif
#if !defined(LIGHT) && !defined(_WIN32)
      /* These signals are not handled by WIN32 and not the Macintosh */
    case SIGQUIT:
    case SIGKILL:
      Yap_Error(INTERRUPT_ERROR,MkIntTerm(s),NULL);
#endif
#ifdef SIGUSR1
    case SIGUSR1:
      /* force the system to creep */
      Yap_signal (YAP_USR1_SIGNAL);
      break;
#endif /* defined(SIGUSR1) */
#ifdef SIGUSR2
    case SIGUSR2:
      /* force the system to creep */
      Yap_signal (YAP_USR2_SIGNAL);
      break;
#endif /* defined(SIGUSR2) */
#ifdef SIGPIPE
    case SIGPIPE:
      /* force the system to creep */
      Yap_signal (YAP_PIPE_SIGNAL);
      break;
#endif /* defined(SIGPIPE) */
#ifdef SIGHUP
    case SIGHUP:
      /* force the system to creep */
      Yap_signal (YAP_HUP_SIGNAL);
      break;
#endif /* defined(SIGHUP) */
    default:
      fprintf(GLOBAL_stderr, "\n[ Unexpected signal ]\n");
      exit (EXIT_FAILURE);
    }
}
#endif

#if (_MSC_VER || defined(__MINGW32__))
static BOOL WINAPI
MSCHandleSignal(DWORD dwCtrlType) {
  CACHE_REGS
  if (LOCAL_InterruptsDisabled) {
    return FALSE;
  }
  switch(dwCtrlType) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
    Yap_signal(YAP_ALARM_SIGNAL);
    LOCAL_PrologMode |= InterruptMode;
    return(TRUE);
  default:
    return(FALSE);
  }
}
#endif

/* SIGINT can cause problems, if caught before full initialization */
static void
InitSignals (void)
{
  if (GLOBAL_PrologShouldHandleInterrupts) {
#if !defined(LIGHT) && !_MSC_VER && !defined(__MINGW32__) && !defined(LIGHT) 
    my_signal (SIGQUIT, ReceiveSignal);
    my_signal (SIGKILL, ReceiveSignal);
    my_signal (SIGUSR1, ReceiveSignal);
    my_signal (SIGUSR2, ReceiveSignal);
    my_signal (SIGHUP,  ReceiveSignal);
    my_signal (SIGALRM, HandleALRM);
    my_signal (SIGVTALRM, HandleVTALRM);
#endif
#ifdef SIGPIPE
    my_signal (SIGPIPE, ReceiveSignal);
#endif
#if _MSC_VER || defined(__MINGW32__)
    signal (SIGINT, SIG_IGN);
    SetConsoleCtrlHandler(MSCHandleSignal,TRUE);
#else
    my_signal (SIGINT, HandleSIGINT);
#endif
#ifndef MPW
    my_signal (SIGFPE, HandleMatherr);
#endif
#if HAVE_SIGSEGV && !defined(THREADS) 
    my_signal_info (SIGSEGV, HandleSIGSEGV);
#endif
#ifdef YAPOR_COW
    signal(SIGCHLD, SIG_IGN);  /* avoid ghosts */ 
#endif
  } else {
#if OS_HANDLES_TR_OVERFLOW
#if HAVE_SIGSEGV && !defined(THREADS)
    my_signal_info (SIGSEGV, HandleSIGSEGV);
#endif    
#endif
  }
}

#endif /* HAVE_SIGNAL */


/* TrueFileName -> Finds the true name of a file */

#ifdef __MINGW32__
#include <ctype.h>
#endif

static int
volume_header(char *file)
{
#if _MSC_VER || defined(__MINGW32__)
  char *ch = file;
  int c;

  while ((c = ch[0]) != '\0') {
    if (isalnum(c)) ch++;
    else return(c == ':');
  }
#endif
  return(FALSE);
}

int
Yap_volume_header(char *file)
{
  return volume_header(file);
}


int Yap_getcwd(const char *buf, int len)
{
  CACHE_REGS
#if __simplescalar__
  /* does not implement getcwd */
  strncpy(Yap_buf,GLOBAL_pwd,len);
#elif HAVE_GETCWD
  if (getcwd ((char *)buf, len) == NULL) {
#if HAVE_STRERROR
    Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "%s in getcwd/1", strerror(errno));
#else
    Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "error %d in getcwd/1", errno);
#endif
    return FALSE;
  }
#else
  if (getwd (buf) == NULL) {
#if HAVE_STRERROR
    Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "%s in getcwd/1", strerror(errno));
#else
    Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "in getcwd/1");
#endif
    return FALSE;
  }
#endif
  return TRUE;
}

/******
      TODO: rewrite to use wordexp
 ****/
static int
TrueFileName (char *source, char *root, char *result, int in_lib)
{
  CACHE_REGS
  char *work;
  char ares1[YAP_FILENAME_MAX];

  result[0] = '\0';
#if defined(__MINGW32__) || _MSC_VER
  /* step 0: replace / by \ */
  strncpy(ares1, source, YAP_FILENAME_MAX);
  {
    char *p = ares1, ch = p[0];
    while (ch != '\0') {
      if (ch == '/') p[0] = '\\';
      p++;
      ch = p[0];
    }
  }
  source = ares1;
#endif
  /* step 1: eating home information */
  if (source[0] == '~') {
    if (dir_separator(source[1]) || source[1] == '\0')
      {
	char *s;
	source++;
#if defined(_WIN32)
	s = getenv("HOMEDRIVE");
	if (s != NULL)
	  strncpy (result, getenv ("HOMEDRIVE"), YAP_FILENAME_MAX);
	s = getenv("HOMEPATH");
	if (s != NULL)
	  strncpy (result, s, YAP_FILENAME_MAX);
#else
	s = getenv ("HOME");
	if (s != NULL)
	  strncpy (result, s, YAP_FILENAME_MAX);
#endif
      } else {
#if HAVE_GETPWNAM
      struct passwd *user_passwd;
      char *res0 = result; 

      source++;
      while (!dir_separator((*res0 = *source)) && *res0 != '\0')
	res0++, source++;
      *res0++ = '\0';
      if ((user_passwd = getpwnam (result)) == NULL) {
	return FALSE;
      }
      strncpy (result, user_passwd->pw_dir, YAP_FILENAME_MAX);
#else
      return FALSE;
#endif
    }
    strncat (result, source, YAP_FILENAME_MAX);
  } else if (source[0] == '$') {
    /* follow SICStus expansion rules */
    int ch;
    char *s;
    char *res0 = source+1;

    while ((ch = *res0) && is_valid_env_char (ch)) {
      res0++;
    }
    *res0 = '\0';
    if (!(s = (char *) getenv (source+1))) {
      return FALSE;
    }
    *res0 = ch;
    strncpy (result, s, YAP_FILENAME_MAX);
    strncat (result, res0, YAP_FILENAME_MAX);
  } else {
    strncpy (result, source, YAP_FILENAME_MAX);
  }
  /* step 3: get the full file name */
  if (!dir_separator(result[0]) && !volume_header(result)) {
    if (!Yap_getcwd(ares1, YAP_FILENAME_MAX))
      return FALSE;
#if _MSC_VER || defined(__MINGW32__)
    strncat (ares1, "\\", YAP_FILENAME_MAX-1);
#else
    strncat (ares1, "/", YAP_FILENAME_MAX-1);
#endif
    if (root) {
      if (!dir_separator(root[0]) && !volume_header(root)) {
	strncat(ares1, root, YAP_FILENAME_MAX-1);
      } else {
	strncpy(ares1, root, YAP_FILENAME_MAX-1);
      }
#if _MSC_VER || defined(__MINGW32__)
      strncat (ares1, "\\", YAP_FILENAME_MAX-1);
#else
      strncat (ares1, "/", YAP_FILENAME_MAX-1);
#endif
    }
    strncat (ares1, result, YAP_FILENAME_MAX-1);
    if (in_lib) {
      int tmpf;
      if ((tmpf = open(ares1, O_RDONLY)) < 0) {
	/* not in current directory, let us try the library */
	if  (Yap_LibDir != NULL) {
	  strncpy(LOCAL_FileNameBuf, Yap_LibDir, YAP_FILENAME_MAX);
#if HAVE_GETENV
	} else {
	  char *yap_env = getenv("YAPLIBDIR");
	  if (yap_env != NULL) {
	    strncpy(ares1, yap_env, YAP_FILENAME_MAX);
#endif
	  } else {
#if _MSC_VER || defined(__MINGW32__)
	    if (libdir)
	      strncpy(ares1, libdir, YAP_FILENAME_MAX);
	    else
#endif
	      strncpy(ares1, YAP_LIBDIR, YAP_FILENAME_MAX);
	  }
#if HAVE_GETENV
	}
#endif
#if _MSC_VER || defined(__MINGW32__)
	strncat(ares1,"\\", YAP_FILENAME_MAX-1);
#else
	strncat(ares1,"/", YAP_FILENAME_MAX-1);
#endif
	strncat(ares1,result, YAP_FILENAME_MAX-1);
	if ((tmpf = open(ares1, O_RDONLY)) >= 0) {
	  close(tmpf);
	  strncpy (result, ares1, YAP_FILENAME_MAX);
	}
      } else {
	strncpy (result, ares1, YAP_FILENAME_MAX);
	close(tmpf);
      }
    } else {
      strncpy (result, ares1, YAP_FILENAME_MAX);
    }
  }
  /* step 4: simplifying the file name */
  work = result;
  while (*work != '\0')
    {
      char *new_work, *next_work;
      if (*work++ != '.')
	continue;
      if (*work != '.')
	{
	  if (!dir_separator(*work) || !dir_separator(work[-2]))
	    continue;
	  next_work = work + 1;
	  new_work = --work;
	}
      else
	{
	  if (!dir_separator(work[1]) || !dir_separator(work[-2]))
	    continue;
	  next_work = work + 2;
	  work -= 2;
	  if (work == result)
	    return (FALSE);
	  while (!dir_separator(*--work) && work != result);
	  if (work == result && !dir_separator(work[0]))
	    return (FALSE);
	  new_work = ++work;
	}
      while ((*new_work++ = *next_work++)!=0);
    }
  if (work != result && dir_separator(work[-1])) {
    /* should only do this on result being a directory */
    int ch0 = work[-1];
    work--;
    work[0] = '\0';
    if (!is_directory(result)) {
      /* put it back: */
      work[0] = ch0;
      work++;
    }
  }
  return TRUE;
}

int
Yap_TrueFileName (char *source, char *result, int in_lib)
{
  return TrueFileName (source, NULL, result, in_lib);
}

static Int
p_true_file_name ( USES_REGS1 )
{
  Term t = Deref(ARG1);
  
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"argument to true_file_name unbound");
    return FALSE;
  }
  if (!IsAtomTerm(t)) {
    Yap_Error(TYPE_ERROR_ATOM,t,"argument to true_file_name");
    return FALSE;
  }
  TrueFileName (RepAtom(AtomOfTerm(t))->StrOfAE, NULL, LOCAL_FileNameBuf, FALSE);
  return Yap_unify(ARG2, MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)));
}

static Int
p_true_file_name3 ( USES_REGS1 )
{
  Term t = Deref(ARG1), t2 = Deref(ARG2);
  char *root = NULL;
  
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR,t,"argument to true_file_name unbound");
    return FALSE;
  }
  if (!IsAtomTerm(t)) {
    Yap_Error(TYPE_ERROR_ATOM,t,"argument to true_file_name");
    return FALSE;
  }
  if (!IsVarTerm(t2)) {
    if (!IsAtomTerm(t)) {
      Yap_Error(TYPE_ERROR_ATOM,t2,"argument to true_file_name");
      return FALSE;
    }
    root = RepAtom(AtomOfTerm(t2))->StrOfAE;
  }
  TrueFileName (RepAtom(AtomOfTerm(t))->StrOfAE, root, LOCAL_FileNameBuf, FALSE);
  return Yap_unify(ARG3, MkAtomTerm(Yap_LookupAtom(LOCAL_FileNameBuf)));
}

/* Executes $SHELL under Prolog */

static Int
p_sh ( USES_REGS1 )
{				/* sh				 */
#ifdef HAVE_SYSTEM
  char *shell;
  shell = (char *) getenv ("SHELL");
  if (shell == NULL)
    shell = "/bin/sh";
  if (system (shell) < 0) {
#if HAVE_STRERROR
    Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "%s in sh/0", strerror(errno));
#else
    Yap_Error(OPERATING_SYSTEM_ERROR, TermNil, "in sh/0");
#endif
    return FALSE;
  }
  return TRUE;
#else
#ifdef MSH
  register char *shell;
  shell = "msh -i";
  system (shell);
  return (TRUE);
#else
  Yap_Error(SYSTEM_ERROR,TermNil,"sh not available in this configuration");
  return(FALSE);
#endif /* MSH */
#endif
}

static Int
p_shell ( USES_REGS1 )
{				/* '$shell'(+SystCommand)			 */
#if _MSC_VER || defined(__MINGW32__)
  Yap_Error(SYSTEM_ERROR,TermNil,"shell not available in this configuration");
  return FALSE;
#else
#if HAVE_SYSTEM 
  char *shell;
  register int bourne = FALSE;
  Term t1 = Deref (ARG1);

  shell = (char *) getenv ("SHELL");
  if (!strcmp (shell, "/bin/sh"))
    bourne = TRUE;
  if (shell == NIL)
    bourne = TRUE;
  /* Yap_CloseStreams(TRUE); */
  if (bourne)
    return system(RepAtom(AtomOfTerm(t1))->StrOfAE) == 0;
  else {
    int status = -1;
    int child = fork ();

    if (child == 0) {			/* let the children go */
      if (!execl (shell, shell, "-c", RepAtom(AtomOfTerm(t1))->StrOfAE , NULL)) {
	exit(-1);
      }
      exit(TRUE);
    }
    {				/* put the father on wait */
      int result = child < 0 ||
/* vsc:I am not sure this is used, Stevens say wait returns an integer.
#if NO_UNION_WAIT
*/
	wait ((&status)) != child ||
/*
#else
	wait ((union wait *) (&status)) != child ||
#endif
*/
	status == 0;
	return result;
      }
    }
#else /* HAVE_SYSTEM */
#ifdef MSH
  register char *shell;
  shell = "msh -i";
  /* Yap_CloseStreams(); */
  system (shell);
  return TRUE;
#else
  Yap_Error (SYSTEM_ERROR,TermNil,"shell not available in this configuration");
  return FALSE;
#endif
#endif /* HAVE_SYSTEM */
#endif /* _MSC_VER */
}

static Int
p_system ( USES_REGS1 )
{				/* '$system'(+SystCommand)	       */
#ifdef HAVE_SYSTEM
  Term t1 = Deref (ARG1);
  char *s;

  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR,t1,"argument to system/1 unbound");
    return FALSE;
  } else if (IsAtomTerm(t1)) {
    s = RepAtom(AtomOfTerm(t1))->StrOfAE;
  } else {
    if (!Yap_GetName (LOCAL_FileNameBuf, YAP_FILENAME_MAX, t1)) {
      Yap_Error(TYPE_ERROR_ATOM,t1,"argument to system/1");
      return FALSE;
    }
    s = LOCAL_FileNameBuf;
  }
  /* Yap_CloseStreams(TRUE); */
#if _MSC_VER
  _flushall();
#endif
  if (system (s)) {
#if HAVE_STRERROR
    Yap_Error(OPERATING_SYSTEM_ERROR,t1,"%s in system(%s)", strerror(errno), s);
#else
    Yap_Error(OPERATING_SYSTEM_ERROR,t1,"in system(%s)", s);
#endif
    return FALSE;
  }
  return TRUE;
#else
#ifdef MSH
  register char *shell;
  shell = "msh -i";
  /* Yap_CloseStreams(); */
  system (shell);
  return (TRUE);
#undef command
#else
  Yap_Error(SYSTEM_ERROR,TermNil,"sh not available in this machine");
  return(FALSE);
#endif
#endif /* HAVE_SYSTEM */
}



/* Rename a file */
static Int
p_mv ( USES_REGS1 )
{				/* rename(+OldName,+NewName)   */
#if HAVE_LINK
  int r;
  char oldname[YAP_FILENAME_MAX], newname[YAP_FILENAME_MAX];
  Term t1 = Deref (ARG1);
  Term t2 = Deref (ARG2);
  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR, t1, "first argument to rename/2 unbound");
  } else if (!IsAtomTerm(t1)) {
    Yap_Error(TYPE_ERROR_ATOM, t1, "first argument to rename/2 not atom");
  }
  if (IsVarTerm(t2)) {
    Yap_Error(INSTANTIATION_ERROR, t2, "second argument to rename/2 unbound");
  } else if (!IsAtomTerm(t2)) {
    Yap_Error(TYPE_ERROR_ATOM, t2, "second argument to rename/2 not atom");
  }
  TrueFileName (RepAtom(AtomOfTerm(t1))->StrOfAE, NULL, oldname, FALSE);
  TrueFileName (RepAtom(AtomOfTerm(t2))->StrOfAE, NULL, newname, FALSE);
  if ((r = link (oldname, newname)) == 0 && (r = unlink (oldname)) != 0)
    unlink (newname);
  if (r != 0) {
#if HAVE_STRERROR
    Yap_Error(OPERATING_SYSTEM_ERROR,t2,"%s in rename(%s,%s)", strerror(errno),oldname,newname);
#else
    Yap_Error(OPERATING_SYSTEM_ERROR,t2,"in rename(%s,%s)",oldname,newname);
#endif
    return FALSE;
  }
  return TRUE;
#else
  Yap_Error(SYSTEM_ERROR,TermNil,"rename/2 not available in this machine");
  return (FALSE);
#endif
}


#ifdef MAC

void
Yap_SetTextFile (name)
     char *name;
{
#ifdef MACC
  SetFileType (name, 'TEXT');
  SetFileSignature (name, 'EDIT');
#else
  FInfo f;
  FInfo *p = &f;
  GetFInfo (name, 0, p);
  p->fdType = 'TEXT';
#ifdef MPW
  if (mpwshell)
    p->fdCreator = 'MPS\0';
#endif
#ifndef LIGHT
  else
    p->fdCreator = 'EDIT';
#endif
  SetFInfo (name, 0, p);
#endif
}

#endif


/* return YAP's environment */
static Int p_getenv( USES_REGS1 )
{
#if HAVE_GETENV
  Term t1 = Deref(ARG1), to;
  char *s, *so;

  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR, t1,
	  "first arg of getenv/2");
    return(FALSE);
  } else if (!IsAtomTerm(t1)) {
    Yap_Error(TYPE_ERROR_ATOM, t1,
	  "first arg of getenv/2");
    return(FALSE);
  } else s = RepAtom(AtomOfTerm(t1))->StrOfAE;
  if ((so = getenv(s)) == NULL)
    return(FALSE);
  to = MkAtomTerm(Yap_LookupAtom(so));
  return(Yap_unify_constant(ARG2,to));
#else
    Yap_Error(SYSTEM_ERROR, TermNil,
	  "getenv not available in this configuration");
    return (FALSE);
#endif
}

/* set a variable in YAP's environment */
static Int p_putenv( USES_REGS1 )
{
#if HAVE_PUTENV
  Term t1 = Deref(ARG1), t2 = Deref(ARG2);
  char *s, *s2, *p0, *p;

  if (IsVarTerm(t1)) {
    Yap_Error(INSTANTIATION_ERROR, t1,
	  "first arg to putenv/2");
    return(FALSE);
  } else if (!IsAtomTerm(t1)) {
    Yap_Error(TYPE_ERROR_ATOM, t1,
	  "first arg to putenv/2");
    return(FALSE);
  } else s = RepAtom(AtomOfTerm(t1))->StrOfAE;
  if (IsVarTerm(t2)) {
    Yap_Error(INSTANTIATION_ERROR, t1,
	  "second arg to putenv/2");
    return(FALSE);
  } else if (!IsAtomTerm(t2)) {
    Yap_Error(TYPE_ERROR_ATOM, t2,
	  "second arg to putenv/2");
    return(FALSE);
  } else s2 = RepAtom(AtomOfTerm(t2))->StrOfAE;
  while (!(p0 = p = Yap_AllocAtomSpace(strlen(s)+strlen(s2)+3))) {
    if (!Yap_growheap(FALSE, MinHeapGap, NULL)) {
      Yap_Error(OUT_OF_HEAP_ERROR, TermNil, LOCAL_ErrorMessage);
      return FALSE;
    }
  }
  while ((*p++ = *s++) != '\0');
  p[-1] = '=';
  while ((*p++ = *s2++) != '\0');
  if (putenv(p0) == 0)
    return TRUE;
#if HAVE_STRERROR
  Yap_Error(OPERATING_SYSTEM_ERROR, TermNil,
	"in putenv(%s)", strerror(errno), p0);
#else
  Yap_Error(OPERATING_SYSTEM_ERROR, TermNil,
	"in putenv(%s)", p0);
#endif
  return FALSE;
#else
    Yap_Error(SYSTEM_ERROR, TermNil,
	  "putenv not available in this configuration");
    return FALSE;
#endif
}

/* wrapper for alarm system call */
#if _MSC_VER || defined(__MINGW32__)

static DWORD WINAPI
DoTimerThread(LPVOID targ)
{
  Int *time = (Int *)targ;
  HANDLE htimer;
  LARGE_INTEGER liDueTime;

  htimer = CreateWaitableTimer(NULL, FALSE, NULL);
  liDueTime.QuadPart =  -10000000;
  liDueTime.QuadPart *=  time[0];
  /* add time in usecs */
  liDueTime.QuadPart -=  time[1]*10;
  /* Copy the relative time into a LARGE_INTEGER. */
  if (SetWaitableTimer(htimer, &liDueTime,0,NULL,NULL,0) == 0) {
    return(FALSE);
  }
  if (WaitForSingleObject(htimer, INFINITE) != WAIT_OBJECT_0)
    fprintf(stderr,"WaitForSingleObject failed (%ld)\n", GetLastError());
  Yap_signal (YAP_ALARM_SIGNAL);
  /* now, say what is going on */
  Yap_PutValue(AtomAlarm, MkAtomTerm(AtomTrue));
  ExitThread(1);
#if _MSC_VER
  return(0L);
#endif
}

#endif

static Int
p_alarm( USES_REGS1 )
{
  Term t = Deref(ARG1);
  Term t2 = Deref(ARG2);
  Int i1, i2;
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR, t, "alarm/2");
    return(FALSE);
  }
  if (!IsIntegerTerm(t)) {
    Yap_Error(TYPE_ERROR_INTEGER, t, "alarm/2");
    return(FALSE);
  }
  if (IsVarTerm(t2)) {
    Yap_Error(INSTANTIATION_ERROR, t2, "alarm/2");
    return(FALSE);
  }
  if (!IsIntegerTerm(t2)) {
    Yap_Error(TYPE_ERROR_INTEGER, t2, "alarm/2");
    return(FALSE);
  }
  i1 = IntegerOfTerm(t);
  i2 = IntegerOfTerm(t2);
  if (i1 == 0 && i2 == 0) {
    LOCK(LOCAL_SignalLock);
    if (LOCAL_ActiveSignals & YAP_ALARM_SIGNAL) {
      LOCAL_ActiveSignals &= ~YAP_ALARM_SIGNAL;
      if (!LOCAL_ActiveSignals) {
	CreepFlag = CalculateStackGap();
      }
    }
    UNLOCK(LOCAL_SignalLock);
  }    
#if _MSC_VER || defined(__MINGW32__)
  {
    Term tout;
    Int time[2];

    time[0] = i1;
    time[1] = i2;
    
    if (time[0] != 0 && time[1] != 0) {
      DWORD dwThreadId; 
      HANDLE hThread; 

      hThread = CreateThread( 
			     NULL,     /* no security attributes */
			     0,        /* use default stack size */ 
			     DoTimerThread, /* thread function */
			     (LPVOID)time,  /* argument to thread function */
			     0,        /* use default creation flags  */
			     &dwThreadId);  /* returns the thread identifier */
 
      /* Check the return value for success. */
      if (hThread == NULL) {
	Yap_WinError("trying to use alarm");
      }
    }
    tout = MkIntegerTerm(0);
    return Yap_unify(ARG3,tout) && Yap_unify(ARG4,MkIntTerm(0));
  }
#elif HAVE_SETITIMER && !SUPPORT_CONDOR
  {
    struct itimerval new, old;

    new.it_interval.tv_sec = 0;
    new.it_interval.tv_usec = 0;
    new.it_value.tv_sec = i1;
    new.it_value.tv_usec = i2;
    if (setitimer(ITIMER_REAL, &new, &old) < 0) {
#if HAVE_STRERROR
      Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "setitimer: %s", strerror(errno));
#else
      Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "setitimer %d", errno);
#endif
      return FALSE;
    }
    return Yap_unify(ARG3,MkIntegerTerm(old.it_value.tv_sec)) &&
      Yap_unify(ARG4,MkIntegerTerm(old.it_value.tv_usec));
  }
#elif HAVE_ALARM && !SUPPORT_CONDOR
  {
    Int left;
    Term tout;

    left = alarm(i1);
    tout = MkIntegerTerm(left);
    return Yap_unify(ARG3,tout) && Yap_unify(ARG4,MkIntTerm(0)) ;
  }
#else
  /* not actually trying to set the alarm */
  if (IntegerOfTerm(t) == 0)
    return TRUE;
  Yap_Error(SYSTEM_ERROR, TermNil,
	"alarm not available in this configuration");
  return FALSE;
#endif
}

static Int
p_virtual_alarm( USES_REGS1 )
{
  Term t = Deref(ARG1);
  Term t2 = Deref(ARG2);
  if (IsVarTerm(t)) {
    Yap_Error(INSTANTIATION_ERROR, t, "alarm/2");
    return(FALSE);
  }
  if (!IsIntegerTerm(t)) {
    Yap_Error(TYPE_ERROR_INTEGER, t, "alarm/2");
    return(FALSE);
  }
  if (IsVarTerm(t2)) {
    Yap_Error(INSTANTIATION_ERROR, t2, "alarm/2");
    return(FALSE);
  }
  if (!IsIntegerTerm(t2)) {
    Yap_Error(TYPE_ERROR_INTEGER, t2, "alarm/2");
    return(FALSE);
  }
#if _MSC_VER || defined(__MINGW32__)
  {
    Term tout;
    Int time[2];

    time[0] = IntegerOfTerm(t);
    time[1] = IntegerOfTerm(t2);
    
    if (time[0] != 0 && time[1] != 0) {
      DWORD dwThreadId; 
      HANDLE hThread; 

      hThread = CreateThread( 
			     NULL,     /* no security attributes */
			     0,        /* use default stack size */ 
			     DoTimerThread, /* thread function */
			     (LPVOID)time,  /* argument to thread function */
			     0,        /* use default creation flags  */
			     &dwThreadId);  /* returns the thread identifier */
 
      /* Check the return value for success. */
      if (hThread == NULL) {
	Yap_WinError("trying to use alarm");
      }
    }
    tout = MkIntegerTerm(0);
    return Yap_unify(ARG3,tout) && Yap_unify(ARG4,MkIntTerm(0));
  }
#elif HAVE_SETITIMER && !SUPPORT_CONDOR
  {
    struct itimerval new, old;

    new.it_interval.tv_sec = 0;
    new.it_interval.tv_usec = 0;
    new.it_value.tv_sec = IntegerOfTerm(t);
    new.it_value.tv_usec = IntegerOfTerm(t2);
    if (setitimer(ITIMER_VIRTUAL, &new, &old) < 0) {
#if HAVE_STRERROR
      Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "setitimer: %s", strerror(errno));
#else
      Yap_Error(OPERATING_SYSTEM_ERROR, ARG1, "setitimer %d", errno);
#endif
      return FALSE;
    }
    return Yap_unify(ARG3,MkIntegerTerm(old.it_value.tv_sec)) &&
      Yap_unify(ARG4,MkIntegerTerm(old.it_value.tv_usec));
  }
#else
  /* not actually trying to set the alarm */
  if (IntegerOfTerm(t) == 0)
    return TRUE;
  Yap_Error(SYSTEM_ERROR, TermNil,
	"virtual_alarm not available in this configuration");
  return FALSE;
#endif
}

#if HAVE_FPU_CONTROL_H
#include <fpu_control.h>
#endif

/* by default Linux with glibc is IEEE compliant anyway..., but we will pretend it is not. */
static void
set_fpu_exceptions(int flag)
{
  if (flag) {
#if defined(__hpux)
# if HAVE_FESETTRAPENABLE
/* From HP-UX 11.0 onwards: */
    fesettrapenable(FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW|FE_UNDERFLOW);
# else
/*
  Up until HP-UX 10.20:
  FP_X_INV   invalid operation exceptions
  FP_X_DZ    divide-by-zero exception
  FP_X_OFL   overflow exception
  FP_X_UFL   underflow exception
  FP_X_IMP   imprecise (inexact result)
  FP_X_CLEAR simply zero to clear all flags
*/
    fpsetmask(FP_X_INV|FP_X_DZ|FP_X_OFL|FP_X_UFL);
# endif
#endif /* __hpux */
#if HAVE_FPU_CONTROL_H && i386 && defined(__GNUC__)
    /* I shall ignore denormalization and precision errors */
    int v = _FPU_IEEE & ~(_FPU_MASK_IM|_FPU_MASK_ZM|_FPU_MASK_OM|_FPU_MASK_UM);
    _FPU_SETCW(v);
#endif
#if HAVE_FETESTEXCEPT
    feclearexcept(FE_ALL_EXCEPT);
#endif
    my_signal (SIGFPE, HandleMatherr);
  } else {
    /* do IEEE arithmetic in the way the big boys do */
#if defined(__hpux)
# if HAVE_FESETTRAPENABLE
    fesettrapenable(FE_ALL_EXCEPT);
# else
    fpsetmask(FP_X_CLEAR);
# endif
#endif /* __hpux */
#if HAVE_FPU_CONTROL_H && i386 && defined(__GNUC__)
    /* this will probably not work in older releases of Linux */
    int v = _FPU_IEEE;
   _FPU_SETCW(v);
#endif    
    my_signal (SIGFPE, SIG_IGN);
  }
}

void
Yap_set_fpu_exceptions(int flag)
{
  set_fpu_exceptions(flag);
}
static Int
p_set_fpu_exceptions( USES_REGS1 ) {
  if (yap_flags[LANGUAGE_MODE_FLAG] == 1) {
    set_fpu_exceptions(FALSE); /* can't make it work right */
  } else {
    set_fpu_exceptions(FALSE);
  }
  return(TRUE);
}

static Int
p_host_type( USES_REGS1 ) {
  Term out = MkAtomTerm(Yap_LookupAtom(HOST_ALIAS));
  return(Yap_unify(out,ARG1));
}

static Int
p_yap_home( USES_REGS1 ) {
  Term out = MkAtomTerm(Yap_LookupAtom(YAP_ROOTDIR));
  return(Yap_unify(out,ARG1));
}

static Int
p_env_separator( USES_REGS1 ) {
#if defined(_WIN32)
  return Yap_unify(MkIntegerTerm(';'),ARG1);
#else
  return Yap_unify(MkIntegerTerm(':'),ARG1);
#endif
}

/*
 * This is responsable for the initialization of all machine dependant
 * predicates
 */
void
Yap_InitSysbits (void)
{
#if  __simplescalar__
  {
    char *pwd = getenv("PWD");
    strncpy(GLOBAL_pwd,pwd,YAP_FILENAME_MAX);
  }
#endif
  InitWTime ();
  InitRandom ();
  /* let the caller control signals as it sees fit */
  InitSignals ();
}

void
Yap_InitTime( int wid )
{
  InitTime( wid );
}

void
Yap_ReInitWallTime (void)
{
  InitWTime();
  if (Yap_heap_regs->last_wtime != NULL) 
    Yap_FreeCodeSpace(Yap_heap_regs->last_wtime);
  InitLastWtime();
}

static Int
p_first_signal( USES_REGS1 )
{
  LOCK(LOCAL_SignalLock);
#ifdef THREADS
  pthread_mutex_lock(&(LOCAL_ThreadHandle.tlock));
#endif  
  /* always do wakeups first, because you don't want to keep the
     non-backtrackable variable bad */
  if (LOCAL_ActiveSignals & YAP_WAKEUP_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_WAKEUP_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigWakeUp));
  }
  if (LOCAL_ActiveSignals & YAP_ITI_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_ITI_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigIti));
  }
  if (LOCAL_ActiveSignals & YAP_INT_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_INT_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigInt));
  }
  if (LOCAL_ActiveSignals & YAP_USR2_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_USR2_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigUsr2));
  }
  if (LOCAL_ActiveSignals & YAP_USR1_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_USR1_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigUsr1));
  }
  if (LOCAL_ActiveSignals & YAP_PIPE_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_PIPE_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigPipe));
  }
  if (LOCAL_ActiveSignals & YAP_HUP_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_HUP_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigHup));
  }
  if (LOCAL_ActiveSignals & YAP_ALARM_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_ALARM_SIGNAL;
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigAlarm));
  }
  if (LOCAL_ActiveSignals & YAP_VTALARM_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_VTALARM_SIGNAL;
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigVTAlarm));
  }
  if (LOCAL_ActiveSignals & YAP_DELAY_CREEP_SIGNAL) {
    LOCAL_ActiveSignals &= ~(YAP_CREEP_SIGNAL|YAP_DELAY_CREEP_SIGNAL);
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigDelayCreep));
  }
  if (LOCAL_ActiveSignals & YAP_CREEP_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_CREEP_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigCreep));
  }
  if (LOCAL_ActiveSignals & YAP_TRACE_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_TRACE_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigTrace));
  }
  if (LOCAL_ActiveSignals & YAP_DEBUG_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_DEBUG_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigDebug));
  }
  if (LOCAL_ActiveSignals & YAP_BREAK_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_BREAK_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigBreak));
  }
  if (LOCAL_ActiveSignals & YAP_STACK_DUMP_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_STACK_DUMP_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigStackDump));
  }
  if (LOCAL_ActiveSignals & YAP_STATISTICS_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_STATISTICS_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomSigStatistics));
  }
  if (LOCAL_ActiveSignals & YAP_FAIL_SIGNAL) {
    LOCAL_ActiveSignals &= ~YAP_FAIL_SIGNAL;
#ifdef THREADS
    pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
    UNLOCK(LOCAL_SignalLock);
    return Yap_unify(ARG1, MkAtomTerm(AtomFail));
  }
#ifdef THREADS
  pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
  UNLOCK(LOCAL_SignalLock);
  return FALSE;
}

static Int
p_continue_signals( USES_REGS1 )
{
  /* hack to force the signal anew */
  if (LOCAL_ActiveSignals & YAP_ITI_SIGNAL) {
    Yap_signal(YAP_ITI_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_INT_SIGNAL) {
    Yap_signal(YAP_INT_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_USR2_SIGNAL) {
    Yap_signal(YAP_USR2_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_USR1_SIGNAL) {
    Yap_signal(YAP_USR1_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_HUP_SIGNAL) {
    Yap_signal(YAP_HUP_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_ALARM_SIGNAL) {
    Yap_signal(YAP_ALARM_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_VTALARM_SIGNAL) {
    Yap_signal(YAP_VTALARM_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_CREEP_SIGNAL) {
    Yap_signal(YAP_CREEP_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_DELAY_CREEP_SIGNAL) {
    Yap_signal(YAP_DELAY_CREEP_SIGNAL|YAP_CREEP_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_TRACE_SIGNAL) {
    Yap_signal(YAP_TRACE_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_DEBUG_SIGNAL) {
    Yap_signal(YAP_DEBUG_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_BREAK_SIGNAL) {
    Yap_signal(YAP_BREAK_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_STACK_DUMP_SIGNAL) {
    Yap_signal(YAP_STACK_DUMP_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_STATISTICS_SIGNAL) {
    Yap_signal(YAP_STATISTICS_SIGNAL);
  }
  if (LOCAL_ActiveSignals & YAP_FAIL_SIGNAL) {
    Yap_signal(YAP_FAIL_SIGNAL);
  }
#ifdef THREADS
  pthread_mutex_unlock(&(LOCAL_ThreadHandle.tlock));
#endif  
  return TRUE;
}

static Int
p_unix( USES_REGS1 )
{
#ifdef unix
  return TRUE;
#else
#ifdef __unix__
  return TRUE;
#else
#ifdef __APPLE__
  return TRUE;
#else
  return FALSE;
#endif
#endif
#endif
}

static Int
p_win32( USES_REGS1 )
{
#ifdef _WIN32
  return TRUE;
#else
#ifdef __CYGWIN__
  return TRUE;
#else
  return FALSE;
#endif
#endif
}


static Int
p_enable_interrupts( USES_REGS1 )
{
  LOCK(LOCAL_SignalLock);
  LOCAL_InterruptsDisabled--;
  if (LOCAL_ActiveSignals && !LOCAL_InterruptsDisabled) {
    CreepFlag = Unsigned(LCL0);
  }
  UNLOCK(LOCAL_SignalLock);
  return TRUE;
}

static Int
p_disable_interrupts( USES_REGS1 )
{
  LOCK(LOCAL_SignalLock);
  LOCAL_InterruptsDisabled++;
  if (LOCAL_ActiveSignals) {
    CreepFlag = CalculateStackGap();
  }
  UNLOCK(LOCAL_SignalLock);
  return TRUE;
}

static Int
p_ld_path( USES_REGS1 )
{
  return Yap_unify(ARG1,MkAtomTerm(Yap_LookupAtom(YAP_LIBDIR)));
}

static Int
p_address_bits( USES_REGS1 )
{
#if SIZEOF_INT_P==4
  return Yap_unify(ARG1,MkIntTerm(32));
#else
  return Yap_unify(ARG1,MkIntTerm(64));
#endif
}



#ifdef _WIN32

/* This code is from SWI-Prolog by Jan Wielemaker */

#define wstreq(s,q) (wcscmp((s), (q)) == 0)

static HKEY
reg_open_key(const wchar_t *which, int create)
{ HKEY key = HKEY_CURRENT_USER;
  DWORD disp;
  LONG rval;

  while(*which)
  { wchar_t buf[256];
    wchar_t *s;
    HKEY tmp;

    for(s=buf; *which && !(*which == '/' || *which == '\\'); )
      *s++ = *which++;
    *s = '\0';
    if ( *which )
      which++;

    if ( wstreq(buf, L"HKEY_CLASSES_ROOT") )
    { key = HKEY_CLASSES_ROOT;
      continue;
    } else if ( wstreq(buf, L"HKEY_CURRENT_USER") )
    { key = HKEY_CURRENT_USER;
      continue;
    } else if ( wstreq(buf, L"HKEY_LOCAL_MACHINE") )
    { key = HKEY_LOCAL_MACHINE;
      continue;
    } else if ( wstreq(buf, L"HKEY_USERS") )
    { key = HKEY_USERS;
      continue;
    }

    if ( RegOpenKeyExW(key, buf, 0L, KEY_READ, &tmp) == ERROR_SUCCESS )
    { RegCloseKey(key);
      key = tmp;
      continue;
    }

    if ( !create )
      return NULL;

    rval = RegCreateKeyExW(key, buf, 0, L"", 0,
			  KEY_ALL_ACCESS, NULL, &tmp, &disp);
    RegCloseKey(key);
    if ( rval == ERROR_SUCCESS )
      key = tmp;
    else
      return NULL;
  }

  return key;
}

#define MAXREGSTRLEN 1024

static void
recover_space(wchar_t *k, Atom At)
{
  if (At->WStrOfAE != k)
    Yap_FreeCodeSpace((char *)k);
}

static wchar_t *
WideStringFromAtom(Atom KeyAt)
{
  if (IsWideAtom(KeyAt)) {
    return KeyAt->WStrOfAE;
  } else {
    int len = strlen(KeyAt->StrOfAE);
    int sz = sizeof(wchar_t)*(len+1);
    char *chp = KeyAt->StrOfAE;
    wchar_t *kptr, *k;

    k = (wchar_t *)Yap_AllocCodeSpace(sz);
    while (k == NULL) {
      if (!Yap_growheap(FALSE, sz, NULL)) {
	Yap_Error(OUT_OF_HEAP_ERROR, MkIntegerTerm(sz), "generating key in win_registry_get_value/3");
	return FALSE;
      }      
    }
    kptr = k;
    while ((*kptr++ = *chp++));
    return k;
  }
}

static Int
p_win_registry_get_value( USES_REGS1 )
{
  DWORD type;
  BYTE  data[MAXREGSTRLEN];
  DWORD len = sizeof(data);
  wchar_t *k, *name;
  HKEY key;
  Term Key = Deref(ARG1);
  Term Name = Deref(ARG2);
  Atom KeyAt, NameAt;

  if (IsVarTerm(Key)) {
    Yap_Error(INSTANTIATION_ERROR,Key,"argument to win_registry_get_value unbound");
    return FALSE;
  }
  if (!IsAtomTerm(Key)) {
    Yap_Error(TYPE_ERROR_ATOM,Key,"argument to win_registry_get_value");
    return FALSE;
  }
  KeyAt = AtomOfTerm(Key);
  if (IsVarTerm(Name)) {
    Yap_Error(INSTANTIATION_ERROR,Key,"argument to win_registry_get_value unbound");
    return FALSE;
  }
  if (!IsAtomTerm(Name)) {
    Yap_Error(TYPE_ERROR_ATOM,Key,"argument to win_registry_get_value");
    return FALSE;
  }
  NameAt = AtomOfTerm(Name);

  k = WideStringFromAtom(KeyAt);
  if ( !(key=reg_open_key(k, FALSE)) ) {
    Yap_Error(EXISTENCE_ERROR_KEY, Key, "argument to win_registry_get_value");
    recover_space(k, KeyAt);
    return FALSE;
  }
  name = WideStringFromAtom(NameAt);

  if ( RegQueryValueExW(key, name, NULL, &type, data, &len) == ERROR_SUCCESS ) {
    RegCloseKey(key);
    switch(type) {
    case REG_SZ:
      recover_space(k, KeyAt);
      recover_space(name, NameAt);
      ((wchar_t *)data)[len] = '\0';
      return Yap_unify(MkAtomTerm(Yap_LookupMaybeWideAtom((wchar_t *)data)),ARG3);
    case REG_DWORD:
      recover_space(k, KeyAt);
      recover_space(name, NameAt);
      {
	DWORD *d = (DWORD *)data;
	return Yap_unify(MkIntegerTerm((Int)d[0]),ARG3);
      }
    default:
      recover_space(k, KeyAt);
      recover_space(name, NameAt);
      return FALSE;
    }
  }
  recover_space(k, KeyAt);
  recover_space(name, NameAt);
  return FALSE;
}

char *
Yap_RegistryGetString(char *name)
{
  DWORD type;
  BYTE  data[MAXREGSTRLEN];
  DWORD len = sizeof(data);
  HKEY key;
  char *ptr;
  int i;

#if SIZEOF_INT_P == 8
  if ( !(key=reg_open_key(L"HKEY_LOCAL_MACHINE/SOFTWARE/YAP/Prolog64", FALSE)) ) {
    return NULL;
  }
#else
  if ( !(key=reg_open_key(L"HKEY_LOCAL_MACHINE/SOFTWARE/YAP/Prolog", FALSE)) ) {
    return NULL;
  }
#endif
  if ( RegQueryValueEx(key, name, NULL, &type, data, &len) == ERROR_SUCCESS ) {
    RegCloseKey(key);
    switch(type) {
    case REG_SZ:
      ptr = malloc(len+2);
      if (!ptr)
	return NULL;
      for (i=0; i<= len; i++)
	ptr[i] = data[i];
      ptr[len+1] = '\0';
      return ptr;
    default:
      return NULL;
    }
  }
  return NULL;
}


#endif

void
Yap_InitSysPreds(void)
{
  CACHE_REGS
  Term cm = CurrentModule;

  /* can only do after heap is initialised */
  InitLastWtime();
  Yap_InitCPred ("srandom", 1, p_srandom, SafePredFlag);
  Yap_InitCPred ("sh", 0, p_sh, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$shell", 1, p_shell, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("system", 1, p_system, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("rename", 2, p_mv, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$yap_home", 1, p_yap_home, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$dir_separator", 1, p_dir_sp, SafePredFlag);
  Yap_InitCPred ("$alarm", 4, p_alarm, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$getenv", 2, p_getenv, SafePredFlag);
  Yap_InitCPred ("$putenv", 2, p_putenv, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$set_fpu_exceptions", 0, p_set_fpu_exceptions, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$first_signal", 1, p_first_signal, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$host_type", 1, p_host_type, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$continue_signals", 0, p_continue_signals, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("$env_separator", 1, p_env_separator, SafePredFlag);
  Yap_InitCPred ("$unix", 0, p_unix, SafePredFlag);
  Yap_InitCPred ("$win32", 0, p_win32, SafePredFlag);
  Yap_InitCPred ("$ld_path", 1, p_ld_path, SafePredFlag);
  Yap_InitCPred ("$address_bits", 1, p_address_bits, SafePredFlag);
#ifdef _WIN32
  Yap_InitCPred ("win_registry_get_value", 3, p_win_registry_get_value,0);
#endif
  CurrentModule = HACKS_MODULE;
  Yap_InitCPred ("virtual_alarm", 4, p_virtual_alarm, SafePredFlag|SyncPredFlag);
  Yap_InitCPred ("enable_interrupts", 0, p_enable_interrupts, SafePredFlag);
  Yap_InitCPred ("disable_interrupts", 0, p_disable_interrupts, SafePredFlag);
  CurrentModule = OPERATING_SYSTEM_MODULE;
  Yap_InitCPred ("true_file_name", 2, p_true_file_name, SyncPredFlag);
  Yap_InitCPred ("true_file_name", 3, p_true_file_name3, SyncPredFlag);
  CurrentModule = cm;
}


#ifdef VAX

/* avoid longjmp botch */

int vax_absmi_fp;

typedef struct
  {
    int eh;
    int flgs;
    int ap;
    int fp;
    int pc;
    int dummy1;
    int dummy2;
    int dummy3;
    int oldfp;
    int dummy4;
    int dummy5;
    int dummy6;
    int oldpc;
  }

 *VaxFramePtr;


VaxFixFrame (dummy)
{
  int maxframes = 100;
  VaxFramePtr fp = (VaxFramePtr) (((int *) &dummy) - 6);
  while (--maxframes)
    {
      fp = (VaxFramePtr) fp->fp;
      if (fp->flgs == 0)
	{
	  if (fp->oldfp >= &REGS[6] && fp->oldfp < &REGS[REG_SIZE])
	    fp->oldfp = vax_absmi_fp;
	  return;
	}
    }
}

#endif


#if defined(_WIN32)

#include <windows.h>

int WINAPI STD_PROTO(win_yap, (HANDLE, DWORD, LPVOID));

int WINAPI win_yap(HANDLE hinst, DWORD reason, LPVOID reserved)
{
  switch (reason) 
    {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    }
  return 1;
}
#endif

#if (defined(YAPOR) || defined(THREADS)) && !defined(USE_PTHREAD_LOCKING)
#ifdef sparc
void STD_PROTO(rw_lock_voodoo,(void));

void
rw_lock_voodoo(void) {
  /* code taken from the Linux kernel, it handles shifting between locks */
  /* Read/writer locks, as usual this is overly clever to make it as fast as possible. */
	/* caches... */
	__asm__ __volatile__(
"___rw_read_enter_spin_on_wlock:\n"
"	orcc	%g2, 0x0, %g0\n"
"	be,a	___rw_read_enter\n"
"	 ldstub	[%g1 + 3], %g2\n"
"	b	___rw_read_enter_spin_on_wlock\n"
"	 ldub	[%g1 + 3], %g2\n"
"___rw_read_exit_spin_on_wlock:\n"
"	orcc	%g2, 0x0, %g0\n"
"	be,a	___rw_read_exit\n"
"	 ldstub	[%g1 + 3], %g2\n"
"	b	___rw_read_exit_spin_on_wlock\n"
"	 ldub	[%g1 + 3], %g2\n"
"___rw_write_enter_spin_on_wlock:\n"
"	orcc	%g2, 0x0, %g0\n"
"	be,a	___rw_write_enter\n"
"	 ldstub	[%g1 + 3], %g2\n"
"	b	___rw_write_enter_spin_on_wlock\n"
"	 ld	[%g1], %g2\n"
"\n"
"	.globl	___rw_read_enter\n"
"___rw_read_enter:\n"
"	orcc	%g2, 0x0, %g0\n"
"	bne,a	___rw_read_enter_spin_on_wlock\n"
"	 ldub	[%g1 + 3], %g2\n"
"	ld	[%g1], %g2\n"
"	add	%g2, 1, %g2\n"
"	st	%g2, [%g1]\n"
"	retl\n"
"	 mov	%g4, %o7\n"
"	.globl	___rw_read_exit\n"
"___rw_read_exit:\n"
"	orcc	%g2, 0x0, %g0\n"
"	bne,a	___rw_read_exit_spin_on_wlock\n"
"	 ldub	[%g1 + 3], %g2\n"
"	ld	[%g1], %g2\n"
"	sub	%g2, 0x1ff, %g2\n"
"	st	%g2, [%g1]\n"
"	retl\n"
"	 mov	%g4, %o7\n"
"	.globl	___rw_write_enter\n"
"___rw_write_enter:\n"
"	orcc	%g2, 0x0, %g0\n"
"	bne	___rw_write_enter_spin_on_wlock\n"
"	 ld	[%g1], %g2\n"
"	andncc	%g2, 0xff, %g0\n"
"	bne,a	___rw_write_enter_spin_on_wlock\n"
"	 stb	%g0, [%g1 + 3]\n"
"	retl\n"
"	 mov	%g4, %o7\n"
   );
}
#endif /* sparc */


#endif /* YAPOR || THREADS */
