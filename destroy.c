/* static char SCCSid[] = "@(#)destroy.c	1.39\t10/17/95"; */
/*
 * destroy.c: Routines to allow destruction of database entries.
 *
 *	Peter Crowther, 2/2/90.
 */

#include <string.h>
#include "db.h"
#include "config.h"
#include "interface.h"
#include "lists.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "colour.h"
#include "log.h"

#define RefSet(x)	{if (!db[(x)].get_flag(FLAG_REFERENCED)) {log_bug("%s has REF cleared",unparse_object(god_context,(x)));}}

static	bool			empty_an_object				(dbref, dbref);

static	bool			check_and_destroy_alarm			(context &c, dbref);
static	bool			check_and_destroy_array			(context &c, dbref);
static	bool			check_and_destroy_command		(context &c, dbref);
static	bool			check_and_destroy_dictionary		(context &c, dbref);
static	bool			check_and_destroy_exit			(context &c, dbref);
static	bool			check_and_destroy_fuse			(context &c, dbref);
static	bool			check_and_destroy_player		(context &c, dbref);
static	bool			check_and_destroy_property		(context &c, dbref);
static	bool			check_and_destroy_room			(context &c, dbref);
static	bool			check_and_destroy_thing			(context &c, dbref);
static	bool			check_and_destroy_variable		(context &c, dbref);
static	bool			check_and_destroy_array_element		(context &c, dbref, const CString&);
static	bool			check_and_destroy_dictionary_element	(context &c, dbref, const CString&);
static	void			trash_container				(dbref);
static	void			check_and_make_sane_lock		(dbref object);


/*
 *
 * do_at_empty: Cleans out an array or dictionary
 *
 */

void
context::do_at_empty(
const	CString& name,
const	CString& )

{
	dbref object;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!name)
		object=this->get_player();	
	else
	{
		Matcher matcher(this->get_player(), name, TYPE_NO_TYPE, this->get_effective_id());
		if(this->gagged_command())
			matcher.work_silent();
		matcher.match_everything();
		object=matcher.match_result();
		if(object==NOTHING)
			return;
		if((object!=NOTHING) && (object!=AMBIGUOUS) && (!this->controls_for_write(object)))
		{
			notify_colour(this->get_player(), this->get_player(), COLOUR_ERROR_MESSAGES, permission_denied);
			return;
		}
	}	

	if((Typeof(object) !=TYPE_ARRAY) && (Typeof(object) != TYPE_DICTIONARY))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only empty an Array or Dictionary (see 'help @empty').");
		return;
	}
	db[object].empty_object();

	if(!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Emptied.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}




/*
 * do_at_destroy: Destroy something. This works out what is to be
 *	destroyed and then calls the relevant routine to zap it.
 */

void
context::do_at_destroy (
const	CString& name,
const	CString& dummy)

{
	dbref	object;
	bool	ok;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	/* If nothing specified to destroy, give up */
	if (!name)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You must specify something to destroy.");
		return;
	}
	/* Warn the player if they've got a second argument - have
	   they typoed @desc thing=description? */
        if (dummy)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Do you mean @desc? - Command ignored.");
		return ;
	}
	/* Otherwise, find its dbref */
	Matcher victim_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command())
		victim_matcher.work_silent();	
	victim_matcher.match_everything ();

	/* get result */
	if((object = victim_matcher.noisy_match_result()) == NOTHING)
		return;

	/* If it's me, swear at the player */
	if (object == player)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, no suicide allowed.");
		return;
	}

	/* If the player doesn't control it, give up */
	if (!controls_for_write (object))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	/* Don't destroy if it is set wizard */
	if(Wizard(object) && (Typeof(object) != TYPE_DICTIONARY) && (Typeof(object) != TYPE_ARRAY))
/* A hacky way of doing it I know, but we want to be able to destroy elements */
/* in a wizard set dictionary or array. At least thats what people tell me... */
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, that object is protected.");
		return;
	}	

	/* If it's got any attached commands, give up */
	if (db [object].get_commands() != NOTHING)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't destroy an object with attached commands; @destroy the command(s) first, or use the ashcan flag (see help ashcan and help @gc).");
		return;
	}

	/* Depending on what it is, call the correct function */

	switch (Typeof (object))
	{
		case TYPE_ROOM:
			ok = check_and_destroy_room (*this, object);
			break;
		case TYPE_THING:
			ok = check_and_destroy_thing (*this, object);
			break;
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			ok = check_and_destroy_player (*this, object);
			break;
		case TYPE_EXIT:
			ok = check_and_destroy_exit (*this, object);
			break;
		case TYPE_VARIABLE:
			ok = check_and_destroy_variable (*this, object);
			break;
		case TYPE_PROPERTY:
			ok = check_and_destroy_property (*this, object);
			break;
		case TYPE_DICTIONARY:
			if(victim_matcher.match_index_result())
				ok = check_and_destroy_dictionary_element (*this, object, victim_matcher.match_index_result());
			else if(!(Wizard(object)))
				ok = check_and_destroy_dictionary (*this, object);
			else
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, that object is protected.");
			break;
		case TYPE_ARRAY:
			if(victim_matcher.match_index_result())
				ok = check_and_destroy_array_element (*this, object, victim_matcher.match_index_result());
			else if(!(Wizard(object)))
				ok = check_and_destroy_array (*this, object);
			else
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, that object is protected.");
			break;
		case TYPE_COMMAND:
			if(victim_matcher.match_index_result())
			{
				ok = check_and_destroy_array_element (*this, object, victim_matcher.match_index_result());
			}
			else if(!(Wizard(object)))
				ok = check_and_destroy_command (*this, object);
			else
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, that object is protected.");
			break;
		case TYPE_FUSE:
			ok = check_and_destroy_fuse (*this, object);
			break;
		case TYPE_ALARM:
			ok = check_and_destroy_alarm (*this, object);
			break;
		default:
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "I don't know what type that object is, so I can't destroy it.");
			return;
	}

	if (!ok && (!gagged_command()))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That object was not destroyed.");
	else
	{
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		if (!in_command())
			notify_colour (player, player, COLOUR_MESSAGES, "Destroyed.");
	}
}


static bool
check_and_destroy_room (
context	&c,
dbref	zap_room)

{
	context god_context (GOD_ID);
	dbref	i;
	dbref	temp;
	bool	ok = true;

	/* Zap exit destinations, droptos, and homes for things the player controls */
	for (i = 0; i < db.get_top (); i++)
	{
		switch (Typeof (i))
		{
			case TYPE_EXIT:
				if (db[i].get_destination() == zap_room)
				{
					RefSet (zap_room)
					if (!c.controls_for_write(i))
					{
						notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "%s is linked to that room", unparse_object (c, i));
						ok = false;
					}
					else
					{
						DOLIST (temp, db[i].get_fuses())
							if (!c.controls_for_write (temp))
							{
								notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES,"Fuse %s on exit %s is not owned by you.", getname (temp), unparse_object (c, i));
								ok = false;
							}
					}
				}
				break;
			case TYPE_ROOM:
				if ((db[i].get_parent () == zap_room) && !(c.controls_for_write (i)))
				{
					RefSet (zap_room)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Couldn't unlink child room %s.", unparse_object (c, i));
					ok = false;
				}
				if ((db[i].get_destination() == zap_room) && !(c.controls_for_write (i)))
				{
					RefSet (zap_room)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Couldn't unlink dropto from room %s.", unparse_object (c, i));
					ok = false;
				}
				break;
		}
	}
	if (!ok)
		return (false);

	for (i = 0; i < db.get_top (); i++)
	{
		if ((Typeof (i) == TYPE_ROOM) && (db[i].get_parent () == zap_room))
		{
			RefSet (zap_room)
			notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Room %s reparented.", unparse_object (c, i));
			db [i].set_parent (db [zap_room].get_parent ());
		}

		/* search for all objects set to home here - reset to LIMBO even if they aren't owned by the player. */
		if ((Typeof (i) != TYPE_FREE) && (db[i].get_destination() == zap_room))
		{
			RefSet (zap_room)
			switch (Typeof (i))
			{
				case TYPE_PLAYER:
					notify_colour(c.get_player(), c.get_player(), COLOUR_MESSAGES, "Player %s: Home reset to Limbo.", unparse_object (c, i));
					db[i].set_destination(LIMBO);
					break;
				case TYPE_THING:
					notify_colour(c.get_player(), c.get_player() , COLOUR_MESSAGES, "Thing %s: Home reset to Limbo.", unparse_object(c, i));
					db[i].set_destination(LIMBO);
					break;
				case TYPE_EXIT:
					/* Don't warn if it'll go anyway */
					if (db [i].get_location() != zap_room)
					{
						notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Exit %s: Unlinked.", unparse_object (c, i));
					}
					db[i].set_destination(NOTHING);
					break;
				case TYPE_ROOM:
					notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Room %s: Dropto removed.", unparse_object (c, i));
					db[i].set_destination(NOTHING);
					break;
			}
		}
	}

	/* Send contents home */
	while ((temp = db[zap_room].get_contents()) != NOTHING)
	{
		if (Typeof (temp) == TYPE_ROOM)
		{
			if (!moveto (temp, db[zap_room].get_location()))
			{
				db[zap_room].set_contents (remove_first (temp, db[zap_room].get_contents ()));
				db[temp].set_location (LIMBO);
				PUSH (temp, LIMBO, contents);
			}
		}
		else
		{
			/* Check that the object is not homed to this room */
			if (zap_room == db[temp].get_destination ())
			{
				notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Object %s: Re-homed to Limbo.", unparse_object (c, temp));
				db[temp].set_destination(LIMBO);
			}

			if (!moveto (temp, db[temp].get_destination()))
			{
				db[zap_room].set_contents (remove_first (temp, db[zap_room].get_contents ()));
				db[temp].set_location (LIMBO);
				PUSH (temp, LIMBO, contents);
			}
		}
	}

	db[zap_room].destroy (zap_room);
	return (true);
}


static bool
check_and_destroy_thing (
context	&c,
dbref	zap_thing)

{
	context god_context (GOD_ID);
	bool	ok = true;
	dbref	i;

	/* If it's not empty, give up */
	if (db [zap_thing].get_contents() != NOTHING)
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,  "That object's a container, and it's not empty. Empty it, then destroy it!");
		return (false);
	}

	/* See if any object has this as a key */
	for (i = 0; i < db.get_top (); i++) 
	{
		if (Typeof (i) == TYPE_FREE)
			continue;
		if ((db[i].get_key()->contains (zap_thing))
			|| (Container (i) && (db[i].get_lock_key ()->contains (zap_thing))))
		{
			RefSet (zap_thing)
			notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s is keyed to that object.", unparse_object (c, i));
			ok = false;
		}
		switch (Typeof (i))
		{
			case TYPE_THING:
				if ((db[i].get_parent () == zap_thing) && !(c.controls_for_write (i)))
				{
					RefSet (zap_thing)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Couldn't unlink child thing %s.", unparse_object (c, i));
					ok = false;
				}
				/* FALLTHROUGH */
			case TYPE_EXIT:
			case TYPE_PLAYER:
			case TYPE_ROOM:
				if (db[i].get_destination() == zap_thing)
				{
					RefSet (zap_thing)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s is linked to that thing.", unparse_object (c, i));
					ok = false;
				}
		}
	}

	/* If it's a key, give up */
	if (!ok)
		return (false);

        for (i = 0; i < db.get_top (); i++)
	{
		if ((Typeof (i) == TYPE_THING) && (db[i].get_parent () == zap_thing))
		{
			RefSet (zap_thing)
			notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Thing %s reparented.", unparse_object (c, i));
			db [i].set_parent (db [zap_thing].get_parent ());
		}

                /* search for all objects set to home here - reset to LIMBO even if they aren't owned by the player. */
		if ((Typeof (i) != TYPE_FREE) && (db[i].get_destination() == zap_thing))
		{
			RefSet (zap_thing)
			switch (Typeof (i))
			{
                                case TYPE_PLAYER:
                                        notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Player %s: Home reset to Limbo.", unparse_object (c, i));
                                        db[i].set_destination(LIMBO);
                                        break;
                                case TYPE_THING:
                                        notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Thing %s: Home reset to Limbo.", unparse_object (c, i));
                                        db[i].set_destination(LIMBO);
                                        break;
                                case TYPE_EXIT:
                                        /* Don't warn if it'll die anyway */
                                        if (db [i].get_location() != zap_thing)
                                        {
                                                notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Exit %s: Unlinked.", unparse_object (c, i));
                                        }
                                        db[i].set_destination(NOTHING);
                                        break;
                                case TYPE_ROOM:
                                        notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Room %s: Dropto removed.", unparse_object (c, i));
                                        db[i].set_destination(NOTHING);
                                        break;
                        }
                }
	}

	db[zap_thing].destroy(zap_thing);

	return (true);
}


/*
 * remove_player_from_any_lists_he_is_on(dbref victim, int errors = 1)
 *
 * Well, as it says - remove a player from any lists
 * he happens to be on.
 * if 'errors' is non-zero, then report all inconsitancies
 * otherwise just report major bugs (some inconsistancies could be
 * caused by calling this function from @ashcan)
 */

static void
remove_player_from_any_lists_he_is_on(dbref zap_player, int errors = 1)
{
	unsigned int i;
	/* First, the main player list */

	dbref mylist,hislist;
	int element;
	dbref who;
	char smallbuf[10];
	sprintf(smallbuf,"%d", zap_player);
	int nowarning=1;
	int deadlists=0;

	if ((mylist=find_list_dictionary(zap_player, reverseplist_dictionary)) != NOTHING)
	{
		i=1;
		while(i<=db[mylist].get_number_of_elements())
		{
			who=atoi(db[mylist].get_index(i).c_str());
			if(Typeof(who) != TYPE_PLAYER)
			{
				if(errors)
					log_bug("Attempting to remove a playerlist reference where the target player doesn't exist (in @destroy/@ashcan)");
				i++;
				continue;
			}
			hislist=find_list_dictionary(who, playerlist_dictionary);
			if(hislist == NOTHING)
			{
				if(errors)
					log_bug("Attempting to remove a playerlist reference where the list doesn't exist (in @destroy/@ashcan)");
			}
			else if ((element= db[hislist].exist_element(smallbuf))==0)
			{
				log_bug("Attempting to remove a playerlist reference where none exists (in @destroy)");
			}
			else
			{
				if (nowarning)
				{
					notify_colour(who, who, COLOUR_MESSAGES, "[Player '%s' has just been destroyed, and removed from your main list]", getname(zap_player));
					nowarning=0;
				}
				db[hislist].destroy_element(element);
log_message("Player list updated");
			}
			i++;
		}
	}

	/* Now other people's custom lists */
	int sanity_count;
	char newlist[MAX_LIST_SIZE * 8];
	//char buf[10];
	nowarning=1;
	if ((mylist=find_list_dictionary(zap_player, reverseclist_dictionary)) != NOTHING)
	{
		i=1;
		while(i<=db[mylist].get_number_of_elements())
		{
			who=atoi(db[mylist].get_index(i).c_str());
			if(Typeof(who) != TYPE_PLAYER)
			{
				i++;
				continue;
			}
			hislist=find_list_dictionary(who, list_dictionary);
			if(hislist == NOTHING)
			{
				i++;
				continue;
			}
			sanity_count=atoi(db[mylist].get_element(i).c_str());
			unsigned int j=1;
			deadlists=0;
	/* Now go thru his custom list dictionary, removing me from it */
			while (j<=db[hislist].get_number_of_elements())
			{
				newlist[0]='\0';
				strcpy(scratch_buffer, db[hislist].get_element(j).c_str());
				for (char *number=strtok(scratch_buffer,";"); number; number=strtok(NULL,";"))
				{
					if(atoi(number)==zap_player)
					{
						if (nowarning)
						{
							notify_colour(who, who, COLOUR_MESSAGES, "[Player '%s' has just been destroyed, and removed from your custom lists]", getname(zap_player), db[hislist].get_index(j).c_str());
							nowarning=0;
						}
						sanity_count--;
					}
					else
					{
						strcat(newlist,number);
						strcat(newlist,";");
					}
				}
				if (*newlist)
				{
					newlist[strlen(newlist)-1]='\0';
					db[hislist].set_element(j, NULLCSTRING, newlist);
					j++;
log_message("Custom list updated");
				}
				else
				{
					db[hislist].destroy_element(j); 
					deadlists++;
log_message("Custom list destroyed");
				}
			}
			if (deadlists>0)
				notify_colour(who,who,COLOUR_MESSAGES, "(%d empty custom list%sbeen deleted because of this)", deadlists, (deadlists==1) ? " has ":"s have ");
			if (sanity_count !=0)
				log_bug("Sanity count for reverse lists incorrect in @destroy. Out by %d", sanity_count);

			i++;
		}
		
	}
}

/*
 * do_destroy_player: checks for locks against the player
 * renamed to check_and_destroy_player() at some point, spotted during tidyup
 * JPK
 */

static bool
check_and_destroy_player (
context	&c,
dbref	zap_player)

{
	context god_context (GOD_ID);
	dbref	i;
	bool	ok = true;

	for (i = 0; i < db.get_top (); i++)
		if (Typeof (i) != TYPE_FREE)
		{
			if ((i != zap_player) && db[i].get_key()->contains (zap_player))
			{
				RefSet (zap_player)
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Player is part of key to %s.", unparse_object (c, i));
				ok = false;
			}
			if (Container (i) && db[i].get_lock_key ()->contains (zap_player))
			{
				RefSet (zap_player)
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That player is part of lock key for container %s.", unparse_object (c, i));
				ok = false;
			}
			if ((i != zap_player) && (Typeof (i) == TYPE_PLAYER) && (db[i].get_controller () == zap_player))
			{
				RefSet (zap_player)
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That player is a controller of puppet %s.\n", unparse_object (c, i));
				ok = false;
			}
			if ((Typeof (i) == TYPE_PLAYER) && (db[i].get_parent () == zap_player))
			{
				RefSet (zap_player)
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That player is the parent of player %s.", unparse_object (c, i));
				ok = false;
			}
			if (db[i].get_build_id() == zap_player)
			{
				if(i != zap_player)
				{
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,"%s is still building under that player", unparse_object (c, i));
					ok = false;
				}
			}
		}
	if (Connected (zap_player))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,  "That player is still connected... Boot the bugger first.");
		ok = false;
	}

	if (!empty_an_object(c.get_player (), zap_player))
		ok = false;

	if (!ok)
		return (false);

	/* If this player is on anybody's lists, remove him. */

	remove_player_from_any_lists_he_is_on(zap_player);

	/* Transfer any money the poor soul might have had to the destroyer. */
	db[c.get_player()].set_money(db[c.get_player()].get_money()+db[zap_player].get_money());


	for (i = 0; i < db.get_top (); i++)
		if (Typeof (i) != TYPE_FREE)
		{
			if ((db[i].get_owner() == zap_player) && (i != zap_player))
			{
				RefSet (zap_player)
				db[i].set_owner(c.get_player ());
				notify_colour(c.get_player(), c.get_player(), COLOUR_MESSAGES, "WARNING: Changing owner of object #%d to you.", i);
			}
		}

	db.remove_player_from_cache(db[zap_player].get_name());

#ifdef ALIASES
	for(int alias=0; alias<MAX_ALIASES; alias++)
		if(db[zap_player].get_alias(alias))
			db.remove_player_from_cache(db[zap_player].get_alias(alias));
#endif

	db[zap_player].destroy(zap_player);
	return (true);
}


/*
 * do_destroy_exit: Zap the exit.
 * renamed check_and_destroy_exit() at some point, spotted during tidyup JPK
 */

static bool
check_and_destroy_exit (
context	&c,
dbref	zap_exit)

{
	context god_context (GOD_ID);
	dbref i;

	for (i = 0; i < db.get_top(); i++)
		if ((Typeof(i) == TYPE_EXIT) && (db[i].get_parent() == zap_exit))
		{
			RefSet (zap_exit)
			notify_colour(c.get_player(), c.get_player(), COLOUR_MESSAGES, "Exit %s reparented.", unparse_object(c, i));
			db[i].set_parent(db[zap_exit].get_parent());
		}
	db[zap_exit].destroy(zap_exit);
	return (true);
}


static bool
check_and_destroy_command (
context	&c,
dbref	zap_command)

{
	context god_context (GOD_ID);
	dbref	i;
	bool	ok = true;

	for (i = 0; i < db.get_top (); i++)
	{
		switch (Typeof (i))
		{
			case TYPE_PLAYER:
			case TYPE_ROOM:
			case TYPE_EXIT:
			case TYPE_VARIABLE:
			case TYPE_FREE:
				break;
			case TYPE_COMMAND:
				if ((db[i].get_csucc () == zap_command) || (db[i].get_cfail () == zap_command))
				{
					RefSet (zap_command)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s uses this command in it's csucc or cfail.", unparse_object (c, i));
					ok = false;
				}
				if (db[i].get_parent () == zap_command)
				{
					RefSet (zap_command)
					notify_colour(c.get_player (), c.get_player(), COLOUR_MESSAGES, "Command %s reparented.", unparse_object (c, i));
					db [i].set_parent (db [zap_command].get_parent ());
				}
				break;
			case TYPE_THING:
				if (db[i].get_lock_key ()->contains (zap_command))
				{
					RefSet (zap_command)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s uses this command in its key.", unparse_object (c, i));
					ok = false;
				}
				break;
			case TYPE_FUSE:
				if ((db[i].get_csucc () == zap_command) || (db[i].get_cfail () == zap_command))
				{
					RefSet (zap_command)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s uses this command.", unparse_object (c, i));
					ok = false;
				}
				break;
			case TYPE_ALARM:
				if (db[i].get_csucc () == zap_command)
				{
					RefSet (zap_command)
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s uses this command.", unparse_object (c, i));
					ok = false;
				}
				break;
		}
		if ((Typeof (i) != TYPE_FREE) && (db[i].get_key()->contains (zap_command)))
		{
			RefSet (zap_command)
			notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "%s uses this command in its lock.", unparse_object (c, i));
			ok = false;
		}
	}
	if (!ok)
		return (false);

	db[zap_command].destroy(zap_command);
	return (true);
}


static bool
check_and_destroy_variable (
context	&c,
dbref	zap_variable)

{
	db[zap_variable].destroy(zap_variable);
	return (true);
}

static bool
check_and_destroy_array (
context	&c,
dbref	zap_array)

{
	db[zap_array].destroy (zap_array);
	return (true);
}

static bool
check_and_destroy_array_element (
context		&c,
dbref		zap_array,
const	CString&	elem)

{
	int	temp = atoi (elem.c_str());

	if(db[zap_array].exist_element(temp) == 0)
	{
		notify_colour(c.get_player (), c.get_player(),
			COLOUR_ERROR_MESSAGES,
			"Array \"%s\" only has %d elements.",
			db[zap_array].get_name().c_str(),
			db[zap_array].get_number_of_elements());
		return (false);
	}

	db[zap_array].destroy_element(temp);
	if(Typeof(zap_array) == TYPE_COMMAND)
	{
		db[zap_array].flush_parse_helper();
	}
	return (true);
}

static bool
check_and_destroy_dictionary (
context	&c,
dbref	zap_dictionary)

{
	db[zap_dictionary].destroy (zap_dictionary);
	return (true);
}

static bool
check_and_destroy_dictionary_element (
context		&c,
dbref		zap_dictionary,
const	CString&	elem)

{
	int temp;

	if((temp = db[zap_dictionary].exist_element(elem)) == 0)
	{
		if (!c.gagged_command())
			notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Dictionary \"%s\" does not contain element \"%s\"", db[zap_dictionary].get_name().c_str(), elem.c_str());
		return (false);
	}

	db[zap_dictionary].destroy_element(temp);
	return (true);
}

static bool
check_and_destroy_property (
context	&c,
dbref	zap_property)

{
	db[zap_property].destroy(zap_property);
	return (true);
}

static bool
check_and_destroy_fuse (
context	&c,
dbref	zap_fuse)

{
	context god_context (GOD_ID);
	dbref i;

	for (i = 0; i < db.get_top(); i++)
		if ((Typeof(i) == TYPE_FUSE) && (db[i].get_parent() == zap_fuse))
		{
			RefSet (zap_fuse)
			notify_colour(c.get_player(), c.get_player(), COLOUR_MESSAGES, "Fuse %s reparented.", unparse_object(c, i));
			db[i].set_parent(db[zap_fuse].get_parent());
		}
	db[zap_fuse].destroy(zap_fuse);
	return (true);
}


static bool
check_and_destroy_alarm (
context	&c,
dbref	zap_alarm)
{
	context god_context (GOD_ID);
	dbref i;

	for (i = 0; i < db.get_top(); i++)
		if ((Typeof(i) == TYPE_ALARM) && (db[i].get_parent() == zap_alarm))
		{
			RefSet (zap_alarm)
			notify_colour(c.get_player(), c.get_player(), COLOUR_MESSAGES, "Alarm %s reparented.", unparse_object(c, i));
			db[i].set_parent(db[zap_alarm].get_parent());
		}
	db[zap_alarm].destroy(zap_alarm);
	return (true);
}

void
context::do_at_garbage_collect (
const	CString& type,
const	CString& )

{
	int	kill_players = 0;
	
	dbref	i;
	dbref	object;
	dbref	temp;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if (!Wizard (player))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}
	if (in_command())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't do that in a command.");
		return;
	}
	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;

	if(!string_compare(type, "players"))
		kill_players = 1;

/*	This is the biggie. BUT it is probably faster than the other 
	'normal' destroy stuff. (And this might actually work!).
	First, go through the db, just zapping ANYTHING with a ashcan
	flag set, no constraints, no checking. Then, go through again,
	anything referencing a free object, set to relevent point.
	Cool or wot!.
*/

	int	commands = 0;
	int	acommands = 0;
	int	things = 0;
	int	athings = 0;
	int	rooms = 0;
	int	arooms = 0;
	int	exits = 0;
	int	aexits = 0;

	int	puppets = 0;
	int	apuppets = 0;
	int	players = 0;
	int	aplayers = 0;

	int	properties = 0;
	int	aproperties = 0;
	int	arrays = 0;
	int	aarrays = 0;
	int	dictionaries = 0;
	int	adictionaries = 0;
	int	variables = 0;
	int	avariables = 0;

	int	fuses = 0;
	int	afuses = 0;
	int	alarms = 0;
	int	aalarms = 0;
	
	int	frees = 0;

	/*
	 * Pre-pass, what we are going to do is to set anything
	   that an ashcan player owns to ashcan and allow the
	   @gc to happen normally. Then, once we have finished
	   we will remove all the players in another pass. Not
	   really efficient but frankly, no-one cares unless
	   you are being really pedantic about it
   	 */
	for (i = 0; i < db.get_top (); i++)
	{
		if(Typeof(i) == TYPE_FREE)
			continue;
		if (Ashcan (i))
		{
			switch(Typeof(i))
			{
				case TYPE_THING:
					athings++;
					break;
				case TYPE_ROOM:
					arooms++;
					break;
				case TYPE_EXIT:
					aexits++;
					break;
				case TYPE_COMMAND:
					acommands++;
					break;
				case TYPE_VARIABLE:
					avariables++;
					break;
				case TYPE_PROPERTY:
					aproperties++;
					break;
				case TYPE_ARRAY:
					aarrays++;
					break;
				case TYPE_FUSE:
					afuses++;
					break;
				case TYPE_ALARM:
					aalarms++;
					break;
				case TYPE_PUPPET:
					apuppets++;
					break;
				case TYPE_PLAYER:
					aplayers++;
					if(kill_players)
						remove_player_from_any_lists_he_is_on(i);
					break;
				default:
					break;
			}
		}
		else
		{
			if(kill_players)
				// If the owner is set ashcan or the owner of the owner is set ashcan (NPC's)
				if(Ashcan(db[i].get_owner()) || Ashcan(db[db[i].get_owner()].get_owner()))
					db[i].set_flag(FLAG_ASHCAN);
		}
   	 }

	/*
	 * First pass: Move any non-ashcan objects out of the way.
	 * 		And count to keep track
	 */
	for (i = 0; i < db.get_top (); i++)
	{
		switch(Typeof(i))
		{
			/*Not that this case has a continue not
			  a break */
			case TYPE_FREE:
				frees++;
				continue;
			case TYPE_THING:
				things++;
				break;
			case TYPE_ROOM:
				rooms++;
				break;
			case TYPE_EXIT:
				exits++;
				break;
			case TYPE_COMMAND:
				commands++;
				break;
			case TYPE_VARIABLE:
				variables++;
				break;
			case TYPE_PROPERTY:
				properties++;
				break;
			case TYPE_ARRAY:
				arrays++;
				break;
			case TYPE_FUSE:
				fuses++;
				break;
			case TYPE_ALARM:
				alarms++;
				break;
			case TYPE_PUPPET:
				puppets++;
				break;
			case TYPE_PLAYER:
				players++;
				break;
			default:
				break;
		}


		if(Ashcan(i))
		{
			switch (Typeof (i))
			{
				case TYPE_PLAYER:
					if(!kill_players)
						continue;	// If we're not killing players, don't mess with them.
					// FALLTHROUGH!
				case TYPE_PUPPET:
				case TYPE_THING:
				case TYPE_ROOM:
					/* Go through the contents of the container, sending them home. */
					temp = db[i].get_contents();
					while (temp != NOTHING)
					{
						object = temp;
						temp = db[temp].get_next();
						switch (Typeof (object))
						{
							case TYPE_PLAYER:
							case TYPE_PUPPET:
								/* check to see if the home of the player is ashcan now. */
								if (!Ashcan (db[object].get_destination()) || (kill_players == 1 && !Ashcan (db[db[object].get_destination()].get_owner())))
									moveto (object, db[object].get_destination());
								else
								{
									moveto (object, LIMBO);
									db[object].set_destination(LIMBO);
								}
								break;
							case TYPE_THING:
								moveto (object, db[object].get_destination());
								break;
							case TYPE_ROOM:
								moveto (object, db[i].get_location());
								break;
							default:
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Contents List of %d is wrong.", i);
								break;
						}
					}
					break;
				default:
					break;
			}
		}
	}
		/*
		 * Pass 2: We've got rid of all the stuff we don't want to destroy.
		 *	Blast everything else now.
		 */
		for (i = 0; i < db.get_top (); i++)
		{
			if ((Typeof (i) != TYPE_FREE) && (Ashcan (i) || (kill_players == 1 && Ashcan(db[i].get_owner()))))
			{
				switch (Typeof	(i))
				{
					case TYPE_PLAYER:
					case TYPE_PUPPET:
						/*We're saving the best for last*/
						break;
					case TYPE_ROOM:
					case TYPE_THING:
						trash_container (i);
						break;
					case TYPE_ALARM:
					case TYPE_ARRAY:
					case TYPE_COMMAND:
					case TYPE_DICTIONARY:
					case TYPE_EXIT:
					case TYPE_FUSE:
					case TYPE_PROPERTY:
					case TYPE_VARIABLE:
					case TYPE_WEAPON:
					case TYPE_ARMOUR:
					case TYPE_AMMUNITION:
						db [i].destroy (i);
						break;
					default:
	//					Trace( "BUG: Unknown object type %d at %d with ASHCAN flag set.\n", db [i].get_flags (), i);
						break;
				}
			}
		}

	/*
	 * Pass 2a : Delete players & puppets, now that all their stuff has gone if we are killing players
	 */
	if(kill_players)
	{
		for (i = 0; i < db.get_top (); i++)
		{
			if ((Typeof (i) != TYPE_FREE) && (Ashcan (i)))
			{
				switch (Typeof	(i))
				{
					case TYPE_PLAYER:
						if(Connected(i))
							break;
						remove_player_from_any_lists_he_is_on(i, 0);
						/*Fallthrough*/
					case TYPE_PUPPET:
						/* Dinner time */
							/* Try to keep building points in the game 'level' */
							db[player].add_pennies(db[i].get_pennies());
							trash_container (i);
						break;
					default:
						/* You really shouldn't be here */
						break;
				}
			}
		}
	}
	/*
	 * Pass 3: Check for inconsistencies.
	 */
	for (i = 0; i < db.get_top (); i++)
	{
		switch(Typeof(i))
		{
			case TYPE_FREE:
				break;
			case TYPE_THING:
				things--;
				break;
			case TYPE_ROOM:
				rooms--;
				break;
			case TYPE_EXIT:
				exits--;
				break;
			case TYPE_COMMAND:
				commands--;
				break;
			case TYPE_VARIABLE:
				variables--;
				break;
			case TYPE_PROPERTY:
				properties--;
				break;
			case TYPE_ARRAY:
				arrays--;
				break;
			case TYPE_FUSE:
				fuses--;
				break;
			case TYPE_ALARM:
				alarms--;
				break;
			case TYPE_PUPPET:
				puppets--;
				break;
			case TYPE_PLAYER:
				players--;
				break;
			default:
				break;
		}
		switch (Typeof (i))
		{
			case TYPE_ROOM:
				check_and_make_sane_lock (i);
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				if (db[i].get_destination() != NOTHING
					&& (db[i].get_destination() != HOME)
					&& (Typeof (db[i].get_destination()) != TYPE_ROOM)
					&& (Typeof (db[i].get_destination()) != TYPE_THING))
					db[i].set_destination(NOTHING);
				temp = db[i].get_contents();
				while (temp != NOTHING)
				{
					object = temp;
					temp = db[temp].get_next();
					switch (Typeof (object))
					{
						case TYPE_THING:
						case TYPE_PLAYER:
						case TYPE_PUPPET:
							break;
						case TYPE_ROOM:
							if (db[object].get_location() != i)
								moveto (object, LIMBO);
							break;
						default:
							log_bug("Room #%d has illegal object type on its contents list (object #%d)", i, object);
							break;
					}
				}
				break;
			case TYPE_THING:
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				if ((db [i].get_location () == NOTHING)
					|| ((Typeof (db[i].get_location()) != TYPE_ROOM)
						&& (Typeof (db[i].get_location()) != TYPE_THING)
						&& (Typeof (db[i].get_location()) != TYPE_PLAYER)))
					moveto (i, LIMBO);
				check_and_make_sane_lock (i);
				if ((db[i].get_destination() == NOTHING)
					|| (db[i].get_destination() == HOME)
					|| ((Typeof (db[i].get_destination()) != TYPE_ROOM)
						&& (Typeof (db[i].get_destination()) != TYPE_PLAYER)
						&& (Typeof (db[i].get_destination()) != TYPE_THING)))
					db[i].set_destination(LIMBO);
				break;
			case TYPE_EXIT:
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				if ((db [i].get_location () == NOTHING)
					|| ((Typeof (db[i].get_location()) != TYPE_ROOM)
						&& (Typeof (db[i].get_location()) != TYPE_THING)
						&& (Typeof (db[i].get_location()) != TYPE_PLAYER)))
				{
					db [i].destroy (i);
					break;
				}
				check_and_make_sane_lock (i);
				if (db[i].get_destination() != NOTHING
					&& (db[i].get_destination() != HOME)
					&& (Typeof (db[i].get_destination()) != TYPE_ROOM)
					&& (Typeof (db[i].get_destination()) != TYPE_THING))
				{
					db [i].destroy (i);
				}
				break;
			case TYPE_PLAYER:
			case TYPE_PUPPET:
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				if ((db [i].get_location () == NOTHING)
					|| ((Typeof (db[i].get_location()) != TYPE_ROOM)
						&& (Typeof (db[i].get_location()) != TYPE_THING)))
					moveto (i, LIMBO);
				if ((db[i].get_destination() == NOTHING)
					|| (db[i].get_destination() == HOME)
					|| ((Typeof (db[i].get_destination()) != TYPE_ROOM)
						&& (Typeof (db[i].get_destination()) != TYPE_THING)))
					db[i].set_destination(LIMBO);
				check_and_make_sane_lock (i);
				break;
			case TYPE_COMMAND:
			case TYPE_FUSE:
			case TYPE_ALARM:
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				check_and_make_sane_lock (i);
				/* CSuccess link. */
				if ((db[i].get_csucc() != NOTHING)
					&& (db[i].get_csucc() != HOME)
					&& (Typeof (db[i].get_csucc()) != TYPE_COMMAND))
					db[i].set_csucc(NOTHING);
				/* CFail link. */
				if ((db[i].get_cfail() != NOTHING)
					&& (db[i].get_cfail() != HOME)
					&& (Typeof (db[i].get_cfail()) != TYPE_COMMAND))
					db[i].set_cfail(NOTHING);
				break;
			case TYPE_VARIABLE:
				check_and_make_sane_lock (i);
				break;
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
				if (db[i].get_parent() != NOTHING && Typeof(db[i].get_parent()) == TYPE_FREE)
					db[i].set_parent(NOTHING);
				break;
			case TYPE_PROPERTY:
			case TYPE_FREE:
				break;
			default:
				/* Got an unknown type here. Oops. */
				break;
		}
	}

	notify_colour(player, player, COLOUR_CONTENTS, "Analysis of refuse :-");
	notify_colour(player, player, COLOUR_MESSAGES, "

  Type:          Ashcanned  Obliterated
     Rooms           %-5d      %-5d
     Things          %-5d      %-5d
     Exits           %-5d      %-5d
     Commands        %-5d      %-5d

     Fuses           %-5d      %-5d
     Alarms          %-5d      %-5d

     Properties      %-5d      %-5d
     Arrays          %-5d      %-5d
     Dictionaries    %-5d      %-5d
     Variables       %-5d      %-5d

     Players         %-5d      %-5d
     Puppets         %-5d      %-5d
     ------------------------------------------
     Total           %-5d      %-5d",
		     arooms,rooms, 
		     athings,things, 
		     aexits, exits, 
		     acommands,commands, 

		     afuses, fuses, 
		     aalarms,alarms, 

		     aproperties,properties, 
		     aarrays,arrays, 
		     adictionaries,dictionaries, 
		     avariables,variables, 
		     aplayers,players,
		     apuppets,puppets,

(arooms+athings+aexits+acommands+afuses+aalarms+aproperties+aarrays+adictionaries+avariables+aplayers+apuppets),
(rooms+things+exits+commands+fuses+alarms+properties+arrays+dictionaries+variables+players+puppets));

}


static void
trash_container (
dbref	i)

{
	dbref	object;

	/* The destroy() call loses everything except the Contents list. */
	while (db[i].get_contents() != NOTHING)
	{
		object = db[i].get_contents();
		switch (Typeof (object))
		{
			case TYPE_PLAYER:
			case TYPE_PUPPET:
				moveto (object, LIMBO);
				break;
			case TYPE_ROOM:
				trash_container (object);
				break;
			case TYPE_THING:
				trash_container (object);
				break;
			default:
				/* Just to make things more secure */
				moveto (object,LIMBO);
				break;
		}
	}

	if(Typeof(i)==TYPE_PLAYER)
	{
		db.remove_player_from_cache(db[i].get_name());
#ifdef ALIASES
		for(int alias=0; alias<MAX_ALIASES; alias++)
			if(db[i].get_alias(alias))
				db.remove_player_from_cache(db[i].get_alias(alias));
#endif
	}

	db [i].destroy (i);
}


static void
check_and_make_sane_lock (
dbref	object)

{
	if (db[object].get_key() != TRUE_BOOLEXP)
		db[object].set_key_no_free (db[object].get_key_for_edit()->sanitise ());
	if (Typeof(object) == TYPE_THING)
		if (db[object].get_lock_key () != TRUE_BOOLEXP)
			db[object].set_lock_key_no_free (db[object].get_lock_key_for_edit ()->sanitise ());
}


static bool
empty_an_object(dbref player, dbref container)
{
	while (db[container].get_contents() != NOTHING)
	{
		if (db[db[container].get_contents()].get_destination() == container)
			db[db[container].get_contents()].set_destination(player);
		moveto (db[container].get_contents(), db[db[container].get_contents()].get_destination());
	}
	return(true);
}
