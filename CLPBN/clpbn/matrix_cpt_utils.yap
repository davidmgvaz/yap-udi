:- module(clpbn_matrix_utils,
	  [init_CPT/2,
	   project_from_CPT/3,
	   reorder_CPT/5,
	   get_CPT_sizes/2,
	   normalise_CPT/2,
	   multiply_CPTs/4,
	   divide_CPTs/3,
	   expand_CPT/4,
	   reset_CPT_that_disagrees/5,
	   unit_CPT/2,
	   sum_out_from_CPT/4,
	   list_from_CPT/2]).

:- use_module(dists,
	      [get_dist_domain_size/2,
	       get_dist_domain/2]).

:- use_module(library(matrix),
	      [matrix_new/4,
	       matrix_new_set/4,
	       matrix_select/4,
	       matrix_dims/2,
	       matrix_shuffle/3,
	       matrix_expand/3,
	       matrix_op/4,
	       matrix_dims/2,
	       matrix_sum/2,
	       matrix_sum_out/3,
	       matrix_sum_out_several/3,
	       matrix_op_to_all/4,
	       matrix_set_all_that_disagree/5,
	       matrix_to_list/2]).

:- use_module(library(lists), [nth0/3]).

init_CPT(List, Sizes, TAB) :-
	matrix_new(floats, Sizes, List, TAB).

project_from_CPT(V,tab(Table,Deps,_),tab(NewTable,NDeps,NSzs)) :-
	evidence(V,Pos), !,
	vnth(Deps, 0, V, N, NDeps),
	matrix_select(Table, N, Pos, NewTable),
	matrix_dims(NewTable, NSzs).
project_from_CPT(V,tab(Table,Deps,_),tab(NewTable,NDeps,NSzs)) :-
	vnth(Deps, 0, V, N, NDeps),
	matrix_sum_out(Table, N, NewTable),
	matrix_dims(NewTable, NSzs).

evidence(V, Pos) :-
	clpbn:get_atts(V, [evidence(Ev),dist(Id,_)]),
	get_dist_domain(Id, Dom),
	nth0(Pos, Dom, Ev).

vnth([V1|Deps], N, V, N, Deps) :-
	V == V1, !.	
vnth([V1|Deps], N0, V, N, [V1|NDeps]) :-
	N1 is N0+1,
	vnth(Deps, N1, V, N, NDeps).

reorder_CPT(Vs0,T0,Vs,TF,Sizes) :-
	var(Vs), !,
	order_vec(Vs0,Vs,Map),
	(
	 Vs == Vs0
	->
	 TF = T0
	;
	 matrix_shuffle(T0,Map,TF)
	),
	matrix_dims(TF, Sizes).
reorder_CPT(Vs0,T0,Vs,TF,Sizes) :-
	mapping(Vs0,Vs,Map),
	(
	 Vs == Vs0
	->
	 TF = T0
	;
	 matrix_shuffle(T0,Map,TF)
	),
	matrix_dims(TF, Sizes).

order_vec(Vs0,Vs,Map) :-
	add_indices(Vs0,0,Is),
	keysort(Is,NIs),
	get_els(NIs, Vs, Map).

add_indices([],_,[]).
add_indices([V|Vs0],I0,[V-I0|Is]) :-
	I is I0+1,
	add_indices(Vs0,I,Is).

get_els([], [], []).
get_els([V-I|NIs], [V|Vs], [I|Map]) :-
	get_els(NIs, Vs, Map).
	
mapping(Vs0,Vs,Map) :-
	add_indices(Vs0,0,I1s),
	add_indices( Vs,I2s),
	keysort(I1s,Ks),
	keysort(I2s,Ks),
	split_map(I2s, Map).

add_indices([],[]).
add_indices([V|Vs0],[V-_|I1s]) :-
	add_indices(Vs0,I1s).

split_map([], []).
split_map([_-M|Is], [M|Map]) :-
	split_map(Is, Map).

divide_CPTs(Tab1, Tab2, OT) :-
	matrix_op(Tab1,Tab2,/,OT).


multiply_CPTs(tab(Tab1, Deps1, Sz1), tab(Tab2, Deps2, Sz2), tab(OT, NDeps, NSz), NTab2) :-
	expand_tabs(Deps1, Sz1, Deps2, Sz2, Map1, Map2, NDeps),
	matrix_expand(Tab1, Map1, NTab1),
	matrix_expand(Tab2, Map2, NTab2),
	matrix_op(NTab1,NTab2,*,OT),
	matrix_dims(OT,NSz).

expand_tabs([], [], [], [], [], [], []).
expand_tabs([V1|Deps1], [S1|Sz1], [], [], [0|Map1], [S1|Map2], [V1|NDeps]) :-
	expand_tabs(Deps1, Sz1, [], [], Map1, Map2, NDeps).
expand_tabs([], [], [V2|Deps2], [S2|Sz2], [S2|Map1], [0|Map2], [V2|NDeps]) :-
	expand_tabs([], [], Deps2, Sz2, Map1, Map2, NDeps).
expand_tabs([V1|Deps1], [S1|Sz1], [V2|Deps2], [S2|Sz2], Map1, Map2, NDeps) :-
	compare(C,V1,V2),
	(C == = ->
	 NDeps = [V1|MDeps],
	 Map1 = [0|M1],
	 Map2 = [0|M2],
	 NDeps = [V1|MDeps],
	 expand_tabs(Deps1, Sz1, Deps2, Sz2, M1, M2, MDeps)
	;
	 C == < ->
	 NDeps = [V1|MDeps],
	 Map1 = [0|M1],
	 Map2 = [S1|M2],
	 NDeps = [V1|MDeps],
	 expand_tabs(Deps1, Sz1, [V2|Deps2], [S2|Sz2], M1, M2, MDeps)
	;
	 NDeps = [V2|MDeps],
	 Map1 = [S2|M1],
	 Map2 = [0|M2],
	 NDeps = [V2|MDeps],
	 expand_tabs([V1|Deps1], [S1|Sz1], Deps2, Sz2, M1, M2, MDeps)
	).
	
normalise_CPT(MAT,NMAT) :-
	matrix_sum(MAT, Sum),
	matrix_op_to_all(MAT,/,Sum,NMAT).

list_from_CPT(MAT, List) :-
	matrix_to_list(MAT, List).

expand_CPT(MAT0, Dims0, DimsNew, MAT) :-
	generate_map(DimsNew, Dims0, Map),
	matrix_expand(MAT0, Map, MAT).

generate_map([], [], []).
generate_map([V|DimsNew], [V0|Dims0], [0|Map]) :- V == V0, !,
	generate_map(DimsNew, Dims0, Map).
generate_map([V|DimsNew], Dims0, [Sz|Map]) :-
	clpbn:get_atts(V, [dist(Id,_)]),
	get_dist_domain_size(Id, Sz),	
	generate_map(DimsNew, Dims0, Map).
	
unit_CPT(V,CPT) :-
	clpbn:get_atts(V, [dist(Id,_)]),
	get_dist_domain_size(Id, Sz),
	matrix_new_set(floats,[Sz],1.0,CPT).

reset_CPT_that_disagrees(CPT, Vars, V, Pos, NCPT) :-
	vnth(Vars, 0, V, Dim,  _),
	matrix_set_all_that_disagree(CPT, Dim, Pos, 0.0, NCPT).

sum_out_from_CPT(Vs,Table,Deps,tab(NewTable,Vs,Sz)) :-
	conversion_matrix(Vs, Deps, Conv),
	matrix_sum_out_several(Table, Conv, NewTable),
	matrix_dims(NewTable, Sz).

conversion_matrix([], [], []).
conversion_matrix([], [_|Deps], [1|Conv]) :-
	conversion_matrix([], Deps, Conv).
conversion_matrix([V|Vs], [V1|Deps], [0|Conv]) :- V==V1, !,
	conversion_matrix(Vs, Deps, Conv).
conversion_matrix([V|Vs], [_|Deps], [1|Conv]) :-
	conversion_matrix([V|Vs], Deps, Conv).

get_CPT_sizes(CPT, Sizes) :-
	matrix_dims(CPT, Sizes).