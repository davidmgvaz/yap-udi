
  /* This file, rhstruct.h, was generated automatically by "yap -L misc/buildheap"
     please do not update, update misc/HEAPFIELDS instead */






























#if USE_DL_MALLOC


#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(DLMallocLock);
#endif
#endif
#if USE_DL_MALLOC || (USE_SYSTEM_MALLOC && HAVE_MALLINFO)
#ifndef  HeapUsed
#define  HeapUsed  Yap_givemallinfo()		
#endif

#else

#endif




#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(FreeBlocksLock);
  REINIT_LOCK(HeapUsedLock);
  REINIT_LOCK(HeapTopLock);

#endif




#if USE_THREADED_CODE
  OP_RTABLE = OpRTableAdjust(OP_RTABLE);
#endif

  EXECUTE_CPRED_OP_CODE = Yap_opcode(_execute_cpred);
  EXPAND_OP_CODE = Yap_opcode(_expand_index);
  FAIL_OPCODE = Yap_opcode(_op_fail);
  INDEX_OPCODE = Yap_opcode(_index_pred);
  LOCKPRED_OPCODE = Yap_opcode(_lock_pred);
  ORLAST_OPCODE = Yap_opcode(_or_last);
  UNDEF_OPCODE = Yap_opcode(_undef_p);





  RestoreInvisibleAtoms();
  RestoreWideAtoms();
  RestoreAtoms();

#include "ratoms.h"
#ifdef EUROTRA
  TermDollarU = AtomTermAdjust(TermDollarU);
#endif
  TermProlog = AtomTermAdjust(TermProlog);
  TermReFoundVar = AtomTermAdjust(TermReFoundVar);
  USER_MODULE = AtomTermAdjust(USER_MODULE);
  IDB_MODULE = AtomTermAdjust(IDB_MODULE);
  ATTRIBUTES_MODULE = AtomTermAdjust(ATTRIBUTES_MODULE);
  CHARSIO_MODULE = AtomTermAdjust(CHARSIO_MODULE);
  TERMS_MODULE = AtomTermAdjust(TERMS_MODULE);
  SYSTEM_MODULE = AtomTermAdjust(SYSTEM_MODULE);
  OPERATING_SYSTEM_MODULE = AtomTermAdjust(OPERATING_SYSTEM_MODULE);
  READUTIL_MODULE = AtomTermAdjust(READUTIL_MODULE);
  HACKS_MODULE = AtomTermAdjust(HACKS_MODULE);
  ARG_MODULE = AtomTermAdjust(ARG_MODULE);
  GLOBALS_MODULE = AtomTermAdjust(GLOBALS_MODULE);
  SWI_MODULE = AtomTermAdjust(SWI_MODULE);
  DBLOAD_MODULE = AtomTermAdjust(DBLOAD_MODULE);
  RANGE_MODULE = AtomTermAdjust(RANGE_MODULE);

  RestoreHiddenPredicates();



  CurrentModules = ModEntryPtrAdjust(CurrentModules);






  RestorePredHash();
#if defined(YAPOR) || defined(THREADS)

#endif



  CreepCode = PtoPredAdjust(CreepCode);
  UndefCode = PtoPredAdjust(UndefCode);
  SpyCode = PtoPredAdjust(SpyCode);
  PredFail = PtoPredAdjust(PredFail);
  PredTrue = PtoPredAdjust(PredTrue);
#ifdef COROUTINING
  WakeUpCode = PtoPredAdjust(WakeUpCode);
#endif
  PredGoalExpansion = PtoPredAdjust(PredGoalExpansion);
  PredMetaCall = PtoPredAdjust(PredMetaCall);
  PredDollarCatch = PtoPredAdjust(PredDollarCatch);
  PredRecordedWithKey = PtoPredAdjust(PredRecordedWithKey);
  PredLogUpdClause = PtoPredAdjust(PredLogUpdClause);
  PredLogUpdClauseErase = PtoPredAdjust(PredLogUpdClauseErase);
  PredLogUpdClause0 = PtoPredAdjust(PredLogUpdClause0);
  PredStaticClause = PtoPredAdjust(PredStaticClause);
  PredThrow = PtoPredAdjust(PredThrow);
  PredHandleThrow = PtoPredAdjust(PredHandleThrow);
  PredIs = PtoPredAdjust(PredIs);
  PredSafeCallCleanup = PtoPredAdjust(PredSafeCallCleanup);
  PredRestoreRegs = PtoPredAdjust(PredRestoreRegs);
#ifdef YAPOR
  PredGetwork = PtoPredAdjust(PredGetwork);
  PredGetworkSeq = PtoPredAdjust(PredGetworkSeq);
#endif /* YAPOR */

#ifdef LOW_LEVEL_TRACER

#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(Yap_low_level_trace_lock);
#endif
#endif












  DUMMYCODE->opc = Yap_opcode(_op_fail);
  FAILCODE->opc = Yap_opcode(_op_fail);
  NOCODE->opc = Yap_opcode(_Nstop);
  RestoreEnvInst(ENV_FOR_TRUSTFAIL,&TRUSTFAILCODE,_trust_fail,PredFail);

  RestoreEnvInst(ENV_FOR_YESCODE,&YESCODE,_Ystop,PredFail);

  RestoreOtaplInst(RTRYCODE,_retry_and_mark,PredFail);
#ifdef BEAM
  BEAM_RETRY_CODE->opc = Yap_opcode(_beam_retry_code);
#endif /* BEAM */
#ifdef YAPOR
  RestoreOtaplInst(GETWORK,_getwork,PredGetwork);
  RestoreOtaplInst(GETWORK_SEQ,_getwork_seq,PredGetworkSeq);
  GETWORK_FIRST_TIME->opc = Yap_opcode(_getwork_first_time);
#endif /* YAPOR */
#ifdef TABLING
  RestoreOtaplInst(LOAD_ANSWER,_table_load_answer,PredFail);
  RestoreOtaplInst(TRY_ANSWER,_table_try_answer,PredFail);
  RestoreOtaplInst(ANSWER_RESOLUTION,_table_answer_resolution,PredFail);
  RestoreOtaplInst(COMPLETION,_table_completion,PredFail);
#ifdef THREADS_CONSUMER_SHARING
  RestoreOtaplInst(ANSWER_RESOLUTION_COMPLETION,_table_answer_resolution_completion,PredFail);
#endif /* THREADS_CONSUMER_SHARING */
#endif /* TABLING */




  P_before_spy = PtoOpAdjust(P_before_spy);

  RETRY_C_RECORDEDP_CODE = PtoOpAdjust(RETRY_C_RECORDEDP_CODE);
  RETRY_C_RECORDED_K_CODE = PtoOpAdjust(RETRY_C_RECORDED_K_CODE);













#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(DBTermsListLock);
#endif
  RestoreDBTermsList();


  RestoreExpandList();

#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(ExpandClausesListLock);
  REINIT_LOCK(OpListLock);
#endif

#ifdef DEBUG




#endif


  RestoreUdiControlBlocks();




  RestoreIntKeys();
  RestoreIntLUKeys();
  RestoreIntBBKeys();







  RestoreDBErasedMarker();
  RestoreLogDBErasedMarker();

  RestoreDeadStaticClauses();
  RestoreDeadMegaClauses();
  RestoreDeadStaticIndices();
  RestoreDBErasedList();
  RestoreDBErasedIList();
#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(DeadStaticClausesLock);
  REINIT_LOCK(DeadMegaClausesLock);
  REINIT_LOCK(DeadStaticIndicesLock);
#endif
#ifdef COROUTINING




#endif



  OpList = OpListAdjust(OpList);


  RestoreStreams();



  RestoreAliases();

  AtPrompt = AtomAdjust(AtPrompt);


  CharConversionTable = CodeCharPAdjust(CharConversionTable);
  CharConversionTable2 = CodeCharPAdjust(CharConversionTable2);



  Yap_LibDir = CodeCharPAdjust(Yap_LibDir);

  LastWtimePtr = CodeVoidPAdjust(LastWtimePtr);

  RestoreForeignCode();




  RestoreYapRecords();

  RestoreSWIAtoms();



  RestoreSWIBlobTypes();
  RestoreSWIBlobs();


#if defined(YAPOR) || defined(THREADS)
  REINIT_LOCK(SWI_Blobs_Lock);
#endif
