/* static char SCCSid[] = "@(#)predicates.c	1.32\t10/17/95"; */
/** \file
 * Predicates for testing conditions. This lot are independent of netmud and have been separated out into their own compilation unit as a result.
 */

#include "copyright.h"
#include "db.h"
#include "config.h"
#include "externs.h"
#include "context.h"


/** The flag map defines what flags are allowed to be set on each object type.
   Used as the first test in set and as a check in the sanity-check */

unsigned char flag_map[][64] = 
{
/*Type unassigned yet*/
	{0},

/*NO_TYPE*/
	{0},

/*TYPE_FREE*/
	{0},

/*TYPE_PLAYER*/
	{FLAG_NATTER, FLAG_APPRENTICE, FLAG_ASHCAN, FLAG_BUILDER, 
	 FLAG_CENSORED, FLAG_CENSORALL, FLAG_CENSORPUBLIC,
	 FLAG_CHOWN_OK, FLAG_COLOUR, FLAG_CONNECTED, FLAG_FCHAT,
	 FLAG_FEMALE, FLAG_FIGHTING, FLAG_HAVEN, FLAG_DEBUG,
	 FLAG_HOST, FLAG_INHERITABLE, FLAG_LISTEN, FLAG_LINENUMBERS,
	 FLAG_MALE, FLAG_NOHUHLOGS, 
	 FLAG_NEUTER, FLAG_NUMBER, FLAG_REFERENCED, FLAG_SILENT, 
	 FLAG_READONLY, FLAG_TRACING, FLAG_VISIBLE, FLAG_WIZARD,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 
	 FLAG_WELCOMER, FLAG_XBUILDER, FLAG_PRETTYLOOK,
	 FLAG_DONTANNOUNCE, FLAG_RETIRED, FLAG_LITERALINPUT,
	 FLAG_NO_EMAIL_FORWARD,
	 FLAG_GOD_SET_GOD, FLAG_GOD_WRITE_ALL, FLAG_GOD_BOOT_ALL,
	 FLAG_GOD_CHOWN_ALL, FLAG_GOD_MAKE_WIZARD, FLAG_GOD_PASSWORD_RESET, FLAG_GOD_DEEP_POCKETS,
	 0},

/*TYPE_PUPPET*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_CONNECTED, FLAG_FEMALE, FLAG_FIGHTING,
	 FLAG_DEBUG, FLAG_BUILDER,
	 FLAG_INHERITABLE, FLAG_LISTEN, FLAG_MALE, FLAG_NEUTER, FLAG_TRACING,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, FLAG_LITERALINPUT, 0},

/*TYPE_ROOM*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_HAVEN,
	 FLAG_PRIVATE,
	 FLAG_INHERITABLE, FLAG_LOCKED, FLAG_SILENT, FLAG_STICKY,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE,
	 FLAG_WIZARD, FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ABODE_OK, FLAG_LINK_OK, FLAG_JUMP_OK, FLAG_CENSORED, FLAG_PUBLIC,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_THING*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_HAVEN, FLAG_FIGHTING,
	 FLAG_PRIVATE,
	 FLAG_INHERITABLE, FLAG_LOCKED, FLAG_OPAQUE,
	 FLAG_OPEN, FLAG_OPENABLE, FLAG_STICKY,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_EXIT*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_INHERITABLE,
	 FLAG_PRIVATE,
	 FLAG_LOCKED, FLAG_OPAQUE, FLAG_REFERENCED, FLAG_READONLY,
	 FLAG_VISIBLE, FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_COMMAND*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_INHERITABLE,
	 FLAG_PRIVATE, FLAG_PUBLIC,
	 FLAG_LOCKED, FLAG_OPAQUE, FLAG_SILENT, FLAG_REFERENCED,
	 FLAG_READONLY, FLAG_TRACING, FLAG_VISIBLE,
	 FLAG_WIZARD, FLAG_BACKWARDS,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_VARIABLE*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_PRIVATE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_PROPERTY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK,
	 FLAG_PRIVATE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, FLAG_ERIC, 0},

/*TYPE_ARRAY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_PRIVATE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, FLAG_ERIC, 0},

/*TYPE_DICTIONARY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_PRIVATE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, FLAG_ERIC, 0},

/*TYPE_ALARM*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_INHERITABLE, FLAG_LOCKED, 
	 FLAG_PRIVATE,
	 FLAG_OPAQUE, FLAG_READONLY, FLAG_REFERENCED, FLAG_VISIBLE,
	 FLAG_WIZARD, 0},

/*TYPE_FUSE*/
	{
#ifdef ABORT_FUSES
	 FLAG_ABORT,
#endif
	 FLAG_PRIVATE,
	 FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_INHERITABLE, FLAG_LOCKED, 
	 FLAG_OPAQUE, FLAG_READONLY, FLAG_REFERENCED, FLAG_STICKY,
	 FLAG_TOM, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_TASK*/
	{0},
/*TYPE_SEMAPHORE*/
	{0}
};


/**
 * Return whether 'flag' is allowed to be set on an object of type 'type'.
 */

bool
is_flag_allowed(
typeof_type	type,
flag_type	flag)
{
	int current = 0;
	int count = 0;
	do
	{
		if((current = flag_map[type][count]) == flag)
			return(true);
		count++;
	}
	while(current);
	return(false);
}


/**
 * Return whether the player identified in context c can, at least in theory, open an exit into 'where'.
 * TODO: Work out what this is used for, as the logic is hopelessly out of date.  PJC, 2003-10-26.
 */

bool
can_link_to (
const	context	&c,
const	dbref	where)

{
    return (where >= 0
	   && where < db.get_top ()
	   && (Typeof(where) == TYPE_ROOM || Container(where))
	   && (c.controls_for_write (where)));
}


/**
 * Can this context make changes to 'what'?  Rules:
 *
 * - what must be a legal dbref
 * - what must not be readonly
 * - At least one of the following conditions must hold:
 * -- The effective ID of the player is GOD; or
 * -- The player is really a Wizard and they're not trying to control another Wizard; or
 * -- The player is effectively a Wizard and 'what' has no wizard flag set; or
 * -- The player owns 'what'; or
 * -- The player is part of a build group that owns 'what'; or
 * -- The player owns what's controller.
 */

const bool
context::controls_for_write (
const	dbref	what)
const

{
	return(what >= 0 && what < db.get_top ()
		&& (!Readonly (what))			/* Nothing set READONLY can be changed */
		&& (WriteAll(get_effective_id())		/* GOD controls everything */
			|| (Wizard(player) && (player == get_effective_id()) && (!Wizard (what) || (Typeof(what) != TYPE_PLAYER)))	/* WIZARDs control everything except other WIZARD players */
			|| (Wizard(get_effective_id()) && !Wizard (db[what].get_owner()))	/* Effective WIZARDs control mortals */
			|| (get_effective_id() == db[what].get_owner())				/* Everyone controls themselves */
			|| (db[get_effective_id()].get_build_id() == db[what].get_owner())			/* Controls for their group */
			|| (get_effective_id() == db [db [what].get_owner()].get_controller ())));	/* Players own their puppets and puppets' stuff */
}


/**
 * Can this context see all properties of 'what'?  Rules:
 *
 * - what must be a legal dbref
 * - At least one of the following conditions must hold:
 * -- The context is a real Apprentice or Wizard;
 * -- The context is an effective Apprentice or Wizard;
 * -- The effective ID or the real ID of the context owns the object;
 * -- The effective ID or the real ID of the context controls the object's owner;
 * -- The context's build group owns the object;
 * -- The object is set Visible.
 */

const bool
context::controls_for_read (
const	dbref	what)
const

{
	return ::controls_for_read(player, what, get_effective_id());
}

bool
controls_for_read (
const	dbref	player,
const	dbref	what,
const	dbref	effective_id)

{
	return (what >= 0
		&& what < db.get_top ()
		&& (	   (effective_id == UNPARSE_ID)					/* UNPARSE_ID can read anything */
			|| Wizard(effective_id)						/* Effective WIZARDS can read anything */
			|| (effective_id == db [what].get_owner())			/* Everyone controls themselves */
			|| (effective_id == db [db [what].get_owner()].get_controller ()) /* Everyone controls themselves */
			|| Wizard(player)						/* Real WIZARDS can read anything */
			|| (player == db [what].get_owner())				/* Everyone controls themselves */
			|| (player == db [db [what].get_owner()].get_controller ())	/* Everyone controls themselves */
			|| (db[player].get_build_id() == db[what].get_owner())		/* Controls for their group */
			|| (Visible(what))						/* Everyone can see visible things */
			|| (Apprentice(effective_id))					/* Effective APPRENTICES can read everything */
			|| (Apprentice(player))));					/* APPRENTICES can read everything */
}


/**
 * TODO: I have no idea what this does! PJC 2003-10-26.
 */

bool
can_link (
const	context	&c,
const	dbref	what)

{
	return((Typeof(what) == TYPE_EXIT && db[what].get_destination() == NOTHING)
		|| c.controls_for_write (what) || Visible(what)
		|| (Apprentice(c.get_player ()) && ((Typeof(what) != TYPE_PLAYER) || !Wizard(what))));
}

const bool
context::controls_for_private(const dbref what) const
{
	dbref owner = db[what].get_owner();
	return (get_effective_id() == owner)
		|| (player == owner)
		|| (db[player].get_build_id() == owner);
}

const bool
context::controls_for_look(const dbref what) const
{
	dbref whatloc = db[what].get_location();

	return	Visible(what) ||				// Its visible
		controls_for_read(what) ||			// You control it
		controls_for_read(whatloc) ||			// You control its location
		(what == player) ||				// Looking at yourself
		(whatloc == player) ||				// Something you hold
		(what == db[player].get_location()) ||		// Looking where you are
		(whatloc == db[player].get_location());		// Looking at something in the same place as you
}

