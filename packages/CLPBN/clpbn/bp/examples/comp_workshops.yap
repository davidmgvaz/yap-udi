
:- use_module(library(pfl)).

%:- set_clpbn_flag(solver,ve).
%:- set_clpbn_flag(solver,bp), clpbn_bp:set_horus_flag(inf_alg,ve).
:- set_clpbn_flag(solver,fove).

c(p1,w1).
c(p1,w2).
c(p1,w3).
c(p2,w1).
c(p2,w2).
c(p2,w3).
c(p3,w1).
c(p3,w2).
c(p3,w3).
c(p4,w1).
c(p4,w2).
c(p4,w3).
c(p5,w1).
c(p5,w2).
c(p5,w3).

markov attends(P)::[t,f] , hot(W)::[t,f] ; [0.1, 0.2, 0.3, 0.4] ; [c(P,W)].

markov attends(P)::[t,f], series::[t,f] ; [0.5, 0.6, 0.7, 0.8] ; [c(P,_)].

?- series(X).


