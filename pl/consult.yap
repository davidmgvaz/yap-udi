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
* File:		consult.yap						 *
* Last rev:	8/2/88							 *
* mods:									 *
* comments:	Consulting Files in YAP					 *
*									 *
*************************************************************************/

%
% SWI options
% autoload(true,false)
% derived_from(File) -> make
% encoding(Encoding) => implemented
% expand({true,false)
% if(changed,true,not_loaded) => implemented
% imports(all,List) => implemented
% qcompile(true,false)
% silent(true,false)  => implemented
% stream(Stream)  => implemented
% consult(consult,reconsult) => implemented
% compilation_mode(compact,source,assert_all) => implemented
%
load_files(Files,Opts) :-
	'$load_files'(Files,Opts,load_files(Files,Opts)).

'$load_files'(Files,Opts,Call) :-
	'$check_files'(Files,load_files(Files,Opts)),
	'$process_lf_opts'(Opts,Silent,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,Files,Call),
	'$check_use_module'(Call,UseModule),
        '$current_module'(M0),
	'$lf'(Files,M0,Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule),
	'$close_lf'(Silent).

'$check_files'(Files,Call) :-
	var(Files), !,
	'$do_error'(instantiation_error,Call).
'$check_files'(M:Files,Call) :- !,
	(var(M)
	->
	'$do_error'(instantiation_error,Call)
	;
	 atom(M)
	->
	 '$check_files'(Files,Call)
	;
	'$do_error'(type_error(atom,M),Call)
	).
'$check_files'(Files,Call) :-
	(ground(Files)
	->
	 true
	;
	'$do_error'(instantiation_error,Call)
	).
	 

'$process_lf_opts'(V,_,_,_,_,_,_,_,_,_,_,_,_,Call) :-
	var(V), !,
	'$do_error'(instantiation_error,Call).
'$process_lf_opts'([],_,InfLevel,_,_,_,_,_,_,_,_,_,_,_).
'$process_lf_opts'([Opt|Opts],Silent,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,Files,Call) :-
	'$process_lf_opt'(Opt,Silent,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,Files,Call), !, 
	'$process_lf_opts'(Opts,Silent,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,Files,Call).
'$process_lf_opts'([Opt|_],_,_,_,_,_,_,_,_,_,_,_,_,Call) :-
	'$do_error'(domain_error(unimplemented_option,Opt),Call).

'$process_lf_opt'(autoload(true),Silent,InfLevel,_,_,_,_,_,_,_,_,_,_,_) :-
	get_value('$verbose_auto_load',VAL),
	(VAL = true ->
	    InfLevel = informational,
	    (get_value('$lf_verbose',informational) -> true ;  Silent = silent),
	    set_value('$lf_verbose',informational)
	;
	    InfLevel = silent,
	    (get_value('$lf_verbose',silent) -> true ;  Silent = informational),
	    set_value('$lf_verbose',silent)
	).
'$process_lf_opt'(autoload(false),_,_,_,_,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(derived_from(File),_,_,_,_,_,_,_,_,_,_,_,Files,Call) :-
	( atom(File) -> true ;  '$do_error'(type_error(atom,File),Call) ),
	( atom(Files) -> true ;  '$do_error'(type_error(atom,Files),Call) ),
	/* call make */
	'$do_error'(domain_error(unimplemented_option,derived_from),Call).
'$process_lf_opt'(encoding(Encoding),_,_,_,_,_,_,_,_,Encoding,_,_,_,Call) :-
	atom(Encoding).
'$process_lf_opt'(expand(true),_,_,true,_,_,_,_,_,_,_,_,_,Call) :-
	'$do_error'(domain_error(unimplemented_option,expand),Call).
'$process_lf_opt'(expand(false),_,_,false,_,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(if(changed),_,_,_,changed,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(if(true),_,_,_,true,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(if(not_loaded),_,_,_,not_loaded,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(imports(all),_,_,_,_,_,_,_,_,_,_,_,_,_).
'$process_lf_opt'(imports(Imports),_,_,_,_,_,Imports,_,_,_,_,_,_,_).
'$process_lf_opt'(qcompile(true),_,_,_,_,true,_,_,_,_,_,_,_,Call) :-
	'$do_error'(domain_error(unimplemented_option,qcompile),Call).
'$process_lf_opt'(qcompile(false),_,_,_,_,false,_,_,_,_,_,_,_,_).
'$process_lf_opt'(silent(true),Silent,silent,_,_,_,_,_,_,_,_,_,_,_) :-
	( get_value('$lf_verbose',silent) -> true ;  Silent = informational),
	set_value('$lf_verbose',silent).
'$process_lf_opt'(silent(false),Silent,informational,_,_,_,_,_,_,_,_,_,_,_) :-
	( get_value('$lf_verbose',informational) -> true ;  Silent = silent),
	set_value('$lf_verbose',informational).
'$process_lf_opt'(skip_unix_comments,_,_,_,_,_,_,_,_,skip_unix_comments,_,_,_,_).
'$process_lf_opt'(compilation_mode(source),_,_,_,_,_,_,_,_,_,source,_,_,_).
'$process_lf_opt'(compilation_mode(compact),_,_,_,_,_,_,_,_,_,compact,_,_,_).
'$process_lf_opt'(compilation_mode(assert_all),_,_,_,_,_,_,_,_,_,assert_all,_,_,_).
'$process_lf_opt'(consult(reconsult),_,_,_,_,_,_,_,_,_,_,reconsult,_,_).
'$process_lf_opt'(consult(consult),_,_,_,_,_,_,_,_,_,_,consult,_,_).
'$process_lf_opt'(stream(Stream),_,_,_,_,_,_,Stream,_,_,_,_,Files,Call) :-
/*	( is_stream(Stream) -> true ;  '$do_error'(domain_error(stream,Stream),Call) ), */
	( atom(Files) -> true ;  '$do_error'(type_error(atom,Files),Call) ).

'$check_use_module'(use_module(_),use_module(_)) :- !.
'$check_use_module'(use_module(_,_),use_module(_)) :- !.
'$check_use_module'(use_module(M,_,_),use_module(M)) :- !.
'$check_use_module'(_,load_files) :- !.

'$lf'(V,_,Call,_,_,_,_,_,_,_,_,_,_) :- var(V), !,
	'$do_error'(instantiation_error,Call).
'$lf'([],_,_,_,_,_,_,_,_,_,_,_,_,_) :- !.
'$lf'(M:X, _, Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule) :- !,
	(
	  atom(M)
	->
	  '$lf'(X, M, Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule)
	  ;
	  '$do_error'(type_error(atom,M),Call)
	).
'$lf'([F|Fs], Mod,Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule) :- !,
	'$lf'(F,Mod,Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,_),
	'$lf'(Fs, Mod,Call,InfLevel,Expand,Changed,CompilationMode,Imports,Stream,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule).
'$lf'(_, Mod, _,InfLevel,_,_,CompilationMode,Imports,Stream,_,Reconsult,SkipUnixComments,CompMode,UseModule) :-
        nonvar(Stream), !,
	'$do_lf'(Mod, Stream, InfLevel,CompilationMode,Imports,SkipUnixComments,CompMode,Reconsult,UseModule).
'$lf'(user, Mod, _,InfLevel,_,_,CompilationMode,Imports,_,_,SkipUnixComments,CompMode,Reconsult,UseModule) :- !,
	'$do_lf'(Mod, user_input, InfLevel, CompilationMode,Imports,SkipUnixComments,CompMode,Reconsult,UseModule).
'$lf'(user_input, Mod, _,InfLevel,_,_,CompilationMode,Imports,_,_,SkipUnixComments,CompMode,Reconsult,UseModule) :- !,
	'$do_lf'(Mod, user_input, InfLevel, CompilationMode,Imports,SkipUnixComments,CompMode,Reconsult,UseModule).
'$lf'(X, Mod, Call, InfLevel,_,Changed,CompilationMode,Imports,_,Encoding,SkipUnixComments,CompMode,Reconsult,UseModule) :-
	'$full_filename'(X, Y, Call),
	(
	  var(Encoding)
	 ->
	  Opts = []
        ;
	  Opts = [encoding(Encoding)]
        ),
	open(Y, read, Stream, Opts), !,
	'$set_changed_lfmode'(Changed),
	'$start_lf'(X, Mod, Stream, InfLevel, CompilationMode, Imports, Changed,SkipUnixComments,CompMode,Reconsult,UseModule),
	close(Stream).
'$lf'(X, _, Call, _, _, _, _, _, _, _, _, _, _, _) :-
	'$do_error'(permission_error(input,stream,X),Call).

'$set_changed_lfmode'(true) :- !.
'$set_changed_lfmode'(_).

'$start_lf'(_, Mod, Stream,_ ,_, Imports, not_loaded, _, _, _, _) :-
	'$file_loaded'(Stream, Mod, Imports), !.
'$start_lf'(_, Mod, Stream, _, _, Imports, changed, _, _, _, _) :-
	'$file_unchanged'(Stream, Mod, Imports), !.
'$start_lf'(_, Mod, Stream, InfLevel, CompilationMode, Imports, _, SkipUnixComments, CompMode, Reconsult, UseModule) :-
	'$do_lf'(Mod, Stream, InfLevel, CompilationMode, Imports, SkipUnixComments, CompMode, Reconsult, UseModule).

'$close_lf'(Silent) :- 
	nonvar(Silent), !,
	set_value('$lf_verbose',Silent).
'$close_lf'(_).

ensure_loaded(Fs) :-
	'$load_files'(Fs, [if(changed)],ensure_loaded(Fs)).

compile(Fs) :-
	'$load_files'(Fs, [], compile(Fs)).

% consult(Fs) :-
% 	'$has_yap_or',
% 	'$do_error'(context_error(consult(Fs),clause),query).
consult(V) :-
	var(V), !,
	'$do_error'(instantiation_error,consult(V)).
consult(M0:Fs) :- !,
	'$consult'(Fs, M0).
consult(Fs) :-
	'$current_module'(M0),
	'$consult'(Fs, M0).

'$consult'(Fs,Module) :-
	'$access_yap_flags'(8, 2), % SICStus Prolog compatibility
	!,
	'$load_files'(Module:Fs,[],consult(Fs)).
'$consult'(Fs, Module) :-
	'$load_files'(Module:Fs,[consult(consult)],consult(Fs)).

reconsult(Fs) :-
	'$load_files'(Fs, [], reconsult(Fs)).

use_module(F) :-
	'$load_files'(F, [if(not_loaded)], use_module(F)).

use_module(F,Is) :-
	'$load_files'(F, [if(not_loaded),imports(Is)], use_module(F,Is)).

use_module(M,F,Is) :-
	'$use_module'(M,F,Is).

'$use_module'(U,_F,Is) :- nonvar(U), U = user, !,
	'$import_to_current_module'(user_input, user, Is).
'$use_module'(M,F,Is) :- nonvar(M), !,
	recorded('$module','$module'(F1,M,_),_),
	'$load_files'(F1, [if(not_loaded),imports(Is)], use_module(M,F,Is)),
	F1 = F.
'$use_module'(M,F,Is) :- nonvar(F),
	'$load_files'(F, [if(not_loaded),imports(Is)], use_module(M,F,Is)).

'$csult'(V, _) :- var(V), !,
	'$do_error'(instantiation_error,consult(V)).
'$csult'([], _).
'$csult'([-F|L], M) :- !, '$load_files'(M:F, [],[-M:F]), '$csult'(L, M).
'$csult'([F|L], M) :- '$consult'(F, M), '$csult'(L, M).

'$do_lf'(ContextModule, Stream, InfLevel, _, Imports, SkipUnixComments, CompMode, Reconsult, UseModule) :-
	'$reset_if'(OldIfLevel),
	'$into_system_mode'(OldMode),
	'$record_loaded'(Stream, ContextModule, Reconsult),
	'$current_module'(OldModule,ContextModule),
	working_directory(OldD,OldD),
	'$ensure_consulting_file'(OldF, Stream),
	H0 is heapused, '$cputime'(T0,_),
	'$file_name'(Stream,File),
	'$set_current_loop_stream'(OldStream, Stream),
	'$ensure_consulting'(Old, false),
	'$access_yap_flags'(18,GenerateDebug),
	'$consult_infolevel'(InfLevel),
	'$comp_mode'(OldCompMode, CompMode),
	( get_value('$syntaxcheckflag',on) -> '$init_style_check'(File) ; true ),
	recorda('$initialisation','$',_),
	( Reconsult = reconsult ->
	    '$start_reconsulting'(File),
	    '$start_consult'(Reconsult,File,LC),
	    '$remove_multifile_clauses'(File),
	    StartMsg = reconsulting,
	    EndMsg = reconsulted
	    ;
	    '$start_consult'(Reconsult,File,LC),
	    StartMsg = consulting,
	    EndMsg = consulted
	),
	print_message(InfLevel, loading(StartMsg, File)),
	( SkipUnixComments == skip_unix_comments ->
	    '$skip_unix_comments'(Stream)
	;
	    true
	),
	'$loop'(Stream,Reconsult),
	H is heapused-H0, '$cputime'(TF,_), T is TF-T0,
	'$current_module'(Mod,OldModule),
	print_message(InfLevel, loaded(EndMsg, File, Mod, T, H)),
	'$end_consult',
	( 
	    Reconsult = reconsult ->
	    '$clear_reconsulting'
	;
	    true
	),
	'$set_current_loop_stream'(_, OldStream),
	'$set_yap_flags'(18,GenerateDebug),
	'$comp_mode'(CompMode, OldCompMode),
	nb_setval('$consulting',Old),
	nb_setval('$consulting_file',OldF),
	working_directory(_,OldD),
	% surely, we were in run mode or we would not have included the file!
	nb_setval('$if_skip_mode',run),
	% back to include mode!
	nb_setval('$if_level',OldIfLevel),
	'$bind_module'(Mod, UseModule),
	'$import_to_current_module'(File, ContextModule, Imports),
	( LC == 0 -> prompt(_,'   |: ') ; true),
        ( OldMode == off -> '$exit_system_mode' ; true ),
	'$exec_initialisation_goals',
	!.

'$reset_if'(OldIfLevel) :-
	'$nb_getval'('$if_level', OldIfLevel, fail), !,
	nb_setval('$if_level',0).
'$reset_if'(0) :-
	nb_setval('$if_level',0).

'$get_if'(Level0) :-
	'$nb_getval'('$if_level', Level, fail), !,
	Level0 = Level.
'$get_if'(0).

'$into_system_mode'(OldMode) :-
	( '$nb_getval'('$system_mode', OldMode, fail) -> true ; OldMode = off),
        ( OldMode == off -> '$enter_system_mode' ; true ).

'$ensure_consulting_file'(OldF, Stream) :-
	( '$nb_getval'('$consulting_file',OldF, fail) -> true ; OldF = []),
	'$set_consulting_file'(Stream).

'$ensure_consulting'(Old, New) :-
	( '$nb_getval'('$consulting',Old, fail) -> true ; Old = false ),
	nb_setval('$consulting', New).

'$bind_module'(_, load_files).
'$bind_module'(Mod, use_module(Mod)).

'$import_to_current_module'(File, M, Imports) :-
	recorded('$module','$module'(File,NM,Ps),_), M \= NM, !,
	'$use_preds'(Imports, Ps, NM, M).
'$import_to_current_module'(_, _, _).

'$consult_infolevel'(InfoLevel) :- nonvar(InfoLevel), !.
'$consult_infolevel'(InfoLevel) :-
	get_value('$lf_verbose',InfoLevel), InfoLevel \= [], !.
'$consult_infolevel'(informational).

'$start_reconsulting'(F) :-
	recorda('$reconsulted','$',_),
	recorda('$reconsulting',F,_).

'$initialization'(V) :-
	var(V), !,
	'$do_error'(instantiation_error,initialization(V)).
'$initialization'(C) :- number(C), !,
	'$do_error'(type_error(callable,C),initialization(C)).
'$initialization'(C) :- db_reference(C), !,
	'$do_error'(type_error(callable,C),initialization(C)).
'$initialization'(G) :-
	'$show_consult_level'(Level1),
	% it will be done after we leave the current consult level.
	Level is Level1-1,
	recordz('$initialisation',do(Level,G),_),
	fail.
'$initialization'(_).

initialization(G,OPT) :-
	'$initialization'(G,OPT).

'$initialization'(G,OPT) :-
	( 
	   var(G)
	->
	  '$do_error'(instantiation_error,initialization(G,OPT))
	;
	   number(G)
	->
	  '$do_error'(type_error(callable,G),initialization(G,OPT))
	;
	   db_reference(G)
	->
	  '$do_error'(type_error(callable,G),initialization(G,OPT))
	;
	   var(OPT)
	->
	  '$do_error'(instantiation_error,initialization(G,OPT))
	;
	  atom(OPT)
	->
	  (
	   OPT == now
	  ->
	   fail
	  ;
	   OPT == after_load
	  ->
	   fail
	  ;
	   OPT == restore
	  ->
	   fail
	  ;
	   '$do_error'(domain_error(initialization,OPT),initialization(OPT))
	  )
	;
	  '$do_error'(type_error(OPT),initialization(G,OPT))
	).
'$initialization'(G,now) :-
	( '$notrace'(G) -> true ; format(user_error,':- ~w:~w failed.~n',[M,G]) ).
'$initialization'(G,after_load) :-
	'$initialization'(G).
% ignore for now.
'$initialization'(G,restore).

'$exec_initialisation_goals' :-
	nb_setval('$initialization_goals',on),
	fail.
'$exec_initialisation_goals' :-
	recorded('$blocking_code',_,R),
	erase(R),
	fail.
% system goals must be performed first 
'$exec_initialisation_goals' :-
	recorded('$system_initialisation',G,R),
	erase(R),
	G \= '$',
	'$notrace'(G),
	fail.
'$exec_initialisation_goals' :-
	'$show_consult_level'(Level),
	'$current_module'(M),
	recorded('$initialisation',do(Level,_),_),
	findall(G,
	        '$fetch_init_goal'(Level, G),
		LGs),
	lists:member(G,LGs),
	'$nb_getval'('$system_mode', OldMode, fail),
        ( OldMode == on -> '$exit_system_mode' ; true ),
	% run initialization under user control (so allow debugging this stuff).
	(
	  '$system_catch'('$oncenotrace'(M:G), M, Error, user:'$LoopError'(Error, top)),
	  fail
	;
          OldMode = on,
	  '$enter_system_mode',
	  fail
	).
'$exec_initialisation_goals' :-
	nb_setval('$initialization_goals',off).


'$fetch_init_goal'(Level, G) :-
	recorded('$initialisation',do(Level,G),R),
	erase(R),
	G\='$'.

'$include'(V, _) :- var(V), !,
	'$do_error'(instantiation_error,include(V)).
'$include'([], _) :- !.
'$include'([F|Fs], Status) :- !,
	'$include'(F, Status),
	'$include'(Fs, Status).
'$include'(X, Status) :-
	get_value('$lf_verbose',Verbosity),
	'$full_filename'(X,Y,include(X)),
	( '$nb_getval'('$included_file', OY, fail ) -> true ; OY = [] ),
	nb_setval('$included_file', Y),
	'$current_module'(Mod),
	H0 is heapused, '$cputime'(T0,_),
	( open(Y, read, Stream), !,
	  print_message(Verbosity, loading(including, Y)),
	  '$loop'(Stream,Status), close(Stream)
	;
	  '$do_error'(permission_error(input,stream,Y),include(X))
	),
	H is heapused-H0, '$cputime'(TF,_), T is TF-T0,
	print_message(Verbosity, loaded(included, Y, Mod, T, H)),
	nb_setval('$included_file',OY).

'$do_startup_reconsult'(X) :-
	( '$access_yap_flags'(15, 0) ->
	  '$system_catch'(load_files(X, [silent(true)]),Module, Error, '$Error'(Error))
	;
	  set_value('$verbose',off),
	  '$system_catch'(load_files(X, [silent(true),skip_unix_comments]),Module,_,fail)
	;
	  true
	),
	!,
	( '$access_yap_flags'(15, 0) -> true ; halt).
'$do_startup_reconsult'(_).

'$skip_unix_comments'(Stream) :-
	peek_code(Stream, 0'#), !, % 35 is ASCII for '#
	skip(Stream, 10),
	'$skip_unix_comments'(Stream).
'$skip_unix_comments'(_).


source_file(FileName) :-
	recorded('$lf_loaded','$lf_loaded'(FileName,Mod,_,_),_), Mod \= prolog.

source_file(Mod:Pred, FileName) :-
	current_module(Mod),
	Mod \= prolog,
	'$current_predicate_no_modules'(Mod,_,Pred),
	'$owned_by'(Pred, Mod, FileName).

'$owned_by'(T, Mod, FileName) :-
	'$is_multifile'(T, Mod),
	functor(T, Name, Arity),
	setof(FileName, Ref^recorded('$multifile_defs','$defined'(FileName,Name,Arity,Mod), Ref), L),
	lists:member(FileName, L).
'$owned_by'(T, Mod, FileName) :-
	'$owner_file'(T, Mod, FileName).

prolog_load_context(_, _) :-
	'$nb_getval'('$consulting_file', [], fail), !, fail.
prolog_load_context(directory, DirName) :- 
	getcwd(DirName).
prolog_load_context(file, FileName) :- 
	( '$nb_getval'('$included_file', IncFileName, fail ) -> true ; IncFileName = [] ),
	( IncFileName = [] ->
	  '$nb_getval'('$consulting_file', FileName, fail),
	  FileName \= []
        ;
           FileName  = IncFileName
        ).
prolog_load_context(module, X) :-
	'$current_module'(X).
prolog_load_context(source, FileName) :-
	nb_getval('$consulting_file',FileName).
prolog_load_context(stream, Stream) :- 
	'$current_loop_stream'(Stream).
% return this term for SWI compatibility.
prolog_load_context(term_position, '$stream_position'(0,Line,0,0,0)) :- 
	'$current_loop_stream'(Stream),
	stream_property(Stream, position(Position)),
	stream_position_data(line_count, Position, Line).


% if the file exports a module, then we can
% be imported from any module.
'$file_loaded'(Stream, M, Imports) :-
	'$file_name'(Stream, F),
	'$ensure_file_loaded'(F, M, Imports).

'$ensure_file_loaded'(F, M, Imports) :-
	recorded('$module','$module'(F1,NM,P),_),
	recorded('$lf_loaded','$lf_loaded'(F1,_,_,_),_),
	same_file(F1,F), !,
	'$use_preds'(Imports,P, NM, M).
'$ensure_file_loaded'(F, M, _) :-
	recorded('$lf_loaded','$lf_loaded'(F1,M,_,_),_),
	same_file(F1,F), !.
	

% if the file exports a module, then we can
% be imported from any module.
'$file_unchanged'(Stream, M, Imports) :-
	'$file_name'(Stream, F),
	'$ensure_file_unchanged'(F, M, Imports).

'$ensure_file_unchanged'(F, M, Imports) :-
	recorded('$module','$module'(F1,NM,P),_),
	recorded('$lf_loaded','$lf_loaded'(F1,_,Age,_),R),
	same_file(F1,F), !,
	'$file_is_unchanged'(F, R, Age),
	'$use_preds'(Imports, P, NM, M).
'$ensure_file_unchanged'(F, M, _) :-
	recorded('$lf_loaded','$lf_loaded'(F1,M,Age,_),R),
	same_file(F1,F), !,
	'$file_is_unchanged'(F, R, Age).

'$file_is_unchanged'(F, R, Age) :-
        time_file(F,CurrentAge),
         ((CurrentAge = Age ; Age = -1)  -> true; erase(R), fail).



path(Path) :- findall(X,'$in_path'(X),Path).

'$in_path'(X) :- recorded('$path',Path,_),
		atom_codes(Path,S),
		( S = ""  -> X = '.' ;
		  atom_codes(X,S) ).

add_to_path(New) :- add_to_path(New,last).

add_to_path(New,Pos) :-
	atom(New), !,
	'$check_path'(New,Str),
	atom_codes(Path,Str),
	'$add_to_path'(Path,Pos).

'$add_to_path'(New,_) :- recorded('$path',New,R), erase(R), fail.
'$add_to_path'(New,last) :- !, recordz('$path',New,_).
'$add_to_path'(New,first) :- recorda('$path',New,_).

remove_from_path(New) :- '$check_path'(New,Path),
			recorded('$path',Path,R), erase(R).

'$check_path'(At,SAt) :- atom(At), !, atom_codes(At,S), '$check_path'(S,SAt).
'$check_path'([],[]).
'$check_path'([Ch],[Ch]) :- '$dir_separator'(Ch), !.
'$check_path'([Ch],[Ch,A]) :- !, integer(Ch), '$dir_separator'(A).
'$check_path'([N|S],[N|SN]) :- integer(N), '$check_path'(S,SN).

% add_multifile_predicate when we start consult
'$add_multifile'(Name,Arity,Module) :-
	nb_getval('$consulting_file',File),
	'$add_multifile'(File,Name,Arity,Module).

'$add_multifile'(File,Name,Arity,Module) :-
	recorded('$multifile_defs','$defined'(File,Name,Arity,Module), _), !.
%	print_message(warning,declaration((multifile Module:Name/Arity),ignored)).
'$add_multifile'(File,Name,Arity,Module) :-
	recordz('$multifile_defs','$defined'(File,Name,Arity,Module),_), !,
	fail.
'$add_multifile'(File,Name,Arity,Module) :-
	recorded('$mf','$mf_clause'(File,Name,Arity,Module,Ref),R),
	erase(R),
	'$erase_clause'(Ref,Module),
	fail.
'$add_multifile'(_,_,_,_).

% retract old multifile clauses for current file.
'$remove_multifile_clauses'(FileName) :-
	recorded('$multifile_defs','$defined'(FileName,_,_,_),R1),
	erase(R1),
	fail.
'$remove_multifile_clauses'(FileName) :-
	recorded('$mf','$mf_clause'(FileName,_,_,Module,Ref),R),
	'$erase_clause'(Ref, Module),
	erase(R),
	fail.
'$remove_multifile_clauses'(_).

'$set_consulting_file'(user) :- !,
	nb_setval('$consulting_file',user_input).
'$set_consulting_file'(user_input) :- !,
	nb_setval('$consulting_file',user_input).
'$set_consulting_file'(Stream) :-
	'$file_name'(Stream,F),
	nb_setval('$consulting_file',F),
	'$set_consulting_dir'(F).

%
% Use directory where file exists
%
'$set_consulting_dir'(F) :-
	file_directory_name(F, Dir),
	working_directory(_, Dir).

'$record_loaded'(Stream, M, Reconsult) :-
	Stream \= user,
	Stream \= user_input,
	'$file_name'(Stream,F),
	( recorded('$lf_loaded','$lf_loaded'(F,M,_,_),R), erase(R), fail ; true ),
	time_file(F,Age),
	recorda('$lf_loaded','$lf_loaded'(F,M,Age,Reconsult),_),
	fail.
'$record_loaded'(_, _, _).

'$set_encoding'(Encoding) :-
	'$current_loop_stream'(Stream),
	( Encoding == default -> true ; set_stream(Stream, encoding(Encoding)) ).

absolute_file_name(V,Out) :- var(V), !,
	'$do_error'(instantiation_error, absolute_file_name(V, Out)).
absolute_file_name(user,user) :- !.
absolute_file_name(File0,File) :-
	'$absolute_file_name'(File0,[access(none),file_type(txt),file_errors(fail),solutions(first)],File,absolute_file_name(File0,File)).

'$full_filename'(F0,F,G) :-
	'$absolute_file_name'(F0,[access(read),file_type(source),file_errors(fail),solutions(first),expand(true)],F,G).

% fix wrong argument order, TrueFileName should be last.
absolute_file_name(File,TrueFileName,Opts) :-
	( var(TrueFileName) -> true ; atom(TrueFileName), TrueFileName \= [] ),
	!,
	absolute_file_name(File,Opts,TrueFileName).
absolute_file_name(File,Opts,TrueFileName) :-
	'$absolute_file_name'(File,Opts,TrueFileName,absolute_file_name(File,Opts,TrueFileName)).
	
'$absolute_file_name'(File,Opts,TrueFileName,G) :- var(File), !,
	'$do_error'(instantiation_error, G).
'$absolute_file_name'(File,Opts,TrueFileName, G) :-
	'$process_fn_opts'(Opts,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G),
	/* our own local findall */
	nb:nb_queue(Ref),
	(
	    '$find_in_path'(File,opts(Extensions,RelTo,Type,Access,FErrors,Expand,Debug),TrueFileName,G),
	    nb:nb_queue_enqueue(Ref, TrueFileName),
	    fail
	; 
	    nb:nb_queue_close(Ref, FileNames, [])
	 ),
	'$absolute_file_names'(Solutions, FileNames, FErrors, TrueFileName, File, G).

'$absolute_file_names'(Solutions, [], error, _, File, G) :- !,
	'$do_error'(existence_error(file,File),G).
'$absolute_file_names'(Solutions, FileNames, _, TrueFileName, _, _) :-
        lists:member(TrueFileName, FileNames),
	(Solutions == first -> ! ; true).
	 

'$process_fn_opts'(V,_,_,_,_,_,_,_,_,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$process_fn_opts'([],[],_,txt,none,error,first,false,false,_) :- !.
'$process_fn_opts'([Opt|Opts],Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$process_fn_opt'(Opt,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions0,RelTo0,Type0,Access0,FErrors0,Solutions0,Expand0,Debug0,G),
	'$process_fn_opts'(Opts,Extensions0,RelTo0,Type0,Access0,FErrors0,Solutions0,Expand0,Debug0,G).
'$process_fn_opts'(Opts,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$do_error'(type_error(list,Opts),G).

'$process_fn_opt'(Opt,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G) :- var(Opt), !,
	'$do_error'(instantiation_error, G).
'$process_fn_opt'(extensions(Extensions),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,_,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$check_fn_extensions'(Extensions,G).
'$process_fn_opt'(relative_to(RelTo),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,_,Type,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$check_atom'(RelTo,G).
'$process_fn_opt'(access(Access),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,_,FErrors,Solutions,Expand,Debug,G) :- !,
	'$check_atom'(Access,G).
'$process_fn_opt'(file_type(Type),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,_,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$check_fn_type'(Type,G).
'$process_fn_opt'(file_errors(FErrors),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,_,Solutions,Expand,Debug,G) :- !,
	'$check_fn_errors'(FErrors,G).
'$process_fn_opt'(solutions(Solutions),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,FErrors,_,Expand,Debug,G) :- !,
	'$check_fn_solutions'(Solutions,G).
'$process_fn_opt'(expand(Expand),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,FErrors,Solutions,_,Debug,G) :- !,
	'$check_true_false'(Expand,G).
'$process_fn_opt'(verbose_file_search(Debug),Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,_,G) :- !,
	'$check_true_false'(Debug,G).
'$process_fn_opt'(Opt,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,Extensions,RelTo,Type,Access,FErrors,Solutions,Expand,Debug,G) :- !,
	'$do_error'(domain_error(file_name_option,Opt),G).	

'$check_fn_extensions'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_fn_extensions'([],_) :- !.
'$check_fn_extensions'([A|L],G) :- !,
	'$check_atom'(A,G),
	'$check_fn_extensions'(L,G).
'$check_fn_extensions'(T,G) :- !,
	'$do_error'(type_error(list,T),G).

'$check_atom'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_atom'(A,G) :- atom(A), !.
'$check_atom'(T,G) :- !,
	'$do_error'(type_error(atom,T),G).
	
'$check_fn_type'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_fn_type'(txt,_) :- !.
'$check_fn_type'(prolog,_) :- !.
'$check_fn_type'(source,_) :- !.
'$check_fn_type'(executable,_) :- !.
'$check_fn_type'(qlf,_) :- !.
'$check_fn_type'(directory,_) :- !.
'$check_fn_type'(T,G) :- atom(T), !,
	'$do_error'(domain_error(file_type,T),G).
'$check_fn_type'(T,G) :- !,
	'$do_error'(type_error(atom,T),G).
	
'$check_fn_errors'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_fn_errors'(fail,_) :- !.
'$check_fn_errors'(error,_) :- !.
'$check_fn_errors'(T,G) :- atom(T), !,
	'$do_error'(domain_error(file_errors,T),G).
'$check_fn_errors'(T,G) :- !,
	'$do_error'(type_error(atom,T),G).
	
'$check_fn_solutions'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_fn_solutions'(first,_) :- !.
'$check_fn_solutions'(all,_) :- !.
'$check_fn_solutions'(T,G) :- atom(T), !,
	'$do_error'(domain_error(solutions,T),G).
'$check_fn_solutions'(T,G) :- !,
	'$do_error'(type_error(atom,T),G).
	
'$check_true_false'(V,G) :- var(V), !,
	'$do_error'(instantiation_error, G).
'$check_true_false'(true,_) :- !.
'$check_true_false'(false,_) :- !.
'$check_true_false'(T,G) :- atom(T), !,
	'$do_error'(domain_error(boolean,T),G).
'$check_true_false'(T,G) :- !,
	'$do_error'(type_error(atom,T),G).
	
% This sequence must be followed:
% user and user_input are special;
% library(F) must check library_directories
% T(F) must check file_search_path
% all must try search in path
'$find_in_path'(user,_,user_input, _) :- !.
'$find_in_path'(user_input,_,user_input, _) :- !.
'$find_in_path'(commons(F0),_,_, _) :-
	% make sure library_directory is open.
	\+ clause(user:commons_directory(_),_),
	'$system_commons_directories'(D),
	assert(user:commons_directory(D)),
	fail.
'$find_in_path'(S, Opts, NewFile, Call) :-
	S =.. [Name,File0],
	'$cat_file_name'(File0,File), !,
	'$dir_separator'(D),
	atom_codes(A,[D]),
	'$extend_path_directory'(Name, A, File, Opts, NewFile, Call).
'$find_in_path'(File0,Opts,NewFile,_) :-
	'$cat_file_name'(File0,File), !,
	'$add_path'(File,PFile),
	'$get_abs_file'(PFile,Opts,AbsFile),
	'$search_in_path'(AbsFile,Opts,NewFile).
'$find_in_path'(File,_,_,Call) :-
	'$do_error'(domain_error(source_sink,File),Call).

% allow paths in File Name
'$cat_file_name'(File0,File) :-
	atom(File0), !,
	File = File0.
'$cat_file_name'(Atoms, File) :-
	'$to_list_of_atoms'(Atoms, List, []),
	atom_concat(List, File).

'$to_list_of_atoms'(V, _, _) :- var(V), !, fail.
'$to_list_of_atoms'(Atom, [Atom|L], L) :- atom(Atom), !.
'$to_list_of_atoms'(Atoms, L1, LF) :-
	Atoms =.. [A,As,Bs],
	atom_codes(A,[D]),
	'$dir_separator'(D),
	'$to_list_of_atoms'(As, L1, [A|L2]),
	'$to_list_of_atoms'(Bs, L2, LF).

'$get_abs_file'(File,opts(_,RelTo,_,_,_,Expand,_),AbsFile) :-
	(
	 nonvar(Relto)
	->
	 '$dir_separator'(D),
	 atom_concat([RelTo, D, File], ActualFile)
	;
	  ActualFile = File
	),
	'$swi_current_prolog_flag'(file_name_variables, OldF),
	'$swi_set_prolog_flag'(file_name_variables, Expand),
	(
	 '$absolute_file_name'(ActualFile,AbsFile)
	-> 
	'$swi_set_prolog_flag'(file_name_variables, OldF)
	;
	'$swi_set_prolog_flag'(file_name_variables, OldF),
	 fail
	).
	 

'$search_in_path'(File,opts(Extensions,_,Type,Access,_,_,_),F) :-
	'$add_extensions'(Extensions, File, F0),
	'$check_file'(F0, Type, Access, F).
'$search_in_path'(File,opts(_,_,Type,Access,_,_,_),F) :-
	'$add_type_extensions'(Type, File, F0),
	'$check_file'(F0, Type, Access, F).

'$check_file'(F, Type, none, F) :- !.
'$check_file'(F0, Type, Access, F0) :-
	access_file(F0, Access),
	(Type == directory
	->
	 exists_directory(F0)
	;
	 true
	).

'$add_extensions'([Ext|_],File,F) :-
	'$mk_sure_true_ext'(Ext,NExt),
	atom_concat([File,NExt],F).
'$add_extensions'([_|Extensions],File,F) :-
	'$add_extensions'(Extensions,File,F).

'$mk_sure_true_ext'(Ext,NExt) :-
	atom_codes(Ext,[C|L]),
	C \= 0'.,
	!,
	atom_codes(NExt,[0'.,C|L]).
'$mk_sure_true_ext'(Ext,Ext).

'$add_type_extensions'(Type,File,F) :-
	( Type == source -> NType = prolog ; NType = Type ),
	user:prolog_file_type(Ext, NType),
	atom_concat([File,'.',Ext],F).
'$add_type_extensions'(_,File,File).

'$add_path'(File,File).
'$add_path'(File,PFile) :-
	recorded('$path',Path,_),
	atom_concat([Path,File],PFile).

'$system_library_directories'(Dir) :-
	getenv('YAPSHAREDIR', Dir).
'$system_library_directories'(Dir) :-
	getenv('YAPCOMMONSDIR', Dir).
'$system_library_directories'(Dir) :-
	get_value(system_library_directory,Dir).
'$system_library_directories'(Dir) :-
	get_value(prolog_commons_directory,Dir).


'$extend_path_directory'(Name, D, File, Opts, NewFile, Call) :-
	'$notrace'(user:file_search_path(Name, Dir)),
	'$extend_pathd'(Dir, D, File, Opts, NewFile, Call).

'$extend_pathd'(Dir, A, File, Opts, NewFile, Call) :-
	atom(Dir), !,
	'$add_file_to_dir'(Dir,A,File,NFile),
	'$find_in_path'(NFile, Opts, NewFile, Goal), !.
'$extend_pathd'(Name, A, File, Opts, OFile, Goal) :-
	nonvar(Name),
	Name =.. [N,P0],
	'$add_file_to_dir'(P0,A,File,NFile),
	NewName =.. [N,NFile],
	'$find_in_path'(NewName, Opts, OFile, Goal).

'$add_file_to_dir'(P0,A,Atoms,NFile) :-
	atom_concat([P0,A,Atoms],NFile).


%
% This is complicated because of embedded ifs.
%
'$if'(_,top) :- !, fail.
'$if'(Goal,_) :-
	'$get_if'(Level0),
	Level is Level0 + 1,
	nb_setval('$if_level',Level),
	nb_getval('$endif',OldEndif),
	nb_getval('$if_skip_mode',Mode),
	nb_setval('$endif',elif(Level,OldEndif,Mode)),
	fail.
% we are in skip mode, ignore....
'$if'(Goal,_) :-
	nb_getval('$endif',elif(Level, OldEndif, skip)), !,
	nb_setval('$endif',endif(Level, OldEndif, skip)).	
% we are in non skip mode, check....
'$if'(Goal,_) :-
	('$if_call'(Goal)
	    ->
	 % we will execute this branch, and later enter skip
	 nb_getval('$endif',elif(Level,OldEndif,Mode)),
	 nb_setval('$endif',endif(Level,OldEndif,Mode))
	;
	 % we are now in skip, but can start an elif.
	 nb_setval('$if_skip_mode',skip)
	).

'$else'(top) :- !, fail.
'$else'(_) :-
	'$get_if'(0), !,
	'$do_error'(context_error(no_if),(:- else)).
% we have done an if, so just skip
'$else'(_) :-
	nb_getval('$endif',endif(_,_,_)), !,
	nb_setval('$if_skip_mode',skip).
% we can try the elif
'$else'(_) :-
	'$get_if'(Level),
	nb_getval('$endif',elif(Level,OldEndif,Mode)),
	nb_setval('$endif',endif(Level,OldEndif,Mode)),
	nb_setval('$if_skip_mode',run).

'$elif'(_,top) :- !, fail.
'$elif'(Goal,_) :-
	'$get_if'(0),
	'$do_error'(context_error(no_if),(:- elif(Goal))).
% we have done an if, so just skip
'$elif'(_,_) :-
	 nb_getval('$endif',endif(_,_,_)), !,
	 nb_setval('$if_skip_mode',skip).
% we can try the elif
'$elif'(Goal,_) :-
	'$get_if'(Level),
	nb_getval('$endif',elif(Level,OldEndif,Mode)),
	('$if_call'(Goal)
	    ->
% we will not skip, and we will not run any more branches.
	 nb_setval('$endif',endif(Level,OldEndif,Mode)),
	 nb_setval('$if_skip_mode',run)
	;
% we will (keep) on skipping
	 nb_setval('$if_skip_mode',skip)
	).
'$elif'(_,_).

'$endif'(top) :- !, fail.
'$endif'(_) :-
% unmmatched endif.
	'$get_if'(0),
	'$do_error'(context_error(no_if),(:- endif)).
'$endif'(_) :-
% back to where you belong.
	'$get_if'(Level),
	nb_getval('$endif',Endif),
	Level0 is Level-1,
	nb_setval('$if_level',Level0),
	arg(2,Endif,OldEndif),
	arg(3,Endif,OldMode),
	nb_setval('$endif',OldEndif),
	nb_setval('$if_skip_mode',OldMode).


'$if_call'(G) :-
	catch('$eval_if'(G), E, (print_message(error, E), fail)).

'$eval_if'(Goal) :-
	expand_term(Goal,TrueGoal),
	once(TrueGoal).

'$if_directive'((:- if(_))).
'$if_directive'((:- else)).
'$if_directive'((:- elif(_))).
'$if_directive'((:- endif)).


'$comp_mode'(_OldCompMode, CompMode) :-
	var(CompMode), !. % just do nothing.
'$comp_mode'(OldCompMode, assert_all) :-
	'$fetch_comp_status'(OldCompMode),
	nb_setval('$assert_all',on).
'$comp_mode'(OldCompMode, source) :-
	'$fetch_comp_status'(OldCompMode),
	'$set_yap_flags'(11,1).
'$comp_mode'(OldCompMode, compact) :-
	'$fetch_comp_status'(OldCompMode),
	'$set_yap_flags'(11,0).

'$fetch_comp_status'(assert_all) :-
	'$nb_getval'('$assert_all',on, fail), !.
'$fetch_comp_status'(source) :-
	 '$access_yap_flags'(11,1).
'$fetch_comp_status'(compact).

make :-
	recorded('$lf_loaded','$lf_loaded'(F1,M,_,reconsult),_),
	'$load_files'(F1, [if(changed)],make),
	fail.
make.

make_library_index(_Directory).

'$file_name'(Stream,F) :-
	stream_property(Stream, file_name(F)), !.
'$file_name'(user_input,user_output).
'$file_name'(user_output,user_ouput).
'$file_name'(user_error,user_error).


'$fetch_stream_alias'(OldStream,Alias) :-
	stream_property(OldStream, alias(Alias)), !.

'$require'(_Ps, _M).

'$store_clause'('$source_location'(File, Line):Clause, File) :-
	assert_static(Clause).


'$set_current_loop_stream'(OldStream, Stream) :-
	'$current_loop_stream'(OldStream), !,
	'$new_loop_stream'(Stream).
'$set_current_loop_stream'(OldStream, Stream) :-
	'$new_loop_stream'(Stream).

'$new_loop_stream'(Stream) :-
	(var(Stream) ->
	    nb_delete('$loop_stream')
	;
	    nb_setval('$loop_stream',Stream)
	).
	    
'$current_loop_stream'(Stream) :-
	'$nb_getval'('$loop_stream',Stream, fail).

exists_source(File) :-
	'$full_filename'(File, AbsFile, exists_source(File)).

