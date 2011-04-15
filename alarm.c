/* static char SCCSid[] = "@(#)alarm.c	1.11\t1/5/95"; */
/** \file
 * Implementation of alarms, Pending and Pending_alarm.
 */

#include "copyright.h"

#include <time.h>
#include <ctype.h>
#include <stdlib.h>

#pragma implementation "alarm.h"

#include "db.h"
#include "alarm.h"
#include "interface.h"
#include "match.h"
#include "config.h"
#include "externs.h"
#include "command.h"
#include "context.h"

#include "config.h"
#include "log.h"


/*
 * Patch alarms in the database into the pending queue of alarms.
 *	Also amended to do other things:
 *	- Disconnect apparently-connected players;
 *	- @connect each connected puppet;
 *	- Build up the command cache;
 *	- Regenerate the list of what references what.
 */

void
db_patch_alarms ()

{
	dbref	i;

	for (i = 0; i < db.get_top (); i++)
	{
		if ((Typeof (i) == TYPE_ALARM) && (db [i].get_description ()))
			db.pend (i);

		/* We're just going up so, by definition, no players are connected */
		if ((Typeof (i) == TYPE_PLAYER) && (Connected(i)))
			mud_disconnect_player(i);

		// Force an @connect for each connected puppet.
		if ((Typeof (i) == TYPE_PUPPET) && (Connected(i)))
		{
			context *c = new context (db[i].get_owner(), context::DEFAULT_CONTEXT);
			char tmpnum[20];
			sprintf(tmpnum, "#%d", (int)i);
			db[i].clear_flag(FLAG_CONNECTED);
			c->do_at_connect(tmpnum, NULLSTRING);
			delete mud_scheduler.push_new_express_job (c);
		}
		if((Typeof(i) == TYPE_COMMAND) && (db[i].get_location() != NOTHING))
		{
			db[db[i].get_location()].add_command(i);
		}
		/* Knock out all REFERENCED flags before we re-generate them. Cheap, and allows faster destroys */
		if (Typeof (i) != TYPE_FREE)
			db[i].clear_flag(FLAG_REFERENCED);
	}

	/* Re-generate REFERENCED flags */
	for (i = 0; i < db.get_top (); i++)
		if (Typeof (i) != TYPE_FREE)
		{
			db [db [i].get_owner ()].set_referenced ();
			if (db [i].get_key () != TRUE_BOOLEXP)
				db [i].get_key ()->set_references ();
			if (db [i].get_lock_key () != TRUE_BOOLEXP)
				db [i].get_lock_key ()->set_references ();
			if (db [i].get_parent () >= 0)
				db [db [i].get_parent ()].set_referenced ();
			if (db [i].get_destination () >= 0)
				db [db [i].get_destination ()].set_referenced ();
			if (db [i].get_csucc () >= 0)
				db [db [i].get_csucc ()].set_referenced ();
			if (db [i].get_cfail () >= 0)
				db [db [i].get_cfail ()].set_referenced ();
			if ((Typeof (i) == TYPE_PLAYER) && (db [i].get_controller () != i))
			{
				if (Typeof (db [i].get_controller ()) != TYPE_PLAYER) {
					log_bug("player %s(#%d) has non-player %s(#%d) as controller --- patching to themselves",
							getname(i),
							i,
							getname(db[i].get_controller()),
							db[i].get_controller()
					);
					db [i].set_controller (i);
				}
				db [db [i].get_controller ()].set_referenced ();
			}
		}
}


/**
 * Increment *ptr past arbitrary whitespace followed by a positive integer. Return the integer.
 *	Also reads '*' as -1.
 *	Used to read the values from an alarm's description.
 */

static int
eat_space_and_number (
const	char	**ptr)

{
	int	number;

	number = 0;
	while ((**ptr) && isspace (**ptr))
		(*ptr)++;

	/* If it's a '*', return a token for wildcard. */
	if (**ptr == '*')
	{
		(*ptr)++;
		return (-1);
	}

	/* It's not a '*', so it had better be a real number. Blanks count as 0. */
	while (**ptr && (**ptr >= '0') && (**ptr <= '9'))
		number = number * 10 + *((*ptr)++) - '0';
	return (number);
}


/*
 * Given an input string containing an absolute or relative time, return an int (TODO: time_t)
 *	of when the time will next come up given the current system clock.
 */

static long
time_of_next_cron (
const	String& string)

{
	struct	tm	*rtime;
	const	char	*ptr;
	bool		forward = false;
	int		second;
	int		minute;
	int		hour;
	int		day;
	time_t		now;
	long		then;

	ptr = string.c_str();
	while ((*ptr) && isspace (*ptr))
		ptr++;
	if (*ptr == '+')
	{
		ptr ++;
		forward = true;
	}
	second = eat_space_and_number (&ptr);
	minute = eat_space_and_number (&ptr);
	hour = eat_space_and_number (&ptr);
	day = eat_space_and_number (&ptr);

	/* Round */
	if (second != -1)
		second = (((second % 60) + ALARM_RESOLUTION_SECONDS - 1) / ALARM_RESOLUTION_SECONDS) * ALARM_RESOLUTION_SECONDS;
	if (minute != -1)
		minute = minute % 60;
	if (hour != -1)
		hour = hour % 24;
	if (day != -1)
		day = day % 7;

	/* When are we? */
	time (&now);
	now ++;
	rtime = localtime(&now);
	then = now;

	if (forward)
	{
		now += ((day * 24 + hour) * 60 + minute) *60 + second;
		if (now <= then)
			now += 10;
	}
	else
	{
		if (second == -1)
			second = (rtime->tm_sec + ALARM_RESOLUTION_SECONDS) % 60;
		now += second - rtime->tm_sec;
		if (minute == -1)
			minute = (rtime->tm_min + (now < then)) % 60;
		now += (minute - rtime->tm_min) * 60;
		if (hour == -1)
			hour = (rtime->tm_hour + (now < then)) % 60;
		now += (hour - rtime->tm_hour) * 60 * 60;
		if (day == -1)
			day = (rtime->tm_wday + (now < then)) % 7;
		now += (day - rtime->tm_wday) * 60 * 60 * 24;
		if (now < then)
			now += 60 * 60 * 24 * 7;
	}

	return (now);
}


/**
 * Add the alarm at the specified dbref into the list of pending alarms.
 */

void
Database::pend (
dbref	alarm)

{
	// Make sure there isn't an entry for the alarm there already.
	unpend(alarm);

	Pending_alarm*new_entry = new Pending_alarm (alarm, time_of_next_cron (db[alarm].get_description()));
	new_entry->insert_into ((Pending **) &alarms);
}


/*
 * If alarm exists in the list of pending alarms, remove it from that list.  If not, do nothing.
 */

void
Database::unpend (
dbref	alarm)
{
	if (alarms != NULL)
		alarms->remove_object ((Pending **) &alarms, alarm);
}


/**
 * Emit a list of pending alarms.  Unusually for an @? command, this lists direct to the
 * player rather than to a string. TODO: Make a version that can be interrogated by other
 * commands.
 */

void
context::do_query_pending (
const	String& alarm,
const	String&)

{
	int		count = 0;
	Pending_alarm	*current;
	struct	tm	*rtime;
	time_t		now;
	time_t		then;
	dbref		target = NOTHING;

	return_status = COMMAND_FAIL;
	set_return_string(error_return_string);

	if(alarm)
	{
		Matcher matcher (get_player(), alarm, TYPE_ALARM, get_effective_id());
		if(gagged_command())
			matcher.work_silent();
		matcher.match_absolute();
		if(matcher.match_result() == NOTHING)
			matcher.match_fuse_or_alarm();

		if(((target = matcher.match_result()) == NOTHING)
			|| (Typeof(target) != TYPE_ALARM))
		{
			if(!gagged_command())
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't know which alarm you means.");
			}
			return;
		}
		if(!controls_for_read(target))
		{
			if(!gagged_command())
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't control that alarm");
			}
			return;
		}
		for(current = db.get_alarms(); current != NULL; current = (Pending_alarm*) current->get_next())
		{
			if(current->get_object() == target)
			{
				time(&now);
				sprintf(scratch_buffer, "%d;%s",
					(int)(current->get_time_to_execute() - now),
					unparse_for_return(*this, db[target].get_destination()).c_str());
				set_return_string(scratch_buffer);
				return_status = COMMAND_SUCC;
				break;
			}
		}
	}
	else
	{
		time (&now);
		for (current = db.get_alarms (); current != NULL; current = (Pending_alarm *) current->get_next ())
		{
			if (controls_for_read (current->get_object ()))
			{
				then = current->get_time_to_execute ();
				rtime = localtime (&then);
				sprintf (scratch_buffer,
					"%2dd %02dh %02dm %02ds (%ld secs): %s",
					rtime->tm_wday,
					rtime->tm_hour,
					rtime->tm_min,
					rtime->tm_sec,
					(long int)current->get_time_to_execute () - now,
					unparse_object (*this, current->get_object ()).c_str()
					);
				strcat (scratch_buffer, " firing ");
				strcat (scratch_buffer, unparse_object (*this, db[current->get_object ()].get_destination ()).c_str());
				notify (player, "%s", scratch_buffer);
				count++;
			}
		}
		if (count == 0)
			notify (player, "You have nothing pending.");
		set_return_string (ok_return_string);
		return_status = COMMAND_SUCC;
	}
}


/*
 * If there is an alarm on the pending list that should have fired before now, remove it from
 *	the pending list and return it.  Otherwise return NOTHING.  Designed to be called
 *	multiple times until it returns NOTHING.
 */

dbref
Database::alarm_pending (
time_t	now)

{
	Pending_alarm	*current;
	dbref		the_command;

	current = alarms;
	if ((current == NULL) || (current->get_time_to_execute () >= now))
		return (NOTHING);
	else
	{
		current->remove_from ((Pending **) &alarms);
		the_command = current->get_object ();
		delete (current);
		return (the_command);
	}
}


/**
 * Create an unlinked Pending that knows its object.
 */

Pending::Pending (
dbref	obj)

: previous (NULL)
, next (NULL)
, object (obj)

{
}


/**
 * doubly-linked-list maintenance with a sting in the tail - namely
 *	that we're maintaining the head of the list as well.  TODO: Better comment.
 */

void
Pending::insert_into (
Pending	**list)

{
	if (*list == NULL)
		*list = this;
	else if (*this < **list)
	{
		previous = (*list)->previous;
		next = *list;
		if (previous != NULL)
			previous->next = this;
		else
			*list = this;
		if (next != NULL)
			next->previous = this;
	}
	else if ((*list)->next == NULL)
	{
		(*list)->next = this;
		previous = *list;
	}
	else
		insert_into (&((*list)->next));
}


/**
 * remove_from: Remove and return this item. *list is the previous pointer (i.e. the pointer to this item).
 */

Pending *
Pending::remove_from (
Pending	**list)

{
	*list = next;
	if (next != NULL)
		next->previous = previous;

	return (this);
}


/**
 * Try to remove the candidate dbref from list.  Tail-recursive.
 */

void
Pending::remove_object (
Pending	**list,
dbref	candidate)

{
	if (object == candidate)
	{
		remove_from (list);
		delete (this);
	}
	else if(next != NULL)
		next->remove_object (&((*list)->next), candidate);
}


/**
 * Construct a new unlinked Pending_alarm with the given object and firing time.
 */

Pending_alarm::Pending_alarm (
dbref	obj,
time_t	firing_time)

: Pending (obj)
, time_to_execute (firing_time)

{
}
