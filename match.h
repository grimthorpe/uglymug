#include "copyright.h"

#include <stdlib.h>

#ifndef	_MATCH_H
#define	_MATCH_H

#ifndef	_DB_H
#include "db.h"
#endif	/* !_DB_H */

#define	NOMATCH_MESSAGE		"I don't see that here."
#define	AMBIGUOUS_MESSAGE	"I don't know which one you mean!"


/*
 * Enumerated type as to what kind of match is being done.
 *
 * Do not alter this without updating matcher_fsm_array in match.c.
 */

enum	Matcher_path
{
	MATCHER_PATH_NONE		= 0,
	MATCHER_PATH_COMMAND_FULL	= 1,
	MATCHER_PATH_COMMAND_PLAYER	= 2,
	MATCHER_PATH_COMMAND_REMOTE	= 3,
	MATCHER_PATH_COMMAND_BOUNDED	= 4,
	MATCHER_PATH_COMMAND_FROM_LOC	= 5,
	MATCHER_PATH_FUSE_THING		= 6,
	MATCHER_PATH_FUSE_AREA		= 7
};


/*
 * Enumerated type as to which state the matcher is in ATM.
 *
 * Do not alter this without updating matcher_fsm_array in match.c, and the dimension in matcher_fsm_array below.
 */

enum	Matcher_state
{
	/* Any state between here... */
	MATCHER_STATE_INVALID		= 0,
	MATCHER_STATE_FINISHED		= 1,
	/* ... and here causes the matcher to give up. */
	MATCHER_STATE_SETUP		= 2,
	MATCHER_STATE_COMMAND_PLAYER	= 3,
	MATCHER_STATE_COMMAND_PCONTENTS	= 4,
	MATCHER_STATE_COMMAND_LOCATION	= 5,
	MATCHER_STATE_COMMAND_LCONTENTS	= 6,
	MATCHER_STATE_COMMAND_AREA	= 7,
	MATCHER_STATE_COMMAND_LAST	= 8,
	MATCHER_STATE_FUSE_THING	= 9,
	MATCHER_STATE_FUSE_AREA		= 10
};


extern	Matcher_state matcher_fsm_array [] [11];
extern	Matcher_state matcher_state_array [];


/* Usage:
 *	Matcher matcher (player, name, type);
 *	Matcher matcher (player, base_object, type);
 *	matcher.match_this ();
 *	matcher.match_that();
 *	...
 *	object = matcher.[noisy_]match_result ();
 *	index_attempt = matcher.match_index_attempt_result();
 *	index = match_index_result();
 */

class	Matcher
{
private:
	Matcher& operator=(const Matcher&); // DUMMY
    private:
	dbref			exact_match;		/* holds result of exact match */
	bool			checking_keys;
	dbref			first_match;		/* holds result of the first match */
	dbref			last_match;		/* holds result of last match */
	int			match_count;		/* holds total number of inexact matches */
	dbref			effective_who;		/* effective player who is being matched around */
	dbref			real_who;		/* real player who is being matched around */
	const	char		*match_name;		/* name to match */
	int			preferred_type;		/* preferred type */

	char			*index;			/* Index into array or dictionary */
	bool			index_attempt;		/* Whether the player tried to define an index */
	dbref			absolute;		/* Cached absolute ID or NOTHING */

	bool			gagged;			/* Whether we're working silently or not */

	/* This is here to support name matching */
	dbref			absolute_loc;		/* location specified by ':' */
	char			*beginning;		/* begining of above for freeing */

	/* Following to support command and fuse search restarts */
	bool			already_checked_location;
	Matcher_path		path;
	Matcher_state		current_state;
	dbref			internal_restart;
	dbref			internal_inheritance_restart;
	dbref			location;
	dbref			thing;
	dbref			end_location;		/* the last room of an area command search */

	/* Private members' bills (sorry, functions) */
	dbref			absolute_name			();
	void			match_set_state			()			{ current_state = matcher_state_array [path]; }
	void			match_list			(dbref head);
	void			match_list_variable		(dbref head);
	void			match_command_remote		();
	bool			match_command_internal		();
	bool			match_fuse_internal		();
	void			match_contents_list		(dbref base);
	void			match_continue			();
	void			match_fuse_list			(dbref base);
	void			match_thing_remote		(dbref loc);
	void			match_variable_list		(dbref base);
    public:
				Matcher				(const Matcher &src);
				Matcher				(dbref player, const String& name, object_flag_type type, dbref effective_player);
				Matcher				(dbref player, dbref object, object_flag_type type, dbref effective_player);
				~Matcher			();
	void			set_beginning			(const char *str);
	void			check_keys			();
	void			work_silent			()			{ gagged = true; }
	void			match_me			();
	void			match_here			();
	void			match_player			();
	void			match_absolute			();
	void			match_bounded_area_command	(dbref start_location, dbref end_location);
	void			match_command			();
	void			match_command_from_location	(dbref start_location);
	void			match_count_down_fuse		();
	void			match_exit			();
	void			match_exit_remote		(dbref loc);
	void			match_fuse_or_alarm		();
	void			match_neighbor			();
	void			match_neighbor_remote		(dbref loc);
	void			match_player_command		();
	void			match_possession		();
	void			match_variable			();
        void                    match_variable_remote           (dbref loc);
	void			match_array_or_dictionary	();
	void			match_restart			();
	void			match_everything		();
	dbref			match_result			();
	dbref			last_match_result		();
	dbref			noisy_match_result		();
	const String		match_index_result		();
	bool			match_index_attempt_result	();
	dbref			choose_thing			(dbref, dbref);
	dbref			get_leaf			()			{ return (thing); }
	const String		get_match_name			() const { return match_name; }

	/* The following exist to support @?my match and @?my leaf for bits of code that do their own matching. */
	void			set_my_match			(dbref my_match)	{ exact_match = my_match; }
	void			set_my_leaf			(dbref my_leaf)		{ thing = my_leaf; }

	bool			was_absolute			()			{ return (absolute != NOTHING) || (absolute_loc != NOTHING); }
};

#endif	/* _MATCH_H */
