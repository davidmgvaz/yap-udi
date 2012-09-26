
:- use_module(library(pfl)).

/* base file for school database. Supposed to be called from school_*.yap */

%
% bayes is a parfactor for a bayesian network,
% first argument is target of other arguments pop(K) <- abi(K)
% second argument is the name of a predicate to call for \phi (CPT)
% last argument is a list of goals defining the constraints over the elements
% of the 
%

%
% these states that skolem variables abi(K) are in a parametric factor with
% with \phi defined by abi_table(X) and whose domain and constraints
% is obtained from professor/1.
%

bayes abi(K)::[h,m,l] ; abi_table ; [professor(K)].

bayes pop(K)::[h,m,l], abi(K) ; pop_table ; [professor(K)].

bayes diff(C) :: [h,m,l] ; diff_table ; [course(C,_)].

bayes int(S) :: [h,m,l] ; int_table ; [student(S)].

bayes grade(C,S)::[a,b,c,d], int(S), diff(C) ; grade_table ; [registration(_,C,S)].

bayes satisfaction(C,S)::[h,m,l], abi(P), grade(C,S) ; sat_table ; [reg_satisfaction(C,S,P)].

bayes rat(C) :: [h,m,l], Sats ; avg ; [course_rat(C, Sats)].

bayes rank(S) :: [a,b,c,d], Grades ; avg ; [student_ranking(S,Grades)].


grade(Key, Grade) :-
	registration(Key, CKey, SKey),
	grade(CKey, SKey, Grade).

reg_satisfaction(CKey, SKey, PKey) :-
	registration(_Key, CKey, SKey),
	course(CKey, PKey).

course_rat(CKey, Sats) :-
	course(CKey,  _),
	setof(satisfaction(CKey,SKey),
	   PKey^reg_satisfaction(CKey, SKey, PKey),
          Sats).

student_ranking(SKey, Grades) :-
	student(SKey),
	setof(grade(CKey,SKey), RKey^registration(RKey,CKey,SKey), Grades).

:- ensure_loaded(tables).

% convert to longer names 
professor_ability(P,A) :- abi(P, A).

professor_popularity(P,A) :- pop(P, A).

registration_grade(R,A) :-
	registration(R,C,S),
	grade(C,S,A).

registration_satisfaction(R,A) :-
	registration(R,C,S),
	satisfaction(C,S,A).

student_intelligence(P,A) :- int(P, A).

course_difficulty(P,A) :- diff(P, A).


registration_course(R,C) :-
	registration(R, C, _).

registration_student(R,S) :-
	registration(R, _, S).

course_rating(C,X) :- rat(C,X).

%
% evidence
%
%abi(p0, h).

%pop(p1, m).
%pop(p2, h).

% Query
% ?- abi(p0, X).

