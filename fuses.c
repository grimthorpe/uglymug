/* static char SCCSid[] = "@(#)fuses.c	1.16\t1/26/95"; */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "config.h"
#include "match.h"
#include "alarm.h"
#include "context.h"


void	insert_into_pending_queue_fuse	(dbref player, dbref fuse, dbref command, const char *string);
extern	int	abort_from_fuse;


void
count_down_fuses (
context	&c,
dbref	object,
int	tom_fuse)

{
	int		value;
	char		buf [15];
	dbref		fuse;

	const	char	*command_arg1 = c.get_simple_command ();
	const	char	*command_arg2 = reconstruct_message (value_or_empty (c.get_arg1 ()), value_or_empty (c.get_arg2 ()));
	Matcher		matcher		(c.get_player (), object, TYPE_FUSE, c.get_effective_id ());

	matcher.match_count_down_fuse ();
	while (((fuse = matcher.match_result ()) != NOTHING) && (fuse != AMBIGUOUS))
	{
		if ((!Locked (fuse))
			&& (tom_fuse == Tom (fuse))
			&& could_doit (c, fuse)
			&& (!Abort (fuse))
			&& (db[fuse].get_description () != NULL))
		{
			value = atoi(db[fuse].get_description ()) - 1;
			db[fuse].set_description (NULL);
			if (value <= 0)
			{
				/* BANG! */
				if ((db[fuse].get_csucc () != NOTHING)
					&& (could_doit (c, db[fuse].get_csucc ())))
				{
					if (Sticky (fuse))
						c.pend_fuse (fuse, True, "SUCC", command_arg1, command_arg2, matcher);
					else
					{
						context	*fuse_context = new context (c.get_player ());
						fuse_context->set_unchpid_id (db [fuse].get_owner ());
						fuse_context->do_compound_command (db[fuse].get_csucc (), "SUCC", command_arg1, command_arg2, db [fuse].get_owner(), matcher);
						delete mud_scheduler.push_express_job (fuse_context);
					}
				}

				/* we're checking the fuse is still a fuse in case it was destroyed in the */
				/* commands fired by the fuse... it's a hack: if the commands destroy the */
				/* fuse and create a new one, the new one will get redescribed with its */
				/* drop value... tough. - Leech */
				if (Typeof (fuse) == TYPE_FUSE && db[fuse].get_drop_message ())
					db[fuse].set_description (db[fuse].get_drop_message ());
			}
			else
			{
				/* Normal countdown */
				if ((db[fuse].get_cfail () != NOTHING)
					&& (could_doit (c, db[fuse].get_cfail ())))
				{
					if (Sticky (fuse))
						c.pend_fuse (fuse, False, "FAIL", command_arg1, command_arg2, matcher);
					else
					{
						context	*fuse_context = new context (c.get_player ());
						fuse_context->set_unchpid_id (db [fuse].get_owner ());
						fuse_context->do_compound_command (db[fuse].get_cfail (), "FAIL", command_arg1, command_arg2, db[fuse].get_owner(), matcher);
						delete mud_scheduler.push_express_job (fuse_context);
					}
				}
				sprintf (buf, "%d", value);
				if (Typeof (fuse) == TYPE_FUSE)
					db[fuse].set_description (buf);
			}
		}
		if (Typeof(fuse) == TYPE_FUSE) /* They might have destroyed the fuse with the command, so... */
			matcher.match_restart ();
		else
			break; /* We can't carry on looking. */
	}
}

#ifdef ABORT_FUSES

void
count_down_abort_fuses (
context	&c,
dbref	object)

{
	int		value;
	char		buf [15];
	dbref		fuse;

	const	char	*command_arg1 = c.get_simple_command ();
	const	char	*command_arg2 = reconstruct_message (value_or_empty (c.get_arg1 ()), value_or_empty (c.get_arg2 ()));
	Matcher		matcher		(c.get_player (), object, TYPE_FUSE, c.get_effective_id ());

	matcher.match_count_down_fuse ();
	while (((fuse = matcher.match_result ()) != NOTHING) && (fuse != AMBIGUOUS))
	{
		if ((!Locked (fuse))
			&& could_doit (c, fuse)
			&& (Abort (fuse))
			&& (db[fuse].get_description () != NULL))
		{
			value = atoi(db[fuse].get_description ()) - 1;
			db[fuse].set_description (NULL);
			if (value <= 0)
			{
				/* BANG! */
				if ((db[fuse].get_csucc () != NOTHING)
					&& (could_doit (c, db[fuse].get_csucc ())))
				{
					context	*fuse_context = new context (c.get_player ());
					fuse_context->set_unchpid_id (db [fuse].get_owner ());
					fuse_context->do_compound_command (db[fuse].get_csucc (), "SUCC", command_arg1, command_arg2, db [fuse].get_owner(), matcher);
					mud_scheduler.push_express_job (fuse_context);

					if (fuse_context->get_return_status () == COMMAND_FAIL)
						abort_from_fuse = 1;
					delete fuse_context;
				}

				/* we're checking the fuse is still a fuse in case it was destroyed in the */
				/* commands fired by the fuse... it's a hack: if the commands destroy the */
				/* fuse and create a new one, the new one will get redescribed with its */
				/* drop value... tough. - Leech */
				if (Typeof (fuse) == TYPE_FUSE && db[fuse].get_drop_message ())
					db[fuse].set_description (db[fuse].get_drop_message ());
			}
			else
			{
				/* Normal countdown */
				if ((db[fuse].get_cfail () != NOTHING)
					&& (could_doit (c, db[fuse].get_cfail ())))
				{
					if (Sticky (fuse))
						c.pend_fuse (fuse, False, "FAIL", command_arg1, command_arg2, matcher);
					else
					{
						context	*fuse_context = new context (c.get_player ());
						fuse_context->set_unchpid_id (db [fuse].get_owner ());
						fuse_context->do_compound_command (db[fuse].get_cfail (), "FAIL", command_arg1, command_arg2, db[fuse].get_owner(), matcher);
						delete mud_scheduler.push_express_job (fuse_context);
					}
				}
				sprintf (buf, "%d", value);
				if (Typeof (fuse) == TYPE_FUSE)
					db[fuse].set_description (buf);
			}
		}
		if (Typeof(fuse) == TYPE_FUSE)
			matcher.match_restart ();
		else
			break;
	}
}

#endif

Pending_fuse::Pending_fuse (
dbref		f,
Boolean		succ,
const	char	*sc,
const	char	*a1,
const	char	*a2,
const	Matcher	&m)

: Pending (f)
, success (succ)
, command (alloc_string (sc))
, arg1 (alloc_string (a1))
, arg2 (alloc_string (a2))
, matcher (m)

{
}


Pending_fuse::~Pending_fuse ()

{
	if (command)
		free (const_cast <char *> (command));
	if (arg1)
		free (const_cast <char *> (arg1));
	if (arg2)
		free (const_cast <char *> (arg2));
}


void
Command_and_arguments::fire_sticky_fuses (
context	&c)

{
	Pending_fuse	*temp;

	while (sticky_fuses != NULL)
	{
		sticky_fuses->fire (c);
		temp = (Pending_fuse *) sticky_fuses->remove_from ((Pending **) &sticky_fuses);
		delete (temp);
	}
}


void
Pending_fuse::fire (
context	&c)

{
	dbref	command_id;

	if (Typeof (get_object ()) == TYPE_FUSE)
	{
		if ((command_id = success ? db [get_object ()].get_csucc () : db [get_object ()].get_cfail ()) != NOTHING)
		{
			if (Typeof (command_id) != TYPE_COMMAND)
				fputs ("BUG: Pended fuse does not fire a command.", stderr);
			else
			{
				context	*fuse_context = new context (c.get_player ());
				fuse_context->do_compound_command (command_id, command, arg1, arg2, db [get_object ()].get_owner (), matcher);
				delete mud_scheduler.push_express_job (fuse_context);
			}
		}
	}
}
