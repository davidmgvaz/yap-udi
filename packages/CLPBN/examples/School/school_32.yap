/*
total_professors(32).

total_courses(64).

total_students(256).

*/

:- source.

:- style_check(all).

:- yap_flag(unknown,error).

:- yap_flag(write_strings,on).

:- use_module(library(clpbn)).

:- [-schema].

:- ensure_loaded(school32_data).

