/* static char SCCSid[] = "@(#)alarm.c	1.11\t1/5/95"; */
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


/*
 * db_patch_alarms: Patch alarms in the database into the pending queue of alarms.
 *	Also does odd other things.
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

		if ((Typeof (i) == TYPE_PUPPET) && (Connected(i)))
		{
			context *c = new context (GOD_ID);
			char tmpnum[20];
			sprintf(tmpnum, "#%d", i);
			db[i].clear_flag(FLAG_CONNECTED);
			c->do_at_connect(tmpnum, NULL);
			delete mud_scheduler.push_express_job (c);
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
				if (Typeof (db [i].get_controller ()) != TYPE_PLAYER)
				{
					Trace( "BUG: Player %d has non-player %d as controller --- patching to GOD.\n", i, db[i].get_controller());
					db [i].set_controller (GOD_ID);
				}
				db [db [i].get_controller ()].set_referenced ();
			}
		}
}


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


static long
time_of_next_cron (
const	CString& string)

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
		second = (((second % 60) + 9) / 10) * 10;
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
			second = (rtime->tm_sec + 10) % 60;
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

void
Database::pend (
dbref	alarm)

{
	Pending_alarm	*new_entry;
	
	new_entry = new Pending_alarm (alarm, time_of_next_cron (db[alarm].get_description()));
	new_entry->insert_into ((Pending **) &alarms);
}


void
Database::unpend (
dbref	alarm)
{
	if (alarms != NULL)
		alarms->remove_object ((Pending **) &alarms, alarm);
}


void
context::do_query_pending (
const	CString&,
const	CString&)

{
	int		count = 0;
	Pending_alarm	*current;
	struct	tm	*rtime;
	time_t		now;
	time_t		then;

	time (&now);
	for (current = db.get_alarms (); current != NULL; current = (Pending_alarm *) current->get_next ())
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
				unparse_object (*this, current->get_object ())
                                );
			strcat (scratch_buffer, " firing ");
			strcat (scratch_buffer, unparse_object (*this, db[current->get_object ()].get_destination ()));
			notify (player, "%s", scratch_buffer);
			count++;
		}
	if (count == 0)
		notify (player, "You have nothing pending.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


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


Pending::Pending (
dbref	obj)

: previous (NULL)
, next (NULL)
, object (obj)

{
}


/*
 * doubly-linked-list maintenance with a sting in the tail - namely
 *	that we're maintaining the head of the list as well.
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


Pending *
Pending::remove_from (
Pending	**list)

{
	*list = next;
	if (next != NULL)
		next->previous = previous;

	return (this);
}


void
Pending::remove_object (
Pending	**list,
dbref	candidate)

{
	if (this == NULL)
		Trace( "BUG: Attempt to remove element from list that didn't contain it.\n");
	else if (object == candidate)
	{
		remove_from (list);
		delete (this);
	}
	else
		next->remove_object (&((*list)->next), candidate);
}


Pending_alarm::Pending_alarm (
dbref	obj,
time_t	firing_time)

: Pending (obj)
, time_to_execute (firing_time)

{
}
