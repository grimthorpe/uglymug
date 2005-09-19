#include "copyright.h"

#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include "db.h"
#include "colour.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "regexp_interface.h"
#include "context.h"


#define	CONTENTS_TAB	2

static char *tiny_time_string(time_t interval);

/*
 * look_container: We know the thing's a container, and have already printed out
 *	its top-level name.
 *
 * This routine prints:
 *	- its 'content-string' field, or the value of contents_name if it has none,
 *	- its contents (recursively).
 */


/** Yuck! Sometime I'll get around to changing this - PJC 23/12/96 **/
// If you hit this limit then just increase the table below
#define MAX_LEVELS 13
const char *const spacestring[] = 
{
	"",
	"  ",
	"    ",
	"      ",
	"        ",
	"          ",
	"            ",
	"              ",
	"                ",
	"                  ",
	"                    ",
	"                      ",
	"                        ",
	"                          "
};

const char* const
spaces(unsigned int level)
{
static char spc[1024];	// 512 levels of indentation...
	if(level < MAX_LEVELS)
	{
		return spacestring[level];
	}
	if(level >= (sizeof(spc) / 2))
	{
		level = (sizeof(spc) / 2) - 1;
	}
	memset((void*)spc, ' ', level*2);
	spc[level*2] = 0;

	return spc;
}

static void
output_command(
dbref	command,
dbref	player)
{
	int		tab_level = 0;
	unsigned	line = 1;
	bool		force_outdent=false;
	bool		linenumbers = Linenumbers(player);
	Command_next	what;
	const	char	*text;

	while(line <= (unsigned)db[command].get_inherited_number_of_elements())
	{
		text = db[command].get_inherited_element(line).c_str();
		what = what_is_next(text);

		switch(what)
		{
			case ELSE_NEXT:
			case ENDIF_NEXT:
			case ELSEIF_NEXT:
			case END_NEXT:
				tab_level--;
				break;
			default:
				break;
		}


		if(tab_level < 0)
			tab_level = 0;

		unsigned block_end=line+db[command].inherited_lines_in_cmd_blk(line);
		for(;line<block_end;line++)
		{
			if (linenumbers)
				notify_censor(player, player, "%%g[%3u]%%z %s%s", line, spaces(tab_level), db[command].get_inherited_element(line).c_str());
			else
				notify_censor(player, player, "%s%s", spaces(tab_level), db[command].get_inherited_element(line).c_str());
		}

		if (force_outdent)
		{
			force_outdent = false;
			tab_level--;
		}

		switch(what)
		{
			case IF_NEXT:
			case ELSE_NEXT:
			case ELSEIF_NEXT:
			case WITH_NEXT:
			case FOR_NEXT:
				tab_level++;
				break;
			default:
				break;
		}
	}
}


static void
look_container (
context		&c,
dbref		object,
const	char	*contents_name,
int		level)

{
	dbref	thing;
	int	had_contents;
	int	level_count;
	/* If it's closed AND opaque, we can't see its contents. */
	if ((!Open (object)) && (Opaque (object)))
		return;

	/* Write the correct number of spaces into the start of scratch_buffer */
	for (level_count = 0; level_count < level; level_count++)
		scratch_buffer [level_count] = ' ';

	had_contents = 0;

	/* ... and dump a list of the contents. */
	DOLIST (thing, db[object].get_contents())
	{
		if (!Dark(thing) || (db[thing].get_owner() != db[object].get_owner()))
		{
			if (!had_contents)
			{
				/* Otherwise, output the 'contents' field (if any)... */
				if (db[object].get_contents_string ())
					strcpy (scratch_buffer + level, db [object].get_contents_string ().c_str());
				else
					strcpy (scratch_buffer + level, contents_name);
				notify_public_colour(c.get_player (), c.get_player(), COLOUR_CONTENTS, "%s", scratch_buffer);
				had_contents = 1;
			}
			sprintf (scratch_buffer + level, unparse_objectandarticle_inherited(c, thing, ARTICLE_UPPER_INDEFINITE));
			notify_public (c.get_player(), c.get_player (), "%s", scratch_buffer);
			if (Container (thing))
			{
				if (db[thing].get_contents() != NOTHING)
				{
					look_container (c, thing, "Contents:", level + CONTENTS_TAB);
				}
			}
		}
	}
}

/*
 * examine_container: We know the thing's a container, and have already printed out
 *	its top-level name.
 *
 * This routine prints:
 *	- its 'content-string' field, or the value of contents_name if it has none,
 *	- its contents (recursively).
 */

static void
examine_container (
context		&c,
dbref		object,
int		level)

{
	dbref	thing;
	int	level_count;
	const colour_at& ca=db[c.get_player()].get_colour_at();

	/* If it's opaque, closed, and you don't control it, you can't see in. */
	if (!(((Open (object)) || (Opaque (object))) || c.controls_for_read (object)))
	{
		for (level_count = 0; level_count < level; level_count++)
			scratch_buffer [level_count] = ' ';

		sprintf (scratch_buffer + level_count, "%sYou cannot see inside it%s",
							ca[COLOUR_ERROR_MESSAGES],
							COLOUR_REVERT);
		notify (c.get_player (), scratch_buffer);
		return;
	}

	/* output the key for the container */
	for (level_count = 0; level_count < level; level_count++)
		scratch_buffer [level_count] = ' ';
	sprintf (scratch_buffer + level_count, "%sLock Key:%s %s", 
			ca[COLOUR_TITLES],
			COLOUR_REVERT,
			db[object].get_lock_key ()->unparse (c));

	notify(c.get_player (), "%s", scratch_buffer);

	/* Otherwise, output the 'contents' field (if any). Re-use the spaces from last time. */
	if (db [object].get_contents_string())
	{
		sprintf(scratch_buffer + level_count, "%sContents string:%s ",
				ca[COLOUR_CONTENTS],
				COLOUR_REVERT);
		strcat (scratch_buffer + level_count, db [object].get_contents_string ().c_str());
	}
	else
		sprintf (scratch_buffer + level_count, "%sContents:%s",
					ca[COLOUR_CONTENTS],
					COLOUR_REVERT);

	notify_public (c.get_player(), c.get_player (), "%s", scratch_buffer);

	/* Write the correct number of spaces into the start of scratch_buffer */
	for (level_count = 0; level_count < (level + CONTENTS_TAB); level_count++)
		scratch_buffer [level_count] = ' ';
	scratch_buffer[level_count] = 0;

	/* ... and dump a list of the contents. */
	DOLIST (thing, db[object].get_contents())
	{
		sprintf (scratch_buffer + level_count, "%s%s", (Connected (thing) ? "*" : ""), unparse_object_inherited(c, thing));
		notify_public (c.get_player(), c.get_player (), "%s", scratch_buffer);
		if (Container (thing))
			if(db[thing].get_contents() != NOTHING)
				examine_container (c, thing, level + CONTENTS_TAB);
	}
}


char *
underline (
int	n)

{ 
	static char under[BUFFER_LEN*2] ;
	int i ;

	if (n>=(BUFFER_LEN*2))
		n= BUFFER_LEN*2-1 ;
	for ( i= 0 ; i<n ; i++)
	  	under[i]='~' ;
	under[n]= '\0' ;
	return under ;
}


static void
look_contents (
context		&c,
dbref		loc,
int		prettyprint_is_on,
const	char	*contents_name)

{
	const colour_at& ca = db[c.get_player()].get_colour_at();
	dbref thing;
	dbref can_see_loc;
	int had_it;

	/* check to see if he can see the location */
	can_see_loc = (!Dark(loc) || c.controls_for_read (loc));

	/* check to see if there is anything there */
	DOLIST(thing, db[loc].get_contents())
	{
		if(can_see(c, thing, can_see_loc))
		{
			/* something exists!  show him everything */
			had_it = 0;
			DOLIST(thing, db[loc].get_contents())
			{
				if ((can_see (c, thing, can_see_loc)) && (Typeof (thing) != TYPE_ROOM))
				{
					if (!had_it)
					{
						notify_public(c.get_player(), c.get_player (), "%s%s%s", ca[COLOUR_CONTENTS], contents_name, COLOUR_REVERT);
						had_it = 1;
						if (prettyprint_is_on)
							notify(c.get_player(), "%s%s%s",ca[COLOUR_UNDERLINES], underline(colour_strlen(contents_name)), COLOUR_REVERT) ;
					}
					if((Typeof(thing) != TYPE_PLAYER) || c.controls_for_read(loc) || Connected (thing))
					{
						switch(Typeof(thing))
						{
							case TYPE_THING:
								notify_public(c.get_player(), c.get_player (), "  %s%s%s", ca[COLOUR_THINGS], unparse_objectandarticle_inherited(c, thing, ARTICLE_UPPER_INDEFINITE), COLOUR_REVERT);
								break;
							case TYPE_PLAYER:
								notify_public(c.get_player(), c.get_player (), "  %s%s%s%s",  (Connected (thing) ? "*" : ""), ca[rank_colour(thing)], unparse_objectandarticle_inherited(c, thing, ARTICLE_UPPER_INDEFINITE), COLOUR_REVERT);
								break;
							case TYPE_PUPPET:
								notify_public(c.get_player(), c.get_player (), "  %s%s%s", ca[COLOUR_PLAYERS], Connected(thing)?"+":"", unparse_objectandarticle_inherited(c, thing, ARTICLE_UPPER_INDEFINITE), COLOUR_REVERT);
								break;

							default:
								notify_public(c.get_player(), c.get_player (), "  %s%s",  (Connected (thing) ? "*" : ""), unparse_objectandarticle_inherited(c, thing, ARTICLE_UPPER_INDEFINITE));
								break;
						}
					}
				}
			}
			break;		/* we're done */
		}
	}
}

static void
look_simple (
context	&c,
dbref	thing)

{
	dbref	looper;
	ColourAttribute	colour;
	const colour_at& ca=db[c.get_player()].get_colour_at();

	switch (Typeof(thing))
	{
		case TYPE_ROOM:
			colour= COLOUR_ROOMDESCRIPTION;
			break;
		case TYPE_PLAYER:
			colour= COLOUR_PLAYERDESCRIPTION;
			break;
		case TYPE_THING:
			colour= COLOUR_THINGDESCRIPTION;
			break;
		default:
			colour= COLOUR_DESCRIPTIONS;
			break;
	}

	
	if(db[thing].get_inherited_description())
	{
		notify_public_colour(c.get_player (), c.get_player(), colour, "%s%s", db[thing].get_inherited_description().c_str(),COLOUR_REVERT);
		DOLIST(looper, db[thing].get_properties())	/* Visible Properties are displayed as <name>: <desc> */
		{
			if (Visible(looper))
				notify_censor(c.get_player(), c.get_player(), "%s%s: %s%s%s", 
				ca[COLOUR_TITLES], db[looper].get_name().c_str(), 
				ca[COLOUR_PROPERTIES],db[looper].get_description().c_str(), COLOUR_REVERT);
		}
		if (Container(thing) && (db[thing].get_contents() != NOTHING))
			look_container (c, thing, "Contents:", 1);
	}
	else
	{
		notify_colour(c.get_player (), c.get_player(), colour,  "You see nothing special about it.");
		DOLIST(looper, db[thing].get_properties())	/* Visible Properties are displayed as <name>: <desc> */
		{
			if (Visible(looper))
				notify_censor(c.get_player(), c.get_player(), "%s%s: %s%s%s", 
				ca[COLOUR_TITLES], db[looper].get_name().c_str(), 
				ca[COLOUR_PROPERTIES],db[looper].get_description().c_str(), COLOUR_REVERT);
		}
		if (Container(thing) && (db[thing].get_contents() != NOTHING))
			look_container (c, thing, "But it contains:", 1);
	}
}


/*
 * look_exits: Looks at exits from loc. If player controls the exit,
 *	or if the exit is !DARK, outputs a message of the form:
 *	<name> leads to <destination> (if linked), or
 *	<name> is unlinked (if unlinked and the player controls it).
 */

static void
look_exits (
context	&c,
dbref	loc,
int 	prettylook_is_on
)

{
	dbref	exit;
	char	namebuf [BUFFER_LEN];
	char	*name_end;
	int	seen = 0;
	const colour_at& ca = db[c.get_player()].get_colour_at();
	while (loc != NOTHING)
	{
		DOLIST (exit, db[loc].get_exits())
		{
			if ((!Dark (exit))
				&& (c.controls_for_read (exit)
					|| (db[exit].get_destination() == HOME)
					|| ((db[exit].get_destination() != NOTHING
						&& ((Typeof (db[exit].get_destination()) == TYPE_ROOM) || (Typeof (db[exit].get_destination()) == TYPE_THING))))))
			{
				/* Zap all but the first option off the name */
				strcpy (namebuf, getarticle (exit, ARTICLE_UPPER_INDEFINITE));
				strcat (namebuf, getname_inherited (exit));
				if ((name_end = strchr (namebuf, ';')) != NULL)
					*name_end = '\0';

				/* If it's the first one we've seen, dump a header */
				if (!seen)
				{
					seen = 1;
					if(db[loc].get_exits_string())
						notify_colour(c.get_player(), c.get_player(), COLOUR_CONTENTS, "%s", db[loc].get_exits_string().c_str());
					else
						notify_colour (c.get_player (), c.get_player(), COLOUR_CONTENTS, "Obvious exits:");
					if (prettylook_is_on)
						notify (c.get_player(), "%s~~~~~~~~~~~~~~%s",ca[COLOUR_UNDERLINES], COLOUR_REVERT);
				}
				/* If it's unlinked, print its name and ref */
				if (db[exit].get_destination() == NOTHING)
					if (!(Number(c.get_player())))
						sprintf (scratch_buffer, "  %s%s%s(#%d E) is unlinked.",ca[COLOUR_EXITS], namebuf, COLOUR_REVERT, (int)exit);
					else
						sprintf (scratch_buffer, "  %s%s%s is unlinked.", ca[COLOUR_EXITS],namebuf, COLOUR_REVERT);
				else
				{
					if (c.controls_for_read (exit) && !(Number(c.get_player())))
					{
						if (Opaque(exit))
							sprintf(scratch_buffer, "  %s%s%s(#%d E).", ca[COLOUR_EXITS], namebuf, COLOUR_REVERT, (int)exit);
						else
						{
							if (!strcmp("some ",getarticle(exit,ARTICLE_LOWER_INDEFINITE)))
								sprintf (scratch_buffer, "  %s%s%s(#%d E) lead to %s%s%s.",
									 ca[COLOUR_EXITS], namebuf, COLOUR_REVERT, (int)exit, ca[COLOUR_ROOMNAME], 
									 unparse_objectandarticle_inherited(c, db[exit].get_destination(), ARTICLE_LOWER_INDEFINITE),
									 COLOUR_REVERT);
							else
								sprintf (scratch_buffer, "  %s%s%s(#%d E) leads to %s%s%s.",
									 ca[COLOUR_EXITS],namebuf,COLOUR_REVERT, (int)exit, ca[COLOUR_ROOMNAME],
									 unparse_objectandarticle_inherited(c, db[exit].get_destination(), ARTICLE_LOWER_INDEFINITE),
									 COLOUR_REVERT);
						}

					}
					else
					{
						if ((Opaque(exit)) || (db[exit].get_destination() == HOME))
							sprintf(scratch_buffer, "  %s%s%s.", ca[COLOUR_EXITS], namebuf,COLOUR_REVERT);
						else
							sprintf (scratch_buffer, "  %s%s%s leads to %s%s%s%s.",
								ca[COLOUR_EXITS],namebuf,COLOUR_REVERT,
								ca[COLOUR_ROOMNAME],
								getarticle (db[exit].get_destination (), ARTICLE_LOWER_INDEFINITE),
								getname_inherited (db[exit].get_destination ()),
								COLOUR_REVERT);
					}
				}
				notify_public (c.get_player(), c.get_player (), "%s", scratch_buffer);
			}
		}
		loc = db [loc].get_parent ();
	}
	if (seen && prettylook_is_on)
		notify(c.get_player() , "") ;
}

void
look_room (
context	&c,
dbref	loc)
{
	const colour_at& ca = db[c.get_player()].get_colour_at();
	dbref looper;
	int   prettylook= Prettylook(c.get_player()) ;

	/* tell hir the name, and the number if he can link to it */
	notify_public(c.get_player(), c.get_player (), "%s%s%s",
			ca[COLOUR_ROOMNAME],
			unparse_objectandarticle_inherited (c, loc, ARTICLE_UPPER_INDEFINITE),
			COLOUR_REVERT);
	if (prettylook)
		notify(c.get_player(), "%s%s%s",
				ca[COLOUR_UNDERLINES],
				underline(colour_strlen(unparse_objectandarticle_inherited (c, loc, ARTICLE_UPPER_INDEFINITE))),
				COLOUR_REVERT);


	/* tell hir the description */
	if( db[loc].get_inherited_description() ) 
		notify_public(c.get_player(), c.get_player (), "%s%s%s", ca[COLOUR_ROOMDESCRIPTION], db[loc].get_inherited_description().c_str(), COLOUR_REVERT);
	
	/* tell hir the appropriate messages if he has the key */
	can_doit(c, loc, NULL);
	
	/* display visible properties */
	DOLIST(looper, db[loc].get_properties())	/* Visible Properties are displayed as <name>: <desc> */
	{
		if (Visible(looper))
			notify_public(c.get_player(), c.get_player(), "%s%s:%s %s", ca[COLOUR_PROPERTIES], db[looper].get_name().c_str(), COLOUR_REVERT, db[looper].get_description().c_str());
	}

	if (prettylook)
		notify(c.get_player(),"") ;

	/* Tell hir about self-announcing exits */
	if ((Typeof (loc) == TYPE_ROOM) || ((Typeof (loc) == TYPE_THING) && (Container (loc))))
		look_exits (c, loc, prettylook);

	/* tell hir the contents */
	if (!(Dark (loc) && (Number(c.get_player()))))
	{
		if (!db[loc].get_contents_string())
			look_contents(c, loc, prettylook, "Contents:");
		else
			look_contents(c, loc, prettylook, db[loc].get_contents_string().c_str());
	}

}


void
context::do_look_at (
const	String& name,
const	String& )

{
	dbref		thing;
	int		number;
	int		temp;
	const colour_at& ca = db[player].get_colour_at();

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!name)
	{
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		if ((thing = getloc (player)) != NOTHING)
			look_room (*this, thing);
	}
	else
	{
		/* look at a thing here */
		Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
		if (gagged_command ())
			matcher.work_silent ();
		matcher.match_everything ();
		if((thing = matcher.noisy_match_result()) != NOTHING)
		{
			Accessed (thing);

			if(Private(thing) && !controls_for_private(thing))
			{
				notify (player, "There's an S.E.P. field around that!");
				RETURN_SUCC;
				return;
			}

			if((!Visible(thing)) && (!can_link (*this, thing)) && (!controls_for_look (thing)))
			{
				switch(Typeof(thing))
				{
					case TYPE_ROOM:
						if(db[player].get_location() != thing) 
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
							RETURN_SUCC;
							return;
						}
						break;
					case TYPE_EXIT:
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_THING:
						if(db[player].get_location() != db[thing].get_location()) 
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
							RETURN_SUCC;
							return;
						}
						break;
					default:
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
							RETURN_SUCC;
							return;
					break;
				}
			}

			switch(Typeof(thing))
			{
				case TYPE_ROOM:
					look_room(*this, thing);
					break;
				case TYPE_EXIT:
					if (Opaque(thing) || (db[thing].get_destination() == NOTHING))
						look_simple(*this,thing);
					else
						look_room(*this,(db[thing].get_destination() == HOME) ? db[player].get_destination() : db[thing].get_destination());
					break;
				case TYPE_PLAYER:
				case TYPE_PUPPET:
					look_simple(*this, thing);
					if(!Unassigned (thing))
					{
						strcpy (scratch_buffer, unparse_object_inherited (*this, thing));
						strcat (scratch_buffer, " is ");
						if(Male(thing)) strcat (scratch_buffer, "male.");
						if(Female(thing)) strcat (scratch_buffer, "female.");
						if(Neuter(thing)) strcat (scratch_buffer, "neuter.");
						notify(player, "%s", scratch_buffer);
					}
					if (db[thing].get_score () != 0)
					{
						notify(player, "Score: %d point%s.",
							db[thing].get_score (),
							PLURAL (db[thing].get_score ()));
					}
					notify_public(player, player, "Race: %s.%s",
						db[thing].get_race ().c_str(), COLOUR_REVERT);
					look_contents (*this, thing, Prettylook(player), "Carrying:");
					break;
				case TYPE_COMMAND:
					if (controls_for_read (thing) || !Opaque (thing))
						output_command(thing, player);
					else
						notify (player, "Looking a bit misty in there.");
					break;

				case TYPE_FUSE:
				case TYPE_ALARM:
					if (controls_for_read (thing) || !Opaque (thing))
						look_simple (*this, thing);
					else
						notify (player, "You can't see into that.");
					break;
				case TYPE_ARRAY:
					{
						unsigned int value;
						if (controls_for_read (thing) || !Opaque (thing))
						{
							if (matcher.match_index_attempt_result())
							{
								if(matcher.match_index_result())
								{
									if ((value=atoi(matcher.match_index_result().c_str())) > 0)
										if (value <= ((Wizard(thing)) ? MAX_WIZARD_ARRAY_ELEMENTS:MAX_MORTAL_ARRAY_ELEMENTS))
										{
																notify(player, "[%d] : %s", value, db[thing].get_element(value).c_str());
											return;
										}
										else
										{
											notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have more than %d elements.", (Wizard(thing))?MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
											return;
										}
									else
									{	
										notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have negative, zero or non-numeric indeces.");
										return;
									}
								}
							}
						
						
							number = db[thing].get_number_of_elements();
							notify(player, "%sElement%s:%s %d", ca[COLOUR_TITLES], PLURAL(number), COLOUR_REVERT, number);
							if(number > 0)
							{
								for(temp = 1;temp <= number;temp++)
									notify_censor(player, player, "  [%d] :%s %s", temp, (temp<10)?"  ":(temp<100)?" ":"", db[thing].get_element(temp).c_str());
							}
						}
					}
					break;
				case TYPE_DICTIONARY:
					if (controls_for_read (thing) || !Opaque (thing))
					{
						unsigned int value;
						if (matcher.match_index_attempt_result())
						{
							if((value=db[thing].exist_element(matcher.match_index_result())))
							{
								notify(player, "[%s] : %s", db[thing].get_index(value).c_str() , db[thing].get_element(value).c_str());
								return;
							}
							else
							{
								notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary doesn't contain element \"%s\"", matcher.match_index_result().c_str());
								return;
							}
								if ((value=atoi(matcher.match_index_result().c_str())) > 0)
									if (value <= ((Wizard(thing)) ? MAX_WIZARD_ARRAY_ELEMENTS:MAX_MORTAL_ARRAY_ELEMENTS))
									{
															notify(player, "[%d] : %s", value, db[thing].get_element(value).c_str());
										return;
									}
						}
						number = db[thing].get_number_of_elements();
							notify(player, "%sElement%s:%s %d", ca[COLOUR_TITLES], PLURAL(number), COLOUR_REVERT, number);
						if(number > 0)
						{
							if(Opaque(thing))
								for(temp = 1;temp <= number;temp++)
									notify_censor(player, player, "  [%s]", db[thing].get_index(temp).c_str());
							else
								for(temp = 1;temp <= number;temp++)
									notify_censor(player, player, "  [%s] : %s", db[thing].get_index(temp).c_str(), db[thing].get_element(temp).c_str());
						}
					}
					break;
				default:
					look_simple(*this, thing);
					break;
			}
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
		}
	}
}


const char *
flag_description (
dbref	thing)

{
	static	char	buf[BUFFER_LEN];
	int		i;

	*buf = '\0';
	/* Formerly tested to see if there were any flags before proceeding but */
	for (i = 0; flag_list [i].string != NULL; i++)
	if (db[thing].get_flag(flag_list [i].flag))
	{
		strcat (buf, " ");
		strcat (buf, flag_list [i].string);
	}

	return (buf);
}

void
context::do_examine (
const	String& name,
const	String& )

{
	const colour_at& ca = db[player].get_colour_at();
	int	number;
	int	temp;
	int	had_title;
	unsigned int	value;
	dbref	thing;
	dbref	looper;
	char	stored_owner [BUFFER_LEN];
	time_t	last, total, now;
	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!name)
	{
		if((thing = getloc(player)) == NOTHING)
			return;
	}
	else
	{
		/* look it up */
		matcher.match_everything ();
		if((thing = matcher.noisy_match_result()) == NOTHING)
			return;
	}

	if(db[thing].get_name())
		notify_censor(player, player, "%s", unparse_object(*this, thing));
	else
		notify_censor(player, player, "%s", unparse_object_inherited(*this, thing));
	strcpy (stored_owner, unparse_object(*this, db[thing].get_owner()));

	if((Private(thing) && !controls_for_private(thing)) ||
	  ((!can_link (*this, thing)) && (!controls_for_read (thing))))
	{
		notify (player, "%sOwner:%s %s.", ca[COLOUR_TITLES], COLOUR_REVERT, unparse_object_inherited (*this, db[thing].get_owner()));
		return;
	}

	return_status = COMMAND_SUCC;
	set_return_string (unparse_for_return (*this, thing));

	switch(Typeof(thing))
	{
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
			notify(player, "%sOwner:%s %s; %sFlags:%s%s",
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					stored_owner,
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					flag_description(thing));
			break;
		default:
			if((db[thing].get_key() == TRUE_BOOLEXP) && (db[thing].get_parent() != NOTHING) && (db[thing].get_inherited_key() != TRUE_BOOLEXP))
			{
				notify(player, "%sOwner:%s %s; %sKey [INHERITED]:%s %s; %sFlags:%s%s",
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					stored_owner,
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					db[thing].get_inherited_key()->unparse (*this),
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					flag_description(thing));
			}
			else
			{
				notify(player, "%sOwner:%s %s; %sKey:%s %s; %sFlags:%s%s",
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					stored_owner,
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					db[thing].get_key()->unparse (*this),
					ca[COLOUR_TITLES],
					COLOUR_REVERT,
					flag_description(thing));
			}
			break;
	}

	switch(Typeof(thing))
	{
		case TYPE_FUSE:
			if (( !db[thing].get_description ()) && (db[thing].get_parent() != NOTHING))
				notify (player, "%sTicks [INHERITED]:%s %s", ca[COLOUR_TITLES], COLOUR_REVERT, db[thing].get_inherited_description ().c_str());
			else
				notify (player, "%sTicks:%s %s", ca[COLOUR_TITLES], COLOUR_REVERT, db[thing].get_description().c_str());
			break;

		case TYPE_COMMAND:
			if(db[thing].get_description())
			{
				notify(player, "%sDescription:%s", ca[COLOUR_TITLES], ca[COLOUR_DESCRIPTIONS]);
				output_command(thing, player);
			}
			else
				if (db[thing].get_inherited_description ())
				{
					notify (player, "%sDescription [INHERITED]:%s", ca[COLOUR_TITLES],  ca[COLOUR_DESCRIPTIONS]);
					output_command(thing, player);
				}
			break;

		default:
			if(db[thing].get_description())
				notify(player, "%sDescription:%s %s%s", ca[COLOUR_TITLES], ca[COLOUR_DESCRIPTIONS], db[thing].get_description().c_str(), COLOUR_REVERT);
			else
				if (db[thing].get_inherited_description ())
					notify (player, "%sDescription [INHERITED]:%s %s%s", ca[COLOUR_TITLES],  ca[COLOUR_DESCRIPTIONS], db[thing].get_inherited_description ().c_str(), COLOUR_REVERT);
			break;
	}

/* Race (for puppets and players) and aliases (for players only) */
	switch(Typeof(thing))
	{
		case TYPE_PLAYER:
#ifdef ALIASES
			{
				const String& aliases = format_alias_string(thing);
				if (aliases)
					notify_public(player, player, "%sAliases:%s %s", ca[COLOUR_TITLES], COLOUR_REVERT, aliases.c_str());
			}
			// FALLTHROUGH
#endif
		case TYPE_PUPPET:
			notify_public(player, player, "%sRace:%s %s.", ca[COLOUR_TITLES], COLOUR_REVERT, db[thing].get_race ().c_str());
			break;
		default:
			break;
	}

/* Do fail message for object */
	switch(Typeof(thing))
	{
		case TYPE_PLAYER:
			if(db[thing].get_fail_message())
			{

				last = atol(db[thing].get_fail_message().c_str());
				sprintf(scratch_buffer, "%sLast time connected:%s %s%s", 
					ca[COLOUR_TITLES], 
					ca[COLOUR_LAST_CONNECTED], 
					(last==0) ? "Unknown" : ctime(&last), 
					COLOUR_REVERT);

				int i;
				for (i= strlen(scratch_buffer) ; (scratch_buffer[i] != '\n') && (i > 0) ;  i--);
				if (scratch_buffer[i]=='\n')
				    scratch_buffer[i]='\0';

				notify(player, "%s", scratch_buffer);
			}
			break;

		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if (db[thing].get_fail_message ())
				notify_censor (player, player, "%sFail:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_FAILURE],
								db[thing].get_fail_message ().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_fail_message ())
					notify_censor(player, player, "%sFail [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_FAILURE],
								db[thing].get_inherited_fail_message().c_str(),
								COLOUR_REVERT);
			break;
		default:
			break;
	}


/* Do success message for object */
	switch(Typeof(thing))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if(db[thing].get_succ_message())
				notify_censor(player, player, "%sSuccess:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_SUCCESS],
								db[thing].get_succ_message().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_succ_message ())
					notify_censor (player, player, "%sSuccess [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_SUCCESS],
								db[thing].get_inherited_succ_message ().c_str(),
								COLOUR_REVERT);
			break;
		default:
			break;
	}

/* Do drop message for object */
	switch(Typeof(thing))
	{
		case TYPE_FUSE:
			if (db[thing].get_drop_message())
				notify(player, "%sReset:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_DROP],
								db[thing].get_drop_message().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_drop_message ())
					notify(player, "%sReset [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_DROP],
								db[thing].get_inherited_drop_message().c_str(),
								COLOUR_REVERT);
			break;

		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if (db[thing].get_drop_message())
				notify_censor(player, player, "%sDrop:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_DROP],
								db[thing].get_drop_message().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_drop_message ())
					notify_censor (player, player, "%sDrop [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_DROP],
								db[thing].get_inherited_drop_message().c_str(),
								COLOUR_REVERT);
			break;
		default:
			break;
	}

/* Do ofail message for object */
	switch(Typeof(thing))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if(db[thing].get_ofail())
				notify_censor(player, player, "%sOfail:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_OFAILURE],
								db[thing].get_ofail().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_ofail ())
					notify_censor (player, player, "%sOfail [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_OFAILURE],
								db[thing].get_inherited_ofail().c_str(),
								COLOUR_REVERT);
			break;
		case TYPE_PLAYER:
			if(db[thing].get_ofail())
			{
				total = atol(db[thing].get_ofail().c_str());
				if(Connected(thing))
				{
					time(&now);
					last = atol(db[thing].get_fail_message().c_str());
					total += (now - last);
				}
				notify(player, "%sTotal time connected:%s %s%s.",
								ca[COLOUR_TITLES],
								ca[COLOUR_TOTAL_CONNECTED],
								(last==0) ? "Unknown" : time_string (total),
								COLOUR_REVERT);
			}
			break;
		default:
			break;
	}

/* Do osuccess message for object */
	switch(Typeof(thing))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if(db[thing].get_osuccess())
				notify_censor(player, player, "%sOsuccess:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_OSUCCESS],
								db[thing].get_osuccess().c_str(),
								COLOUR_REVERT);
			else
				if(db[thing].get_inherited_osuccess())
					notify_censor(player, player, "%sOsuccess [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_OSUCCESS],
								db[thing].get_inherited_osuccess().c_str(),
								COLOUR_REVERT);
			break;
		default:
			break;
	}

/* Do odrop message for object */
	switch(Typeof(thing))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			if (db[thing].get_odrop())
				notify_censor(player, player, "%sOdrop:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_ODROP],
								db[thing].get_odrop().c_str(),
								COLOUR_REVERT);
			else
				if (db[thing].get_inherited_odrop())
					notify_censor(player, player, "%sOdrop [INHERITED]:%s %s%s",
								ca[COLOUR_TITLES],
								ca[COLOUR_ODROP],
								db[thing].get_inherited_odrop().c_str(),
								COLOUR_REVERT);
		default:
			break;
	}

	if((db[thing].get_location() != NOTHING)
		&& (controls_for_read (db[thing].get_location()) || can_link_to (*this, db[thing].get_location())))
		notify_censor(player, player, "%sLocation:%s %s%s", ca[COLOUR_TITLES], ca[COLOUR_ROOMNAME], unparse_object(*this, db[thing].get_location()), COLOUR_REVERT);

	switch(Typeof(thing))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
				notify_censor(player, player, "%sParent:%s %s", ca[COLOUR_TITLES], COLOUR_REVERT, unparse_object(*this, db[thing].get_parent ()));
		default:
			break;
	}

	/* Mass and volume */
	switch(Typeof(thing))
	{
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if (db[thing].get_gravity_factor () != 1)
				if (db[thing].get_gravity_factor () == NUM_INHERIT)
					notify (player, "%sGravity factor [INHERITED]:%s %.9g",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						db[thing].get_inherited_gravity_factor ());
				else
					notify (player, "%sGravity factor:%s %.9g",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						db[thing].get_gravity_factor ());
			if (db[thing].get_mass () == NUM_INHERIT)
				notify (player, "%sMass [INHERITED]:%s %s%.9g kilos%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
					db[thing].get_inherited_mass(),
						COLOUR_REVERT);

			else
				notify (player, "%sMass:%s %s%.9g kilos%s", 
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
					db[thing].get_mass (),
						COLOUR_REVERT);

			if (db[thing].get_volume () == NUM_INHERIT)
				notify(player, "%sVolume [INHERITED]:%s %s%.9g litres%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
					db[thing].get_inherited_volume (),
						COLOUR_REVERT);
			else
				notify(player, "%sVolume:%s %s%.9g litres%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
					db[thing].get_volume (),
						COLOUR_REVERT);

			if (db[thing].get_mass_limit () == NUM_INHERIT)
				if (db[thing].get_inherited_mass_limit () == HUGE_VAL)
					notify(player, "%sMass Limit [INHERITED]:%s %sNone%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
						COLOUR_REVERT);
				else
					notify(player, "%sMass Limit [INHERITED]:%s %s%.9g kilos%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
						db[thing].get_inherited_mass_limit (),
						COLOUR_REVERT);
			else
				if (db[thing].get_mass_limit () == HUGE_VAL)
					notify(player, "%sMass Limit:%s %sNone%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
						COLOUR_REVERT);
				else
					notify(player, "%sMass Limit:%s %s%.9g kilos%s", 
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_MASS],
						db[thing].get_mass_limit (),
						COLOUR_REVERT);

			if (db[thing].get_volume_limit () == NUM_INHERIT)
				if (db[thing].get_inherited_volume_limit () == HUGE_VAL)
					notify(player, "%sVolume Limit [INHERITED]:%s %sNone%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
						COLOUR_REVERT);
				else
					notify(player, "%sVolume Limit [INHERITED]:%s %s%.9g litres%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
						db[thing].get_inherited_volume_limit (),
						COLOUR_REVERT);
			else
				if (db[thing].get_volume_limit () == HUGE_VAL)
					notify(player, "%sVolume Limit:%s %sNone%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
						COLOUR_REVERT);
				else
					notify(player, "%sVolume Limit:%s %s%.9g litres%s",
						ca[COLOUR_TITLES],
						COLOUR_REVERT,
						ca[COLOUR_VOLUME],
						db[thing].get_volume_limit (),
						COLOUR_REVERT);
			break;
		default:
			break;
	}

	const char *currency_name=CURRENCY_NAME;
	
	switch(Typeof(thing))
	{
	    case TYPE_ROOM:
		/* print dropto if present */
		if(db[thing].get_destination() != NOTHING)
			notify_censor(player, player, "%sDropped objects go to:%s %s",
					ca[COLOUR_TITLES], COLOUR_REVERT, unparse_object(*this, db[thing].get_destination()));
		/* print last entry time */
		if(db[thing].get_last_entry_time() == 0)
			notify(player, "%sLast entry time:%s %sUnknown%s", ca[COLOUR_TITLES], COLOUR_REVERT, ca[COLOUR_LAST_CONNECTED], COLOUR_REVERT);
		else
		{
			time_t last_entry_time = db[thing].get_last_entry_time();
			strcpy(scratch_buffer, ctime(&last_entry_time));
			*strchr(scratch_buffer, '\n') = '\0';
			notify(player, "%sLast entry time:%s %s%s%s",  ca[COLOUR_TITLES], COLOUR_REVERT, ca[COLOUR_LAST_CONNECTED], scratch_buffer, COLOUR_REVERT);
		}
		break;
	  case TYPE_PLAYER:
	  case TYPE_PUPPET:
		notify (player, "%sBuilding points:%s %d", ca[COLOUR_TITLES],COLOUR_REVERT,db [thing].get_pennies ());
		notify (player, "%sScore:%s %d", ca[COLOUR_TITLES],COLOUR_REVERT,db[thing].get_score ());
		notify (player, "%s%s:%s %d", ca[COLOUR_TITLES], currency_name, COLOUR_REVERT,db[thing].get_money ());
		notify_censor(player, player, "%sHome:%s %s", ca[COLOUR_TITLES],COLOUR_REVERT,unparse_object(*this, db[thing].get_destination()));
		if (db[thing].get_controller () != NOTHING)
			notify_censor(player, player, "%sController:%s %s", ca[COLOUR_TITLES],COLOUR_REVERT,unparse_object(*this, db[thing].get_controller ()));
		break;
	  case TYPE_THING:
		/* print home */
		notify_censor(player, player, "%sHome:%s %s", ca[COLOUR_TITLES],COLOUR_REVERT,unparse_object(*this, db[thing].get_destination()));
		break;
	  case TYPE_EXIT:
		/* print destination */
		if (db[thing].get_destination() != NOTHING)
			notify_censor(player, player, "%sDestination:%s %s", ca[COLOUR_TITLES],COLOUR_REVERT,unparse_object(*this, db[thing].get_destination()));
		break;
	  case TYPE_COMMAND:
	  case TYPE_FUSE:
		/* Print success and fail destinations */
		notify_censor(player, player, "%sOn success, go to:%s %s%s%s", ca[COLOUR_TITLES],COLOUR_REVERT,
					ca[COLOUR_CSUCCESS],unparse_object (*this, db [thing].get_csucc ()), COLOUR_REVERT);
		notify_censor(player, player, "%sOn failure, go to:%s %s%s%s", ca[COLOUR_TITLES],COLOUR_REVERT,
					ca[COLOUR_CFAILURE],unparse_object (*this, db [thing].get_cfail ()), COLOUR_REVERT);
		break;
	  case TYPE_ALARM:
		/* print alarm fire command */
		notify_censor(player, player, "%sFire:%s %s", ca[COLOUR_TITLES],COLOUR_REVERT,unparse_object (*this, db [thing].get_csucc ()));
		break;
	  case TYPE_DICTIONARY:
		if (matcher.match_index_attempt_result())
		{
			if((value=db[thing].exist_element(matcher.match_index_result())))
			{
				notify(player, "[%s] : %s", db[thing].get_index(value).c_str() , db[thing].get_element(value).c_str());
				break;
			}
			else
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary doesn't contain element \"%s\"", matcher.match_index_result().c_str());
				break;
			}
		}
	  	/* list elements */
	  	number = db[thing].get_number_of_elements();
			notify(player, "%sElements:%s %d", ca[COLOUR_TITLES],COLOUR_REVERT,number);
	  	if(number > 0)
	  	{
	  		/* If a dictionary is set opaque then it doesn't show its elements
	  		   on examine, just its indexes */
	  		if(Opaque(thing))
				for(temp = 1;temp <= number;temp++)
					notify_censor(player, player, "  %s[%s]%s", ca[COLOUR_MESSAGES], db[thing].get_index(temp).c_str(), COLOUR_REVERT);
	  		else
				for(temp = 1;temp <= number;temp++)
					notify_censor(player, player, "  %s[%s]%s : %s", ca[COLOUR_MESSAGES], db[thing].get_index(temp).c_str(), COLOUR_REVERT, db[thing].get_element(temp).c_str());
		}
		break;

	  case TYPE_ARRAY:
		if (matcher.match_index_attempt_result())
		{
			if(matcher.match_index_result())
			{
				if ((value=atoi(matcher.match_index_result().c_str())) > 0)
					if (value <= ((Wizard(thing)) ? MAX_WIZARD_ARRAY_ELEMENTS:MAX_MORTAL_ARRAY_ELEMENTS))
					{
						notify(player, "[%d] : %s", value, db[thing].get_element(value).c_str());
						break;
					}
					else
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have more than %d elements.", (Wizard(thing))?MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
						break;
					}
				else
				{	
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have negative, zero or non-numeric indeces.");
					break;
				}
			}
		}
					
	  	/* list elements */
	  	number = db[thing].get_number_of_elements();
		notify_censor(player, player, "%sElements:%s %d", ca[COLOUR_TITLES],COLOUR_REVERT,number);
	  	if(number > 0)
	  	{
			for(temp = 1;temp <= number;temp++)
				notify_censor(player, player, "  %s[%d]%s %s: %s", ca[COLOUR_TITLES],  temp, COLOUR_REVERT, (temp<10)?"  ":(temp<100)?" ":"", db[thing].get_element(temp).c_str());
		}
		break;
	  default:
		/* do nothing */
		break;
	}

	/* show the contents */
	if ((Typeof(thing) == TYPE_ROOM) && db[thing].get_contents_string())
		notify_censor(player, player, "%sContents string:%s %s",
				ca[COLOUR_CONTENTS],
				COLOUR_REVERT, db[thing].get_contents_string().c_str());

	if (Container (thing))
		examine_container (*this, thing, 0);
	else
		switch (Typeof (thing))
		{
			case TYPE_ROOM:
			case TYPE_THING:
			case TYPE_PLAYER:
			case TYPE_PUPPET:
				if (db[thing].get_contents() != NOTHING)
				{
					notify_colour(player, player, COLOUR_CONTENTS, "Contents:");
					DOLIST(looper, db[thing].get_contents())
					{
						switch(Typeof(looper))
						{
							case TYPE_PLAYER:
								notify(player, "  %s%s%s%s", (Connected (looper) ? "*" : ""), ca[rank_colour(looper)], unparse_object(*this, looper), COLOUR_REVERT);
								break;
							default:
								notify_public_colour(player, player, COLOUR_THINGS, "  %s", unparse_object(*this, looper));
								break;
						}
					}
				}
				break;
			default:
				break;
		}

	/* Exits if there are any */
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
			if(db[thing].get_exits() != NOTHING)
			{
				notify_colour(player, player, COLOUR_CONTENTS, "Exits:");
				DOLIST(looper, db[thing].get_exits())
				{
					notify_public(player, player,"  %s%s%s",  ca[COLOUR_EXITS], unparse_object(*this, looper), COLOUR_REVERT);
				}
			}
			break;
		default:
			break;
		/* default: do nothing */
	}

	/* Show commands */
	switch(Typeof(thing))
	{
		case TYPE_THING:
		case TYPE_ROOM:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if (db[thing].get_commands() != NOTHING)
			{
				notify_colour(player, player, COLOUR_CONTENTS, "Commands:");
				DOLIST(looper, db[thing].get_commands())
				{
					notify_public(player, player,  "  %s%s%s",  ca[COLOUR_COMMANDS], unparse_object(*this, looper), COLOUR_REVERT);
				}
			}
			break;
		default:
			break;
	}

	/* Show variables, properties, arrays and dictionaries */
	switch(Typeof(thing))
	{
		case TYPE_THING:
		case TYPE_ROOM:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
		case TYPE_COMMAND:
		case TYPE_ALARM:
		case TYPE_FUSE:
		case TYPE_EXIT:
			if (db[thing].get_info_items() != NOTHING)
			{
				/* Do the variables list */
				temp = 0; /* Whether we need heading */
				DOLIST(looper, db[thing].get_variables())
				{
					if (Typeof(looper) == TYPE_VARIABLE)
					{
						/* Dark variables don't show up unless you own them or you are a wizard */
						if(!Dark(looper) || controls_for_read(looper))
						{
							if(!temp)
							{
								notify_colour(player, player, COLOUR_CONTENTS, "Variables:");
								temp = 1;
							}
							sprintf(scratch_buffer, "  %s%s", ca[COLOUR_PROPERTIES],unparse_object(*this, looper));
							{
								if (!Dark(looper) && controls_for_read (looper))
								{
									strcat (scratch_buffer, " = ");
									strcat (scratch_buffer, db[looper].get_description().c_str());
								}
								notify_public(player, player, "%s%s", scratch_buffer, COLOUR_REVERT);
							}
						}
					}
				}

				/* Do the property list */
				temp = 0; /* Whether we need heading */
				DOLIST(looper, db[thing].get_properties())
				{
					if(Typeof(looper) == TYPE_PROPERTY)
					{
						/* Dark variables don't show up unless you own them or you are a wizard */
						if(!Dark(looper) || controls_for_read(looper))
						{
							if(!temp)
							{
								notify_colour(player, player, COLOUR_CONTENTS, "Properties:");
								temp = 1;
							}
							sprintf(scratch_buffer, "  %s%s", ca[COLOUR_PROPERTIES], unparse_object(*this, looper));
							if (!Dark(looper) && controls_for_read (looper))
							{
								strcat (scratch_buffer, " = ");
								strcat (scratch_buffer, db[looper].get_description().c_str());
							}
							notify_public(player, player, "%s%s", scratch_buffer, COLOUR_REVERT);
						}
					}
				}

				/* Do the array list */
				temp = 0; /* Whether we need heading */
				DOLIST(looper, db[thing].get_arrays())
				{
					if(Typeof(looper) == TYPE_ARRAY)
					{
						/* Dark arrays don't show up unless you own them or you are a wizard */
						if(!Dark(looper) || controls_for_read(looper))
						{
							if(!temp)
							{
								notify_colour(player, player, COLOUR_CONTENTS, "Arrays:");
								temp = 1;
							}
							sprintf(scratch_buffer, "  %s", unparse_object(*this, looper));
							if (!Dark(looper) && controls_for_read (looper))
								notify_public(player, player, "%s%s : %s%d element%s", ca[COLOUR_ARRAYS], scratch_buffer, COLOUR_REVERT, db[looper].get_number_of_elements(), PLURAL(db[looper].get_number_of_elements()));
							else
								notify_public(player, player, "%s%s%s", ca[COLOUR_ARRAYS], scratch_buffer, COLOUR_REVERT);
						}
					}
				}

				/* Do the Dictionary list */
				temp = 0; /* Whether we need heading */
				DOLIST(looper, db[thing].get_dictionaries())
				{
					if(Typeof(looper) == TYPE_DICTIONARY)
					{
						/* Dark dictionaries don't show up unless you own them or you are a wizard */
						if(!Dark(looper) || controls_for_read(looper))
						{
							if(!temp)
							{
								notify_colour(player, player, COLOUR_CONTENTS, "Dictionaries:");
								temp = 1;
							}
							sprintf(scratch_buffer, "  %s", unparse_object(*this, looper));
							if (!Dark(looper) && controls_for_read (looper))
								notify_public(player, player, "%s%s : %s%d element%s", ca[COLOUR_DICTIONARIES], scratch_buffer, COLOUR_REVERT, db[looper].get_number_of_elements(), PLURAL(db[looper].get_number_of_elements()));
							else
								notify_public_colour(player, player, COLOUR_DICTIONARIES, "%s",  scratch_buffer);
						}
					}
				}
			}
		default:
			break;
	}

	switch(Typeof(thing))
	{
		case TYPE_THING:
		case TYPE_ROOM:
		case TYPE_PLAYER:
		case TYPE_PUPPET:
        	case TYPE_EXIT:
			if (db[thing].get_fuses() != NOTHING)
			{
				had_title = 0;
				DOLIST(looper, db[thing].get_fuses())
				{
					if (Typeof (looper) == TYPE_FUSE)
					{
						if (!had_title)
						{
							notify_colour(player, player, COLOUR_CONTENTS, "Fuses:");
							had_title = 1;
						}
						notify_public (player, player, "  %s%s%s", ca[COLOUR_FUSES], unparse_object(*this, looper), COLOUR_REVERT);
					}
				}
				had_title = 0;
				DOLIST(looper, db[thing].get_fuses())
				{
					if (Typeof (looper) == TYPE_ALARM)
					{
						if (!had_title)
						{
							notify_colour (player, player, COLOUR_CONTENTS, "Alarms:");
							had_title = 1;
						}
						notify_public(player, player, "  %s%s%s", ca[COLOUR_ALARMS], unparse_object(*this, looper), COLOUR_REVERT);
					}
				}
			}
			break;

		default:
			break;
	}

	if(db[thing].get_ctime() || db[thing].get_mtime() || db[thing].get_atime())
	{
		notify_colour(player, player, COLOUR_CONTENTS, "Timestamps:");

		time(&now);

		if((last=db[thing].get_ctime()))
		{
			sprintf(scratch_buffer, "  %sCreate:%s %s(+%s)%s", 
				ca[COLOUR_TITLES], 
				ca[COLOUR_TIMESTAMPS], 
				ctime(&last), 
				tiny_time_string(now-last),
				COLOUR_REVERT);

			*strchr(scratch_buffer, '\n') = ' ';
			notify(player, "%s", scratch_buffer);
		}

		if((last=db[thing].get_mtime()))
		{
			sprintf(scratch_buffer, "  %sModify:%s %s(+%s)%s", 
				ca[COLOUR_TITLES], 
				ca[COLOUR_TIMESTAMPS], 
				ctime(&last), 
				tiny_time_string(now-last),
				COLOUR_REVERT);

			*strchr(scratch_buffer, '\n') = ' ';
			notify(player, "%s", scratch_buffer);
		}

		if((last=db[thing].get_atime()))
		{
			sprintf(scratch_buffer, "  %sAccess:%s %s(+%s)%s", 
				ca[COLOUR_TITLES], 
				ca[COLOUR_TIMESTAMPS], 
				ctime(&last), 
				tiny_time_string(now-last),
				COLOUR_REVERT);

			*strchr(scratch_buffer, '\n') = ' ';
			notify(player, "%s", scratch_buffer);
		}
	}
}


void
context::do_score (
const	String& ,
const	String& )

{
	Accessed (player);

	notify_colour(player, player, COLOUR_MESSAGES, "You have %d %s.",
		db[player].get_pennies(),
		db[player].get_pennies() == 1 ? "building point" : "building points");
	notify_colour(player, player, COLOUR_MESSAGES, "You have %d %s.",
		db[player].get_money(),
		CURRENCY_NAME);
	notify_colour(player, player, COLOUR_MESSAGES, "You have %d %s.",
		db[player].get_score (),
		db[player].get_score () == 1 ? "point" : "points");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_inventory (
const	String& ,
const	String& )
{
	dbref thing;
	const colour_at& ca = db[player].get_colour_at();

	Accessed (player);

	if((thing = db[player].get_contents()) == NOTHING)
		notify_colour(player, player, COLOUR_MESSAGES, "You aren't carrying anything.");
	else
	{
		notify_colour(player, player, COLOUR_CONTENTS, "You are carrying:");
		DOLIST(thing, thing)
		{
			switch(Typeof(thing))
			{
				case TYPE_THING:
					notify_censor(player, player, "  %s%s%s", ca[COLOUR_THINGS], unparse_objectandarticle_inherited(*this, thing, ARTICLE_LOWER_INDEFINITE), COLOUR_REVERT);
					break;
				default:
					notify_censor(player, player, "  %s", unparse_objectandarticle(*this, thing, ARTICLE_LOWER_INDEFINITE));
					break;
			}
		}
	}

	do_score(NULLSTRING, NULLSTRING);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_list (
const	String& descriptor,
const	String& string)

{
	dbref	i;
	dbref	victim;
	int	search_type;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (in_command())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "Sorry, @list may not be used within compound commands.");
		return;
	}
#if LOOKUP_COST
	if(!payfor(get_effective_id (), LOOKUP_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have enough building points.");
		return;
	}
#endif

	if (!string)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "What do you want to list?");
		return;
	}
	if (!descriptor)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Whose things do you want to list?");
		return;
	}

	/*
	 * Special 'victim' of the game itself. This allows us to list Ashcanned objects as a last minute check.
	 * (Since it may reveal 'top secret' information, we restrict its use to admin.)
	 */

	if (string_compare(descriptor, "game") == 0)
	{
		if (!Wizard(player) && !Apprentice(player))
		{
			 notify_colour(player, player, COLOUR_ERROR_MESSAGES, "'@list game' is restricted to Wizards and Apprentices only.");
			 return;
		}
		
		if (! string_prefix("ashcanned", string))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Currently only Ashcan is a valid type to list using '@list game'.");
			return;
		}

		notify_colour(player, player, COLOUR_TITLES, "List of items currently set AshCan:");

		for(i = 0; i < db.get_top (); i++)
		if ((Typeof(i) != TYPE_FREE) && Ashcan(i))
			notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));

		notify_colour(player, player, COLOUR_CONTENTS, "*** End of List ***");
		return;
	}


	if ((victim = lookup_player(player, descriptor)) == NOTHING)
	{
		Matcher matcher (player, descriptor, TYPE_PUPPET, get_effective_id());
		matcher.match_everything();

		if (Typeof(victim= matcher.noisy_match_result()) != TYPE_PUPPET)
		{
			if (victim==NOTHING)
				return;

			/* Only allow this if we're looking at child objects */
			if (string_prefix("children",string))
			{
			
				if (!controls_for_read(victim))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
			}
		}
	}

	if(!controls_for_read (victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if (string_prefix("children", string))
	{
		for (i=0 ; i < db.get_top(); i++)
			if ((Typeof (i) != TYPE_FREE) && (db[i].get_parent() == victim))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s",unparse_object(*this, i));
			notify_colour(player, player, COLOUR_CONTENTS, "*** End of List ***");
		return;
	}

	if (string_prefix("commands", string))
		search_type = TYPE_COMMAND;
	else if (string_prefix("rooms", string))
		search_type = TYPE_ROOM;
	else if (string_prefix("things", string))
		search_type = TYPE_THING;
	else if (string_prefix("exits", string))
		search_type = TYPE_EXIT;
	else if (string_prefix("variables", string))
		search_type = TYPE_VARIABLE;
	else if (string_prefix("fuses", string))
		search_type = TYPE_FUSE;
	else if (string_prefix("alarms", string))
		search_type = TYPE_ALARM;
	else if (string_prefix("puppets", string))
		search_type = TYPE_PLAYER;
	else if (string_prefix("arrays", string))
		search_type = TYPE_ARRAY;
	else if (string_prefix("dictionary", string))
		search_type = TYPE_DICTIONARY;
	else if (string_prefix("dictionaries", string))
		search_type = TYPE_DICTIONARY;
	else if (string_prefix("property", string))
		search_type = TYPE_PROPERTY;
	else if (string_prefix("properties", string))
		search_type = TYPE_PROPERTY;
	else if (string_prefix("npcs", string))
		search_type = TYPE_PLAYER;
	else if (string_prefix("ashcanned", string))
		search_type = TYPE_FREE; /* A bit hacky but it will do for the moment. Used because
						the flag and type numbers overlap.*/
	else if (string_prefix("all", string))
		search_type = TYPE_NO_TYPE;
	else
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's not a valid type to search for.");
		return;
	}

	if (search_type == TYPE_NO_TYPE)
	{
		for(i = 0; i < db.get_top (); i++)
			if ((Typeof (i) != TYPE_FREE) && (db[i].get_owner() == victim))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	if (search_type == TYPE_PLAYER)
	{
		for(i = 0; i < db.get_top (); i++)
		if ((Typeof(i) == TYPE_PLAYER) || (Typeof(i) == TYPE_PUPPET))
			if (db [i].get_controller () == victim)
				notify_censor_colour(player, player, COLOUR_PLAYERS, "%s", unparse_object(*this, i));
	}
	else if(search_type == TYPE_FREE) /*Ashcan*/
	{
		for(i = 0; i < db.get_top (); i++)
		if ((Typeof(i) != TYPE_FREE) && Ashcan(i))
			if (db [i].get_owner () == victim)
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else
	{
		for(i = 0; i < db.get_top (); i++)
			if ((Typeof (i) == search_type) && (db[i].get_owner() == victim))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	notify_colour(player, player, COLOUR_CONTENTS, "***End of List***");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_find (
const	String& descriptor,
const	String& string)

{
	dbref	i;
	dbref	room;

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);

	if (in_command () && !Wizard (player))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@find is not allowed inside a command.");
		return;
	}

	if (!string)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You must specify a string to search for. If you want to list all your objects of a particular type, use @list instead.");
		return;
	}

	if(!payfor(get_effective_id (), LOOKUP_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have enough building points.");
		set_return_string (error_return_string);
		return_status = COMMAND_FAIL;
		return;
	}

	RegularExpression re(string);

	if (string_prefix("me", descriptor))
	/* Remind people about the code change */
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "Please use @list <player> = <type> instead");
		return;
	}
	else if (string_prefix("name", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if((Typeof (i) != TYPE_FREE)
				&& Typeof(i) != TYPE_EXIT
				&& db[i].get_name()
				&& controls_for_read(i)
				&& (!string || re.Match(db[i].get_name())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("description", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if((Typeof (i) != TYPE_FREE)
				&& Typeof(i) != TYPE_EXIT
				&& db[i].get_description()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_description())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("cname", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_COMMAND
				&& db[i].get_name()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_name())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("cdescription", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_COMMAND
				&& db[i].get_description()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_description())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("vname", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_VARIABLE
				&& db[i].get_name()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_name())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("vdescription", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_VARIABLE
				&& db[i].get_description()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_description())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("ename", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_EXIT
				&& db[i].get_name()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_name())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("edescription", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_EXIT
				&& db[i].get_description()
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_description())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("edestination", descriptor))
	{
	/* ok, so the second string should either be 'here' or an absolute number */
		Matcher matcher (player, string, TYPE_NO_TYPE, get_effective_id ());
		matcher.match_here ();
		matcher.match_absolute ();
		if ((room = matcher.noisy_match_result ()) == NOTHING)
		{
			set_return_string (error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}
		else if (Typeof(room) != TYPE_ROOM)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That isn't a room.");
			set_return_string (error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}
		if (controls_for_read  (room))
			for(i = 0; i < db.get_top (); i++)
				if(Typeof(i) == TYPE_EXIT
					&& controls_for_read (i)
					&& (db[i].get_destination() == room))
					notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("cdestination", descriptor))
	{
	/* ok, so the second string should either be 'here' or an absolute number */
		dbref command = NOTHING;
		Matcher matcher (player, string, TYPE_NO_TYPE, get_effective_id ());
		matcher.match_command ();
		matcher.match_absolute ();
		if ((command = matcher.noisy_match_result ()) == NOTHING)
		{
			set_return_string (error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}
		else if (Typeof(command) != TYPE_COMMAND)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That isn't a command.");
			set_return_string (error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}
		if (controls_for_read  (command))
			for(i = 0; i < db.get_top (); i++)
				if((Typeof(i) == TYPE_COMMAND
						|| Typeof(i) == TYPE_FUSE
						|| Typeof(i) == TYPE_ALARM)
					&& controls_for_read (i)
					&& ((db[i].get_csucc() == command)
						|| (db[i].get_cfail() == command)))
					notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("alarm", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if(Typeof(i) == TYPE_ALARM
			&& controls_for_read (i)
			&& (!string || re.Match(db[i].get_description())))
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else if (string_prefix("ashcan", descriptor))
	{
		for(i = 0; i < db.get_top (); i++)
			if((Typeof (i) != TYPE_FREE)
				&& Ashcan(i)
				&& controls_for_read (i)
				&& (!string || re.Match(db[i].get_description())))
					notify_censor_colour(player,  player, COLOUR_MESSAGES, "%s", unparse_object(*this, i));
	}
	else
	{
		notify_censor_colour (player, player, COLOUR_ERROR_MESSAGES, "That is not a valid field to search through.");
		set_return_string (error_return_string);
		return_status = COMMAND_FAIL;
		return;
	}
	notify_colour(player,  player, COLOUR_CONTENTS, "***End of List***");
}


char *time_string (time_t interval)
{
	static char buffer[80];
	int years, days, hours, minutes, seconds;

	/* Obviously the year value is an approximation, but good enough for
	   our purposes. */
	interval -= (years = interval / 31557600L) * 31557600L;
	interval -= (days = interval / 86400) * 86400;
	interval -= (hours = interval / 3600) * 3600;
	interval -= (minutes = interval / 60) * 60;
	seconds= interval;
	*buffer = '\0';

	if (years > 0)
		sprintf (buffer, "%d year%s", years, PLURAL (years));
	if (days > 0)
	{
		sprintf (scratch_buffer, "%s%d day%s", (!*buffer) ? "" : (hours > 0 || minutes > 0 || seconds > 0)? ", ":" and ", days, PLURAL (days));
		strcat (buffer, scratch_buffer);
	}
	if (hours > 0)
	{
		sprintf (scratch_buffer, "%s%d hour%s", (!*buffer) ? "" : (minutes > 0 || seconds > 0) ? ", ":" and ", hours, PLURAL (hours));
		strcat (buffer, scratch_buffer);
	}
	if (minutes > 0)
	{
		sprintf (scratch_buffer, "%s%d minute%s", (!*buffer) ? "" : (seconds > 0) ? ", ":" and ", minutes, PLURAL (minutes));
		strcat (buffer, scratch_buffer);
	}
	if (seconds > 0)
	{
		sprintf (scratch_buffer, "%s%ld second%s", (*buffer) ? " and ":"", (long int)interval, PLURAL (interval));
		strcat (buffer, scratch_buffer);
	}
	if (*buffer=='\0')
		sprintf(buffer, "0s");
	return buffer;
}


char *small_time_string (time_t interval)
{
	static char buffer[80];
	int days, hours, minutes;

	interval -= (days = interval / 86400) * 86400;
	interval -= (hours = interval / 3600) * 3600;
	interval -= (minutes = interval / 60) * 60;

	*buffer = '\0';

	if (days > 0)
	{
		sprintf (scratch_buffer, "%d day%s, ", days, PLURAL (days));
		strcat (buffer, scratch_buffer);
	}
	if (hours > 0 || days > 0)
	{
		sprintf (scratch_buffer, "%d hour%s%s", hours, PLURAL (hours), ((days == 0)?(", "):("")));
		strcat (buffer, scratch_buffer);
	}
	if ((minutes > 0 || hours > 0) && days == 0)
	{
		sprintf (scratch_buffer, "%d minute%s%s", minutes, PLURAL (minutes), ((hours == 0)?(" and "):("")));
		strcat (buffer, scratch_buffer);
	}
	if (hours == 0 && days == 0)
	{
		sprintf (scratch_buffer, "%ld second%s", (long int)interval, PLURAL (interval));
		strcat (buffer, scratch_buffer);
	}

	strcat(buffer, ".");

	return buffer;
}


static char *tiny_time_string(time_t interval)
{
	static char buffer[80];
	int days, hours, minutes, seconds;

	interval -= (days = interval / 86400) * 86400;
	interval -= (hours = interval / 3600) * 3600;
	interval -= (minutes = interval / 60) * 60;
	seconds= interval;

	sprintf (buffer, "%dd%dh%dm%ds", days, hours, minutes, seconds);
	return buffer;
}

void
context::do_at_censorinfo(const String& ,const String& )
{
	return_status= COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!Wizard(get_effective_id()) && !Apprentice(get_effective_id()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards and Apprentices can use the @listcensor command.");
		return;
	}
	return_status= COMMAND_SUCC;
	set_return_string(ok_return_string);

	notify_colour(player, player, COLOUR_TITLES, "The censor list contains the following:");

	*scratch_buffer='\0';
	std::set<String>::const_iterator it;
	for (it = rude_words.begin(); it != rude_words.end(); it++)
	{
		if (*scratch_buffer)
			strcat(scratch_buffer,", ");
		strcat(scratch_buffer, (*it).c_str());
	}
	notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer);

	*scratch_buffer='\0';
	notify(player,"");
	notify_colour(player, player, COLOUR_TITLES, "The exclude list contains the following:");
	for (it = excluded_words.begin(); it != excluded_words.end(); it++)
	{
		if (*scratch_buffer)
			strcat(scratch_buffer,", ");
		strcat(scratch_buffer, (*it).c_str());
	}
	notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer);
}
