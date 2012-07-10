/*  $Id$

    Part of SWI-Prolog

    Author:        Jan Wielemaker
    E-mail:        J.Wielemaker@uva.nl
    WWW:           http://www.swi-prolog.org
    Copyright (C): 1985-2009, University of Amsterdam

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, if you link this library with other files,
    compiled with a Free Software compiler, to produce an executable, this
    library does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

:- module('$win_menu',
	  [ win_insert_menu_item/4,	% +PopupName, +Item, +Before, :Goal
	    win_has_menu/0		% Test whether we have menus
	  ]).

:- meta_predicate
	win_insert_menu_item(+,+,+,:).
%:- multifile
%	prolog:on_menu/1.
:- dynamic
	menu_action/2.
:- volatile
	menu_action/2.

prolog:on_menu(Label) :-
	menu_action(Label, Action),
	catch(Action, Error,
	      print_message(error, Error)).

%	win_has_menu
%
%	Test whether the system provides the menu interface

prolog:win_has_menu :-
       current_predicate(_, system:'$win_insert_menu_item'(_, _, _)).

%	win_insert_menu_item(+Popup, +Item, +Before, :Goal)
%
%	Add a menu-item to the PLWIN.EXE menu.  See the reference manual
%	for details.

prolog:win_insert_menu_item(Popup, --, Before, _Goal) :- !,
	call(system:'$win_insert_menu_item'(Popup, --, Before)). % fool check/0
prolog:win_insert_menu_item(Popup, Item, Before, Goal) :-
	insert_menu_item(Popup, Item, Before, Goal).

insert_menu_item(Popup, Item, Before, Goal) :-
	(   menu_action(Item, OldGoal),
	    OldGoal \== Goal
	->  throw(error(permission_error(redefine, Item),
			win_insert_menu_item/4))
	;   true
	),
	call(system:'$win_insert_menu_item'(Popup, Item, Before)),
	assert(menu_action(Item, Goal)).
