#include <stack>
#include <math.h>
#include "copyright.h"

#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "colour.h"
#include "log.h"

typedef std::stack <dbref>	Simple_stack;


void
context::do_enter (
const	String& object,
const	String&)

{
	dbref			thing;
	enum	fit_result	fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_neighbor ();
	if ((thing = matcher.noisy_match_result ()) == NOTHING)
		return;;
	if (!Open (thing))
	{
		notify_colour (player,player, COLOUR_MESSAGES, "You don't seem to be able to get in.");
		return;
	}
	if (contains (thing, player))
	{
		notify_colour (player,player, COLOUR_ERROR_MESSAGES, "You cannot enter something you are carrying.");
		return;
	}
	if ((fit_error=will_fit (player, thing)) != SUCCESS)
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You can't fit in there - %s", fit_errlist [fit_error]);
	}
	enter_room (thing);
	Accessed (thing);

	if (!Dark(player))
	{
		sprintf(scratch_buffer, "%s has arrived.", getname_inherited (player));
		notify_except(db[db[player].get_location()].get_contents(), player, player, scratch_buffer);
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_leave (
const	String& ,
const	String&)

{
	dbref			object;
	dbref			loc;
	enum	fit_result	fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if ((object = db[player].get_location()) == NOTHING)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You're not in anything!");
		return;
	}
	if (Typeof (object) != TYPE_THING)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Huh?    (Type \"help\" for help.)");
		return;
	}

	if (!Open (object))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You can't get out.");
		return;
	}
	/* right, check the location of the object isnt a player then */
	/* if it is, get the location of the player (certain to be a good */
	/* location for the player wanting to leave) */

	if ((loc = db[object].get_location()) == NOTHING)
	{
		notify_colour (player, player, COLOUR_MESSAGES,"Your location's not in anything!");
		return;
	}
	else if (Typeof(loc) == TYPE_PLAYER)
		loc = db[loc].get_location();

	if ((fit_error=will_fit(player, db[object].get_location())) != SUCCESS)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You can't get out - %s", fit_errlist [fit_error]);
	}

	enter_room (loc);
	Accessed (loc);

	if (!Dark(player))
	{
		sprintf(scratch_buffer, "%s has arrived.", getname_inherited (player));
		notify_except(db[object].get_contents(), player, player, scratch_buffer);
	}
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	return;
}


/* take out players,rooms and things out of contents list and then put them on the new contents list. */
/* commands are on commands list */
/* variables are on variables list */
/* fuses and alarms are on fuses list */
/* exits are on exits list. */

bool
moveto (
dbref	what,
dbref	where)
{
	/* Make sure the type of the target exists */
	if((where >= 0) && (Typeof(where) == TYPE_FREE))
	{
		return false;	// Location doesn't exist
	}

	dbref	loc = db[what].get_location();
	/* If room moving to itself, move to nothing */
	if ((Typeof (what) == TYPE_ROOM) && (what == where))
	{
		if (loc != NOTHING)
			db[loc].set_contents(remove_first (db[loc].get_contents(), what));
		db[what].set_location(NOTHING);
		db[what].set_next(NOTHING);
		return true;
	}

	/* check restrictions */
	if (contains (where, what) || (what == where))
	{
		if (where == HOME)
		{
			/* put the object in limbo, the poor thing can't even go home! */
			if (loc != NOTHING)
				db[loc].set_contents(remove_first (db[loc].get_contents(), what));
			db[what].set_location(LIMBO);
			PUSH (what, LIMBO, contents);
			return true;
		}
		else
		{
			log_bug("Move to create recursive contents: %s(#%d) -> %s(#%d)", getname(what), what, getname(where), where);
			return false;
		}
	}


	switch (Typeof (what))
	{
		case TYPE_PLAYER:
		case TYPE_THING:
		case TYPE_PUPPET:
			if (where == HOME)
				where = db[what].get_destination();
			if (where == NOTHING)
			{
				log_bug("Attempt to move object %s(#%d) to *NOTHING*", getname(what), what);
				return false;
			}
				/* FALLTHROUGH */
		case TYPE_ROOM:
			if (loc != NOTHING)
				db[loc].set_contents(remove_first (db[loc].get_contents(), what));
			db[what].set_location(where);
			if (where != NOTHING)
				PUSH (what, where, contents)
			else
				db[what].set_next(NOTHING);
			break;
		case TYPE_COMMAND:
			if (loc != NOTHING)
			{
				db[loc].set_commands(remove_first (db[loc].get_commands(), what));
			}
			db[what].set_location(where);
			if (where != NOTHING)
			{
				PUSH(what, where, commands)
			}
			else
				db[what].set_next(NOTHING);
			break;
		case TYPE_ARRAY:
		case TYPE_PROPERTY:
		case TYPE_DICTIONARY:
		case TYPE_VARIABLE:
			if (loc != NOTHING)
				db[loc].set_info_items(remove_first (db[loc].get_info_items(), what));
			db[what].set_location(where);
			if (where != NOTHING)
				PUSH (what, where, info_items)
			else
				db[what].set_next(NOTHING);
			break;
		case TYPE_FUSE:
		case TYPE_ALARM:
			if (loc != NOTHING)
				db[loc].set_fuses(remove_first (db[loc].get_fuses(), what));
			db[what].set_location(where);
			if (where != NOTHING)
				PUSH (what, where, fuses)
			else
				db[what].set_next(NOTHING);
			break;
		case TYPE_EXIT:
			if (loc != NOTHING)
				db[loc].set_exits(remove_first (db[loc].get_exits(), what));
			db[what].set_location(where);
			if (where != NOTHING)
				PUSH (what, where, exits)
			else
				db[what].set_next(NOTHING);
			break;
		default:
			log_bug("Unhandled object type %d in moveto()!", Typeof(what));
	}
	return true;
}


void
context::send_contents (
dbref	loc,
dbref	dest)

{
	dbref first;
	dbref rest;

	first = db[loc].get_contents();
	db[loc].set_contents(NOTHING);

	/* blast locations of everything in list */
	DOLIST(rest, first)
	{
		db[rest].set_location(NOTHING);
	}

	while(first != NOTHING)
	{
		rest = db[first].get_real_next();
		if(Typeof(first) != TYPE_THING)
		{
			moveto(first, loc);
		}
		else
		{
			if (Sticky (first))
				send_home(first);
			else
				moveto(first, dest);
		}
		first = rest;
	}
}


void
context::maybe_dropto (
dbref	loc,
dbref	dropto)

{
	dbref	thing;

	if (loc == dropto)
		return; /* bizarre special case */

	/* check for players; if there are any, the drop won't trigger */
	DOLIST(thing, db[loc].get_contents())
	{
		if(Typeof(thing) == TYPE_PLAYER)
			return;
	}

	/* no players, send everything to the dropto */
	send_contents(loc, dropto);
}


void
context::enter_room (
dbref	loc)

{
	dbref		old;
	dbref		dropto = NOTHING;
	dbref		the_command;
	int		overlapped;
	dbref		overlap_room;
	fit_result	fit_error;

	/* Make sure we haven't managed to 'walk' through an unlinked exit */
	if(loc == NOTHING)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't enter NOTHING.");
		return;
	}

	/* check for room == HOME and fix it to a real location */
	if(loc == HOME)
		loc = db[player].get_destination(); /* home */

	old = db[player].get_location();

	if (contains (loc, player))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES,"You cannot enter that.");
		return;
	}
	if ((fit_error = will_fit (player, loc)) != SUCCESS)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You can't seem to go that way - %s", fit_errlist [fit_error]);
		return;
	}

	/* check for self-loop */
	/* self-loops don't do move or other player notification */
	/* but you still get autolook */
	if(loc != old)
	{
		count_down_fuses (*this, player, TOM_FUSE);	/* trigger the movement fuses */
		count_down_fuses (*this, db[player].get_location(), TOM_FUSE);

		if(old != NOTHING)
		{
			/* notify others unless DARK */
			if(!Dark(old) && !Dark(player))
			{
				sprintf(scratch_buffer, "%s has left.", getname_inherited (player));
				notify_except(db[old].get_contents(), player, player, scratch_buffer);
			}
		}



		/* ok, trigger off all the areas the player has left. */
		if (in_area (old, loc))
		{
			/* No need to execute any .enter commands, we ain't entering any new areas. */
			Matcher leave_matcher (player, ".leave", TYPE_COMMAND, player);
			leave_matcher.check_keys ();
			leave_matcher.match_bounded_area_command (old, loc);
			while (((the_command = leave_matcher.match_result ()) != NOTHING)
				&& (the_command != AMBIGUOUS)
				&& (!Dark (the_command))
				&& (db [the_command].get_location () != loc)		/* don't do the target room's .leave */
				&& (in_area (db [the_command].get_location (), loc)))	/* and don't do one in a higher area */
			{
				context *leave_context = new context (player, *this);
				/* Make sure the command can't unchpid to the wrong level */
				leave_context->set_unchpid_id(db[the_command].get_owner());
				/* Prevent the system recursing */
				leave_context->set_depth_limit(depth_limit - 1);
				if (could_doit (*leave_context, the_command))
					leave_context->prepare_compound_command (the_command, ".leave", "", "", db[the_command].get_owner());
				delete mud_scheduler.push_new_express_job (leave_context);
				leave_matcher.match_restart ();
			}
			/* go there */
			moveto(player, loc);
		}
		else if (in_area (loc, old))
		{
			/* go there */
			moveto(player, loc);
			/* No need to execute any .leave commands, we are going down areas, so not leaving any. */

			Simple_stack enter_stack;
			Matcher enter_matcher (player, ".enter", TYPE_COMMAND, player);
			enter_matcher.check_keys ();
			enter_matcher.match_bounded_area_command (loc, old);
			while (((the_command = enter_matcher.match_result ()) != NOTHING)	/* stack the .enter commands */
				&& (the_command != AMBIGUOUS)
				&& (!Dark (the_command))
				&& (db [the_command].get_location () != old)
				&& (in_area (db [the_command].get_location (), old)))
			{
				enter_stack.push (the_command);
				enter_matcher.match_restart ();
			}
			while (!enter_stack.empty ())		/* excute the .enter commands */
			{
				context *enter_context = new context (player, *this);

				the_command = enter_stack.top ();
				enter_stack.pop ();
				/* Make sure the command can't unchpid to the wrong level */
				enter_context->set_unchpid_id(db[the_command].get_owner());
				/* Prevent the system recursing */
				enter_context->set_depth_limit(get_depth_limit() - 1);
				if (could_doit (*enter_context, the_command))
					enter_context->prepare_compound_command (the_command, ".enter", "", "", db[the_command].get_owner());
				delete mud_scheduler.push_new_express_job (enter_context);
			}
		}
		else
		{
			/* ok, no direct area overlaps, so we need to trigger up each tree till they stop, or overlap. */
			/* So, lets find out where they overlap then. */
			overlap_room = loc;
			overlapped = 0;
			while ((overlap_room != NOTHING) && (!overlapped))
			{
				if (in_area (old, overlap_room))
					overlapped = 1;
				else
					overlap_room = db[overlap_room].get_location();
			}
			Matcher leave_matcher (player, ".leave", TYPE_COMMAND, player);
			leave_matcher.check_keys ();
			leave_matcher.match_bounded_area_command (old, overlap_room);
			while (((the_command = leave_matcher.match_result ()) != NOTHING)
				&& (the_command != AMBIGUOUS)
				&& (!Dark (the_command))
				&& (db [the_command].get_location () != overlap_room)
				&& (overlap_room == NOTHING || in_area (db [the_command].get_location (), overlap_room)))
			{
				context *leave_context = new context (player, *this);

				/* Make sure the command can't unchpid to the wrong level */
				leave_context->set_unchpid_id(db[the_command].get_owner());
				/* Prevent the system recursing */
				leave_context->set_depth_limit(get_depth_limit() - 1);
				if (could_doit (*leave_context, the_command))
					leave_context->prepare_compound_command (the_command, ".leave", "", "", db[the_command].get_owner());
				delete mud_scheduler.push_new_express_job (leave_context);
				leave_matcher.match_restart ();
			}

			/* go there */
			moveto(player, loc);

			Simple_stack enter_stack;

			Matcher enter_matcher (player, ".enter", TYPE_COMMAND, player);
			enter_matcher.check_keys ();
			enter_matcher.match_bounded_area_command (loc, overlap_room);
			while (((the_command = enter_matcher.match_result ()) != NOTHING)
				&& (the_command != AMBIGUOUS)
				&& (!Dark (the_command))
				&& (db [the_command].get_location () != overlap_room)
				&& (overlap_room == NOTHING || in_area (db [the_command].get_location (), overlap_room)))
			{
				enter_stack.push (the_command);
				enter_matcher.match_restart ();
			}
			while (!enter_stack.empty ())
			{
				context *enter_context = new context (player, *this);

				the_command = enter_stack.top ();
				enter_stack.pop ();
				/* Make sure the command can't unchpid to the wrong level */
				enter_context->set_unchpid_id(db[the_command].get_owner());
				/* Prevent the system recursing */
				enter_context->set_depth_limit(get_depth_limit() - 1);
				if (could_doit (*enter_context, the_command))
					enter_context->prepare_compound_command (the_command, ".enter", "", "", db[the_command].get_owner());
				delete mud_scheduler.push_new_express_job (enter_context);
			}
		}

		/* if old location has STICKY dropto, send stuff through it */
		if(old != NOTHING
			&& (dropto = db[old].get_destination()) != NOTHING
			&& (Sticky(old)))
		{
			maybe_dropto(old, dropto);
		}
	}

	if((Typeof(loc) == TYPE_FREE) || !((Typeof(loc) == TYPE_ROOM) || Container(loc)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The room you moved into doesn't exist any more");
		return;
	}
	if ((loc != NOTHING) && Connected(player))
		look_room (*this, loc);

	if(Ashcan(loc))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,"** This location is set ASHCAN - See or mail a Wizard if you think it is any good. **");
	if (Typeof (loc) == TYPE_ROOM)
		db[loc].set_last_entry_time();
}

void
context::send_home (
dbref	thing)

{
	switch(Typeof(thing))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			/* send his possessions home first! */
			/* that way he sees them when he arrives */
			send_contents(thing, HOME);
			/* If the object won't fit in its home, send it to limbo */
			{
				context *thing_context = new context (player, *this);

				if (will_fit(thing, db[thing].get_destination()) == SUCCESS)
					thing_context->enter_room (db[thing].get_destination());
				else
				{
					notify_colour(thing, thing, COLOUR_ERROR_MESSAGES, "A space-time distortion causes you to land in limbo");
					thing_context->enter_room (LIMBO); /* home */
				}
				delete mud_scheduler.push_new_express_job (thing_context);
			}

			if(db[db[thing].get_destination()].get_inherited_drop_message())
				notify_public_colour(thing, thing, COLOUR_DROP, "%s", db[db[thing].get_destination()].get_inherited_drop_message().c_str());

			if(db[db[thing].get_destination()].get_inherited_odrop() && !(Dark(thing)))
			{
				pronoun_substitute(scratch_buffer, BUFFER_LEN, thing, db[db[thing].get_destination()].get_inherited_odrop());
				notify_except(db[db[thing].get_destination()].get_contents(), thing, thing, scratch_buffer);
			}
			break;
		case TYPE_THING:
			if ((db[thing].get_destination() != NOTHING) && (will_fit(thing,db[thing].get_destination())==SUCCESS))
				moveto(thing, db[thing].get_destination());	/* home */
			else
				moveto(thing, LIMBO);	/* Limbo */
			break;
		default:
			/* no effect */
			break;
	}
}

bool
can_move (
context&	c,
const String&	direction)

{
	if(!string_compare(direction, "home"))
		return 1;

	/* otherwise match on exits */
	Matcher exit_matcher (c.get_player (), direction, TYPE_EXIT, c.get_effective_id ());
	exit_matcher.match_exit();
	dbref exit = exit_matcher.last_match_result();

	return((exit != NOTHING) && (Typeof(exit) == TYPE_EXIT));
}

void
context::do_move (
const	String& direction,
const	String&)

{
	dbref exit;
	dbref loc;
	enum fit_result fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!string_compare(direction, "home"))
	{
		/* send him home but steal all his possessions */
		if((loc = db[player].get_location()) != NOTHING)
		{
		}
		/* give the player the messages */
		notify_colour(player, player, COLOUR_MESSAGES, "You click your imaginary ruby slippers together and vanish in a puff of golden smoke.");
		notify_colour(player, player, COLOUR_MESSAGES, "You wake up back home, without your possessions.");
		send_home(player);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
	else
	{
		/* find the exit */
		Matcher exit_matcher (player, direction, TYPE_EXIT, get_effective_id ());
		exit_matcher.check_keys ();
		exit_matcher.match_exit();
		switch(exit = exit_matcher.match_result())
		{
			case AMBIGUOUS:
				notify_colour(player, player, COLOUR_MESSAGES, "I don't know which way you mean!");
				break;
			case NOTHING:
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't go that way.");
				break;
			default:
				if(Typeof(exit) != TYPE_EXIT)
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That isn't an exit.");
					break;
				}
				/* we got one */
				count_down_fuses (*this, exit, !TOM_FUSE);
				/* check to see if we got through */
				if(can_doit (*this, exit, "You can't go that way."))
				{
					if (contains (db[exit].get_destination(), player))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You seem to have difficultly doing that.");
						return;
					}
					if ((fit_error=will_fit(player, db[exit].get_destination()))!=SUCCESS)
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't seem to go that way - %s", fit_errlist [fit_error]);
						return;
					}
					enter_room (db[exit].get_destination());
					if (db[exit].get_inherited_drop_message())
						notify_public_colour(player, player, COLOUR_DROP, "%s", db[exit].get_inherited_drop_message().c_str());
					if (db[exit].get_destination() != HOME)
					{
						if (!Dark(player))
						{
							if (db[exit].get_inherited_odrop())
							{
								pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[exit].get_inherited_odrop());
								notify_except(db[db[player].get_location()].get_contents(), player, player, scratch_buffer);
							}
							if(!Dark(db[exit].get_destination()))
							{
								sprintf(scratch_buffer, "%s has arrived.", getname_inherited (player));
								notify_except(db[db[player].get_location()].get_contents(), player, player, scratch_buffer);
							}
						}
					}
					else
					{
						if (!Dark(player))
						{
							if (db[exit].get_inherited_odrop())
							{
								pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[exit].get_inherited_odrop());
								notify_except(db[db[player].get_destination()].get_contents(), player, player, scratch_buffer);
							}
							if(!Dark(db[player].get_destination()))
							{
								sprintf(scratch_buffer, "%s has arrived.", getname_inherited (player));
								notify_except(db[db[player].get_destination()].get_contents(), player, player, scratch_buffer);
							}
						}
					}
					return_status = COMMAND_SUCC;
				}
				break;
		}
	}
}


void
context::do_get (
const	String& what,
const	String& where)

{
	dbref	thing;
	dbref	container;
	enum	fit_result fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher what_matcher (player, what, TYPE_THING, get_effective_id ());
	what_matcher.match_neighbor ();
	what_matcher.match_exit ();
	if (what_matcher.match_result () == NOTHING)
		what_matcher.match_possession ();
	what_matcher.match_absolute();
	if (what_matcher.match_result () == NOTHING)
		what_matcher.match_command ();
	if (what_matcher.match_result () == NOTHING)
		what_matcher.match_array_or_dictionary ();
	if (what_matcher.match_result () == NOTHING)
		what_matcher.match_variable ();
	if (what_matcher.match_result () == NOTHING)
		what_matcher.match_fuse_or_alarm ();

	if((thing = what_matcher.noisy_match_result()) != NOTHING)
	{
		if (!where && (db[thing].get_location() == player))
		{
			notify_colour(player, player, COLOUR_MESSAGES, "You already have that!");
			return;
		}

		/* don't allow "get <player>:thing */
		if (Typeof(db[thing].get_location()) == TYPE_PLAYER) {
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't steal from other players!");
			return;
		}
		
		Accessed (thing);

		switch(Typeof(thing))
		{
			case TYPE_THING:
				if (!can_reach (player, thing))
				{
					notify_colour(player, player, COLOUR_MESSAGES, "You can't reach it.");
					return;
				}
				if (!can_doit (*this, thing, "You can't pick that up."))
					return;
				else
				{
					if (!where)
					{
						/* Not putting thing into a container */
						if (contains (player, thing))
						{
							notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That is impossible");
							return;
						}

						/* Don't get it if it's too big */
						if (will_fit(thing, player)==SUCCESS)
						{
							count_down_fuses (*this, thing, !TOM_FUSE);
							moveto(thing, player);
						}
						else
						{
							if ((db[player].get_inherited_mass_limit () != HUGE_VAL)
							&& (db[player].get_inherited_mass_limit ()) < (find_mass_of_contents_except (player, NOTHING) + db[thing].get_inherited_mass ()))
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't carry that much weight.");
							else
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't carry anything that big.");
							return;
						}
						sprintf (scratch_buffer, "%s picks up %s%s.", getname_inherited (player), getarticle (thing, ARTICLE_LOWER_INDEFINITE), getname_inherited (thing));
						notify_except (db[db [player].get_location()].get_contents(), player, player, scratch_buffer);
					}
					else
					{
						Matcher where_matcher (player, where, TYPE_THING, get_effective_id ());
						where_matcher.match_possession ();
						if ((container = where_matcher.noisy_match_result ()) != NOTHING)
						{
							if (!Container(container))
							{
								notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only put it in an open Container.");
								return;
							}
							else if (container == thing)
							{
								notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't get a thing into itself!");
								return;
							}
							else if (!can_reach (player, container))
							{
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't reach the destination container.");
								return;
							}
							else
							{
								if (contains (thing, container))
								{
									notify_colour (player,player, COLOUR_ERROR_MESSAGES, "That is impossible");
									return;
								}

								/* oh what fun! Have to check the object can (a) fit in the container */
								/* and (b) that the player is strong enough to carry it */
								if ((fit_error=will_fit(thing, container))!=SUCCESS)
								{
									notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't put that in there - %s", fit_errlist [fit_error]);
									return;
								}
								if ((db[player].get_inherited_mass_limit ()) < (find_mass_of_contents_except (player, NOTHING) + db[thing].get_inherited_mass ()))
								{
									notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't carry that as well!");
									return;
								}

								count_down_fuses (*this, thing, !TOM_FUSE);
								moveto (thing, container);
								sprintf (scratch_buffer, "%s picks up %s%s.", getname_inherited (player), getarticle (thing, ARTICLE_LOWER_INDEFINITE), getname_inherited (thing));
								notify_except (db [player].get_location(), player, NOTHING, scratch_buffer);
							}
						}
					}
				}
				break;
			case TYPE_EXIT:
				if(!controls_for_write (thing))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES,"You can't pick that up.");
					return;
				}
				else if(db[thing].get_destination() != NOTHING)
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't pick up a linked exit.");
					return;
				}
#ifdef RESTRICTED_BUILDING
				else if(!Builder(get_effective_id ()))
				{
					notify_colour(player, player, COLOUR_MESSAGES, "Only authorized builders may pick up exits.");
					return;
				}
#endif /* RESTRICTED_BUILDING */
				else
					moveto (thing, player);
				break;
			case TYPE_COMMAND:
				if (!controls_for_write (thing))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't pick that up.");
					return;
				}
				moveto (thing, player);
				break;
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
			case TYPE_VARIABLE:
				if (!controls_for_write (thing))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't pick that up.");
					return;
				}
				moveto (thing, player);
				break;
			case TYPE_FUSE:
				if (!controls_for_write (thing))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't pick that up.");
					return;
				}
				moveto (thing, player);
				break;
			default:
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't pick that up.");
				return;
		}
		if (!in_command())
			notify_colour (player, player, COLOUR_MESSAGES, "Taken.");
		set_return_string (unparse_for_return_inherited (*this, thing));
		return_status = COMMAND_SUCC;
	}
}


void
context::do_drop (
const	String& name,
const	String& where)

{
	dbref			loc;
	dbref			thing;
	dbref			container;
	enum	fit_result	fit_error = SUCCESS;

	if ((loc = getloc(player)) == NOTHING)
		return;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher name_matcher (player, name, TYPE_THING, get_effective_id ());
	name_matcher.match_possession ();
	name_matcher.match_absolute ();
	if (name_matcher.match_result () == NOTHING)
		name_matcher.match_player_command ();
	if (name_matcher.match_result () == NOTHING)
		name_matcher.match_array_or_dictionary ();
	if (name_matcher.match_result () == NOTHING)
		name_matcher.match_variable ();
	if (name_matcher.match_result () == NOTHING)
		name_matcher.match_fuse_or_alarm ();

	if ((thing = name_matcher.noisy_match_result()) != NOTHING)
	{
		/* Work out where to drop it */
		if (!where)
			container = loc;
		else
		{
			Matcher where_matcher (player, where, TYPE_NO_TYPE, get_effective_id ());
			where_matcher.match_everything ();
			if ((container = where_matcher.noisy_match_result ()) == NOTHING)
				return;
		}

		/* you can only drop things you are carrying */
		if (db[thing].get_location() != player) {
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only drop things that you are carrying.");
			return;
		}

		Accessed (thing);

		/* Do the drop */
		switch (Typeof (thing))
		{
			case TYPE_EXIT:
				/* special behavior for exits */
				if (!controls_for_write (container))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				if ((Typeof(container) != TYPE_ROOM) && (Typeof (container) != TYPE_THING))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Exits can only be placed in rooms.");
					return;
				}
				/* else we can put it down */
				moveto(thing, container);
				break;
			case TYPE_THING:
				count_down_fuses (*this, thing, !TOM_FUSE);
				/* FALLTHROUGH */
				if (Typeof (container) == TYPE_THING)
				{
					if (!Container (container) || !Open (container))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only drop into a thing if it's open and a container.");
						return;
					}
					else if (!can_reach (player, container))
					{
						notify_colour (player, player, COLOUR_MESSAGES, "You can't reach the thing you want to drop it into.");
						return;
					}
					else if ((thing == container) || (contains (container, thing)))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's a physical impossibility.");
						return;
					}
					else if ((fit_error = will_fit(thing, container)) != SUCCESS)
					{
						if (container == getloc(player))
							notify_colour(player, player, COLOUR_MESSAGES, "You can't drop that. It's too cramped in here");
						else
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't put that in there - %s", fit_errlist [fit_error]);
						}
						return;
					}
					else
					{
						moveto (thing, container);
						if(db[thing].get_inherited_drop_message())
							notify_public_colour(player, player, COLOUR_DROP, "%s", db[thing].get_inherited_drop_message().c_str());
						else if (!in_command())
							notify_colour(player, player, COLOUR_DROP, "Dropped.");
					}
				}
				else if(Sticky(thing))
				{
					send_home(thing);
					if(db[thing].get_inherited_drop_message())
						notify_public_colour(player, player, COLOUR_DROP, "%s", db[thing].get_inherited_drop_message().c_str());
					else if (!in_command())
						notify_colour(player, player, COLOUR_DROP, "Dropped.");
				}
				else
				{
					if((db[loc].get_destination() != NOTHING && Typeof(loc) == TYPE_ROOM) && !(Sticky(loc)) && (db[loc].get_destination() != thing))
						/* location has an immediate dropto - test to see if it will fit first */
						if (will_fit(thing, db[loc].get_destination())==SUCCESS)
						{
							notify_colour(player, player, COLOUR_MESSAGES, "As the object vanishes there is a faint wisp of red smoke");
							moveto(thing, db[loc].get_destination());
						}
						else
						{
							notify_colour(player, player, COLOUR_MESSAGES, "As the object vanishes there is a faint wisp of purple smoke");
							send_home(thing); /* home */
						}
					else
					{
						if (will_fit(thing, loc)==SUCCESS)
							moveto(thing, loc);
						else
						{
							if (loc == db[player].get_location())
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "It's too cramped in here to drop anything");
							else
							{
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That won't go in there - %s", fit_errlist [fit_error]);
							}
						}
					}
					if(db[thing].get_inherited_drop_message())
						notify_public_colour(player, player, COLOUR_DROP, "%s", db[thing].get_inherited_drop_message().c_str());
					else if (!in_command())
						notify_colour(player, player, COLOUR_DROP, "Dropped.");

					if (db[loc].get_inherited_drop_message())
						notify_public_colour(player, player, COLOUR_DROP, "%s", db[loc].get_inherited_drop_message().c_str());
					if (!(Dark(thing)))
					{
						if (db[thing].get_inherited_odrop())
							pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[thing].get_inherited_odrop());
						else
							sprintf(scratch_buffer, "%s dropped %s%s.", getname_inherited (player), getarticle (thing, ARTICLE_LOWER_INDEFINITE), getname_inherited (thing));
						notify_except(db[loc].get_contents(), player, player, scratch_buffer);
						if (db[loc].get_inherited_odrop() && !(Dark(loc)))
						{
							pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[loc].get_inherited_odrop());
							notify_except(db[loc].get_contents(), player, player, scratch_buffer);
						}
					}
				}
				set_return_string (unparse_for_return_inherited (*this, thing));
				return_status = COMMAND_SUCC;
				return;
			case TYPE_COMMAND:
				if (!controls_for_write (thing))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				switch (Typeof (container))
				{
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_ROOM:
					case TYPE_THING:
					case TYPE_EXIT:
						if (!controls_for_write (container))
						{
							notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
							return;
						}
						moveto (thing, container);
						break;
					default:
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't drop a command on that.");
						return;
				}
				break;
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
			case TYPE_VARIABLE:
				if (!controls_for_write (thing))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				switch (Typeof (container))
				{
					case TYPE_ALARM:
					case TYPE_COMMAND:
					case TYPE_EXIT:
					case TYPE_FUSE:
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_ROOM:
					case TYPE_THING:
						/* doesn't have to own object to attach variable */
						moveto (thing, container);
						break;
					default:
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't attach an information object to that.");
						return;
				}
				break;
			case TYPE_FUSE:
				if (!controls_for_write (thing))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				switch (Typeof (container))
				{
					case TYPE_ROOM:
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_THING:
					case TYPE_EXIT:
						if (!controls_for_write (container))
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
							return;
						}
						moveto (thing, container);
						break;
					default:
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't attach a fuse to that.");
						return;
				}
				break;
		}
		if (!in_command())
			notify_colour (player, player, COLOUR_DROP, "Dropped.");
		set_return_string (unparse_for_return_inherited (*this, thing));
		return_status = COMMAND_SUCC;
	}
}


/*
 * do_remote: Remote to a location, do a command, un-remote afterwards.
 *	Currently buggers up the remoting player's position in the contents
 *	list of initial location. Tough.
 *
 * Grimthorpe: Well, here is a first attempt at not buggering up the remote
 *		player's position...
 */

void
context::do_remote (
const	String& loc_string,
const	String& command)

{
	dbref	loc;
	dbref	cached_loc;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher where_matcher (player, loc_string, TYPE_ROOM, get_effective_id ());
	where_matcher.match_absolute ();
	where_matcher.match_player ();
	// If you @remote to a player, try to go to the player's location instead.
	loc = where_matcher.match_result();
	if ((loc >= 0) && (loc < db.get_top()) && (Typeof(loc) == TYPE_PLAYER))
	{
		loc = db[loc].get_location();
	}
	if ((loc == NOTHING)
		|| (loc == AMBIGUOUS)
		|| (loc < 0)
		|| (loc >= db.get_top ())
		|| ((Typeof (loc) != TYPE_ROOM) && !Container (loc))
		|| !controls_for_read (loc))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only remote to a room (or container) that you control.");
		return;
	}

	/* PJC 7/7/1998: Don't remote into something you're carrying */
	if (contains (loc, player))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot enter something you are carrying.");
		return;
	}

	cached_loc = db [player].get_location();
	db[player].set_remote_location(loc);	// sets up the fudged location.
	Accessed (loc);
//	moveto (player, loc);

	const size_t old_depth = call_stack.size ();
	process_basic_command (command);
	while(call_stack.size () > old_depth)
		step();

	db[player].set_remote_location(cached_loc);
	/* Just make sure players don't pull silly stunts like zapping the thing they remoted out of */
	if ((cached_loc < 0) || (cached_loc >= db.get_top ())
		|| (Typeof(cached_loc) == TYPE_FREE)
		|| ((Typeof (cached_loc) != TYPE_ROOM) && !Container (cached_loc)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Oops! You can't return to where you came from! You'd better go to limbo");
		enter_room(LIMBO);
	}
}

