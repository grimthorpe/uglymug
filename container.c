/* static char SCCSid[] = "@(#)container.c	1.15\t6/14/95"; */
#include "copyright.h"

/* Commands that manipulate containers */

#include "db.h"
#include "config.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "match.h"
#include "context.h"


void
context::do_open (
const	String& object,
const	String& )

{
	dbref	container;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_possession ();
	matcher.match_neighbor ();
	matcher.match_here ();
	matcher.match_absolute ();

	if ((container = matcher.noisy_match_result()) == NOTHING)
		return;
	if (!Container(container))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You can't open that.");
		return;
	}
	count_down_fuses (*this, container, !TOM_FUSE);
	if (!Openable (container))
		notify_censor_colour(player, player, COLOUR_MESSAGES,  "%s cannot be opened.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	else if (Locked (container))
	{
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are locked.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is locked.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	}
	else if (Open (container))
	{
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are already open.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is already open.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	}
	else
	{
		db[container].set_flag(FLAG_OPEN);
		Modified (container);
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are now open", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is now open", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_close (
const	String& object,
const	String& )

{
	dbref	container;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_possession ();
	matcher.match_neighbor ();
	matcher.match_here ();
	matcher.match_absolute ();

	if ((container = matcher.noisy_match_result ()) == NOTHING)
		return;
	if (!Container(container))
	{
		notify_censor_colour (player, player, COLOUR_MESSAGES, "You can't close that.");
		return;
	}
	count_down_fuses (*this, container, !TOM_FUSE);
	if (!Openable (container))
		notify_censor_colour(player, player, COLOUR_MESSAGES, "%s cannot be closed.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	else if (Locked (container))
	{
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are locked.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is locked.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	}
	else if (!Open (container))
	{
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are already closed.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is already closed.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
	}
	else
	{
		db[container].clear_flag(FLAG_OPEN);
		Modified (container);
		if (db[container].get_flag(FLAG_ARTICLE_PLURAL))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s are now closed.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		else
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s is now closed.", unparse_objectandarticle_inherited (*this, container, ARTICLE_UPPER_DEFINITE));
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_at_cstring (
const	String& object,
const	String& description)

{
	dbref container;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_possession ();
	matcher.match_neighbor ();
	matcher.match_here ();
	matcher.match_absolute ();

	if ((container = matcher.noisy_match_result()) == NOTHING)
		return;
	if (!controls_for_write (container))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
	else if (!((Typeof(container) == TYPE_ROOM) || (Container (container))))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't change the contents string for that.");
	else
	{
		db[container].set_contents_string (description);
		Modified (container);
		if (!in_command ())
		{
			if (description && *description.c_str() != '\0')
				notify_censor_colour(player, player, COLOUR_MESSAGES, "Contents string set to '%s'.", db [container].get_contents_string ().c_str());
			else
				notify_colour (player, player, COLOUR_MESSAGES, "Contents string reset.");
		}
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_lock (
const	String& object,
const	String& )

{
	dbref	container;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_possession ();
	matcher.match_neighbor ();
	matcher.match_here ();
	matcher.match_absolute ();

	if ((container = matcher.noisy_match_result()) == NOTHING)
		return;
	if (!Container (container))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You cannot lock that.");
		return;
	}
	count_down_fuses (*this, container, !TOM_FUSE);
	if (!Openable (container))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't lock something that cannot be opened.");
	else if (!db[container].get_lock_key ()->eval (*this, matcher))
		notify_colour (player, player, COLOUR_MESSAGES, "You couldn't lock the container.");
	else
	{
		db[container].set_flag(FLAG_LOCKED);
		Modified (container);
		notify_colour (player, player, COLOUR_MESSAGES, "Locked.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_unlock (
const	String& object,
const	String& )

{
	dbref	container;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, object, TYPE_THING, get_effective_id ());
	matcher.match_possession ();
	matcher.match_neighbor ();
	matcher.match_here ();
	matcher.match_absolute ();

	if ((container = matcher.noisy_match_result()) == NOTHING)
		return;

	if (!Container (container))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You cannot unlock that.");
		return;
	}

	count_down_fuses (*this, container, !TOM_FUSE);
	if (!Openable (container))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't unlock something that cannot be opened.");
	else if (!db[container].get_lock_key ()->eval (*this, matcher))
		notify_censor_colour(player, player, COLOUR_MESSAGES,"You couldn't unlock %s.", unparse_objectandarticle_inherited (*this, container, ARTICLE_LOWER_DEFINITE));
	else
	{
		db[container].clear_flag(FLAG_LOCKED);
		Modified (container);
		notify_colour (player, player, COLOUR_MESSAGES, "Unlocked.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}
