/* static char SCCSid[] = "%W\t8/8/95"; */
/*
 *	group.c, written by The ReaperMan for group building

Not done yet
	Allow suppersistion of Unanimous and percentage
 */

#include <time.h>
#include "context.h"
#include "command.h"
#include "objects.h"
#include "externs.h"
#include "db.h"
#include "interface.h"

#define	MEMBERS_LIST_NAME "MembersList"

#define UNANIMOUS_THRESHOLD	4
#define VOTE_PERCENTAGE		50

static
dbref	create_support_list    (dbref current_member,
				dbref group,
				dbref member);

static
int	add_up_votes	       (dbref members,
				dbref group,
				char* idtring);
void
context::do_at_group(
const	CString& group_string,
const	CString& player_string)

{
/*@group was written by The ReaperMan around July 1995.

  @group			: reports current build status
  @group <group>		: joins a group
  @group me			: leaves group
  @group create = <founder>	: turns player into group with founder
			     	   as the initial member.
  @group <group> = <player>	: supports player for that group
  @group <group> = !<player>	: withdraws support for that player */

	unsigned int	support = 0;
	unsigned int	tmp;
	unsigned int	against = 0; /* Whether they are supporting or withdrawing */

	dbref	group;
	dbref	canvasser;
	dbref	founder;
	dbref	members;
	dbref	support_list;

	char	idstring[8];

	time_t	now;
	char	time_string[15];

	time(&now);
	sprintf(time_string, "%ld", (long int)now);

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* Obvious bit */
	if(in_command())
	{
		notify(player, "Sorry, @group cannot be used within a command.");
		return;
	}
	/* Only allowed to use @group if they are a builder */
	if(!Builder(player))
	{
		notify(player, "Sorry, you need to be a builder to use @group.");
		return;
	}

	/* Just @group on its own then */
	if(!group_string)
	{
		/* Return group status */
		if(db[player].get_build_id() != player)
		{
			Matcher matcher (db[player].get_build_id(), MEMBERS_LIST_NAME, TYPE_DICTIONARY, GOD_ID);
			matcher.match_variable();
			matcher.match_array_or_dictionary();
			members = matcher.match_result();
			
			notify(player, "You are currently building for: %s.",
				unparse_object(*this, db[player].get_build_id()));
			if(members !=NOTHING)
			{
				sprintf(idstring, "#%d", player);
				notify(player, "The group contains %d member%s, of which %d support%s you.",
				db[members].get_number_of_elements(),
				PLURAL(db[members].get_number_of_elements()),
				(tmp = add_up_votes(members, db[player].get_build_id(), idstring)), tmp==1?"s":"");
			}
		}
		else
			notify(player, "You are currently building for yourself.");
		return;
	}
	/* Create a group */
	else if(!string_compare(group_string, "create"))
	{
		/* See if we are already a group */
		Matcher matcher (player, MEMBERS_LIST_NAME, TYPE_DICTIONARY, GOD_ID);
		matcher.match_variable();
		matcher.match_array_or_dictionary();
		members = matcher.match_result();
		if(members != NOTHING)
		{
			notify(player, "Sorry, there is a strong suspicion that you are already a group.");
			return;
		}
		/* Check if founder exists */
		if((founder = lookup_player(player, player_string)) == NOTHING)
		{
			notify(player, "No such player.");
			return;
		}
		else if(founder == player)
		{
			notify(player, "You can't make yourself the founder");
			return;
		}
		else
		{
			/* Now actually create it */
			/* Make a members list for the group,
			   the founder should get one when they try to
			   join for the first time*/
			members = db.new_object(*new (Dictionary));
			db[members].set_name(MEMBERS_LIST_NAME);
			db[members].set_location(player);
			db[members].set_owner(GOD_ID);
			db[members].set_flag(FLAG_WIZARD);
			db[members].set_flag(FLAG_READONLY);
			db[members].set_flag(FLAG_DARK);
			Settypeof(members, TYPE_DICTIONARY);
			PUSH (members, player, info_items);
			/* Put in the founder member */
			sprintf(idstring, "#%d", founder);
			db[members].set_element(0, idstring, "Founder");

			notify(player, "Group conversion kit fitted.");
			return;
		}
	}
	/* Does group/player exist */
	else if((group = lookup_player(player, group_string)) == NOTHING)
	{
		notify(player, "No such group.");
		return;
	}

	/* If they are @grouping themselves then reset their build_id */
	if(group == player)
	{
		db[player].reset_build_id(player);
		notify(player, "You are now building for yourself.");
		return;
	}

	/* Checkout the Canvasser (the second argument) */
	if(!player_string)
	{
		canvasser = NOTHING;
	}
	else
	{
		switch (*player_string.c_str())
		{
			case '!':
				
				if((canvasser = lookup_player(player, player_string.c_str()+1)) == NOTHING)
				{
					notify(player, "No such player.");
					return;
				}
				against = 1;
				break;
			default:
				if((canvasser = lookup_player(player, player_string)) == NOTHING)
				{
					notify(player, "No such player.");
					return;
				}
				against = 0;
				break;
		}
	}

	/* Okay we're done the bit with just @group
	   lets move onto, erm, joining a group */

	/* Find the group dictionary on the group player */
	Matcher matcher (group, MEMBERS_LIST_NAME, TYPE_DICTIONARY, GOD_ID);
	matcher.match_variable();
	matcher.match_array_or_dictionary();
	members = matcher.match_result();

	/* Make up an ID string ("#42") for matching in the dictionary */
	sprintf(idstring,"#%d", player);
	if(canvasser == NOTHING) /* i.e. they are not trying to
				    do any supporting or withdrawing */
	{
		if(members != NOTHING)
		{
			/* Check it is owned by GOD and set Wizard */
			if((db[members].get_owner() != GOD_ID) || !Wizard(members))
			{
				notify(player, "Group Dictionary Error.");
				return;
			}
			/* and a dictionary */
			else if (Typeof(members) != TYPE_DICTIONARY)
			{
				notify(player, "Your group dictionary has transmuted to a higher form.");
				return;
			}

			/* We have found a members dictionary and so we check to
			   see if we are already a member */
			if((tmp = db[members].exist_element(idstring)))
			{
				/* We're already on the table plan, let's eat*/
				db[player].set_build_id(group);
				/* Let's note what time we ate so we don't get fat */
				db[members].set_element(tmp, idstring, time_string);

				notify(player, "You are now building for %s.", db[group].get_name().c_str());
			}
			else
			{
				/* Oh dear, I'll have to see if the members will
				   have you sir, I'll just go and check each of them
				   in turn and count up how much support you have. If
				   it's enough then you may dine otherwise you can
				   just bugger off ... sir */

				/* Count up the votes to see if we get in*/
				support = add_up_votes(members, group, idstring);

				/* I belive that now we have a total for the
				   support for this person so we can
				   calculate a threshold and then fire
				   John Major, Vulcans Rule */

				if((db[members].get_number_of_elements()) <= UNANIMOUS_THRESHOLD)
					if(support == db[members].get_number_of_elements())
					{
						/* Add to group */
						db[members].set_element(0, idstring, time_string);
						create_support_list(player, group, members);
						notify(player, "You are now a member of: %s.", db[group].get_name().c_str());
						db[player].set_build_id(group);
						return;
					}
					else
					{
						/*Don't add to group*/
						notify(player, "Sorry, you need unanimous support to join this group.");
						return;
					}
				else
					if(support >= (db[members].get_number_of_elements() * VOTE_PERCENTAGE / 100))
					{
						/* Add to group */
						db[members].set_element(0, idstring, time_string);
						create_support_list(player, group, members);
						notify(player, "You are now a member of: %s.", db[group].get_name().c_str());
						db[player].set_build_id(group);
						return;
					}
					else
					{
						/*Don't add to group */
						notify(player, "Sorry Mr Redwood, you gained %d votes and you needed %d to join.", support, db[members].get_number_of_elements() * VOTE_PERCENTAGE / 100);
						return;
					}
			}
				
		}
		else
		{
			/* The was no group dictionary on the group player */
			notify(player, "Sorry, that is not a group.");
			return;
		}
	}
	else 	/* This means that a valid canvasser was found */
		/* And a valid group ( I hope ) */
	{
		/* Support and Withdraw in this section */

		/* Check whether they are a member of the group */
		if(!db[members].exist_element(idstring))
		{
			notify(player, "You are not a member of that group.");
			return;
		}
		/*Check existence of dict*/
		Matcher matcher (player, (CString)db[group].get_name(), TYPE_DICTIONARY, GOD_ID);
		matcher.match_variable();
		matcher.match_array_or_dictionary();
		if((support_list = matcher.match_result()) == NOTHING)
		{
			/* No support list, give them one */
			support_list = create_support_list(player, group, members);
		}
		else if(!Wizard(support_list) || (db[support_list].get_owner() != GOD_ID))
		{
			notify(player, "Support list error.");
			return;
		}
		/* Okay, they have a support list, we know whether it
		   is for or against, we know they are in the group
		   and we know who they are supporting or !ing. */

		/* Create a new ID string for the canvasser */
		sprintf(idstring,"#%d", canvasser);
		if((tmp = db[support_list].exist_element(idstring)) == 0)
		{
			if(against)
			{
				notify(player, "That player was not supported.");
				return;
			}
			else
			{
				db[support_list].set_element(tmp, idstring, "1");
				notify(player, "Supported.");
				return;
			}
		}
		else
		{
			if(against)
			{
				db[support_list].destroy_element(tmp);
				notify(player, "Support withdrawn.");
			}
			else
			{
				notify(player, "That player is already supported.");
				return;
			}
		}


		/* Now we must check to see if the person that is being
		   withdrawn from, still has enough votes */

		support = add_up_votes(members, group, idstring);
		/* Now it doesn't matter it they succeed because even
		if they aren't in the group, they will be next time
		they join. If they fail then they must be removed
		from the group immediately (in case they are destroying
		our world (but they're not devils, they're Martians)).*/

		notify(player, "NumOEle %d, Unan %d, Percent %d, Total %d", db[members].get_number_of_elements(), UNANIMOUS_THRESHOLD, VOTE_PERCENTAGE, (db[members].get_number_of_elements() * VOTE_PERCENTAGE) / 100);
		if(db[members].get_number_of_elements() <= UNANIMOUS_THRESHOLD)
		{
			if(support < db[members].get_number_of_elements())
			{
				/* Remove from group */
				tmp = db[members].exist_element(idstring);
				db[members].destroy_element(tmp);
				if(db[canvasser].get_build_id() == group)
				{
					db[canvasser].reset_build_id(canvasser);
					if(Connected(canvasser))
						notify(canvasser, "Sorry, you are now building for yourself. Et tu Brute.");
				}

				/* Remove the support list from the player leaving the
				   group, if they have one which they should bloody well
				   do by this time */
				Matcher matcher (canvasser, (CString)db[group].get_name(), TYPE_DICTIONARY, GOD_ID);
				matcher.match_variable();
				matcher.match_array_or_dictionary();
				if((support_list = matcher.match_result()) != NOTHING)
				{
					if(Wizard(support_list) && (db[support_list].get_owner() == GOD_ID))
						db[support_list].destroy(support_list);
				}

				return;
			}
		}
		else
		{
			notify(player, "Vote thingy: %d", (db[members].get_number_of_elements() * VOTE_PERCENTAGE) / 100);
			if((support < ((db[members].get_number_of_elements() * VOTE_PERCENTAGE) / 100)) ||
				support < UNANIMOUS_THRESHOLD)
			{
					/* Remove from group */
					tmp = db[members].exist_element(idstring);
					db[members].destroy_element(tmp);
					if(db[canvasser].get_build_id() == group)
					{
						if(Connected(canvasser))
							notify(canvasser, "Sorry, you are now building for yourself. Huh, politics.");
						db[canvasser].reset_build_id(canvasser);
					}
					/* Remove the support list from the player leaving the
					   group, if they have one which they should bloody well
					   do by this time */
					Matcher matcher (canvasser, (CString)db[group].get_name(), TYPE_DICTIONARY, GOD_ID);
					matcher.match_variable();
					matcher.match_array_or_dictionary();
					if((support_list = matcher.match_result()) != NOTHING)
					{
						if(Wizard(support_list) && (db[support_list].get_owner() == GOD_ID))
							db[support_list].destroy(support_list);
					}
			}
		}
	}

}

static
dbref create_support_list(
dbref	current_member,
dbref	group,
dbref	members)
{
	dbref support_list;
	unsigned int	j;

	notify(GOD_ID, "I'm creating a support list on: %d", current_member);

	payfor(GOD_ID, DICTIONARY_COST);
	support_list = db.new_object(*new (Dictionary));
	db[support_list].set_name(db[group].get_name());
	db[support_list].set_location(current_member);
	db[support_list].set_owner(GOD_ID);
	db[support_list].set_flag(FLAG_WIZARD);
	db[support_list].set_flag(FLAG_READONLY);
	db[support_list].set_flag(FLAG_DARK);
	Settypeof(support_list, TYPE_DICTIONARY);
	PUSH (support_list, current_member, info_items);

	/*And copy all the members into it, assuming that
	  they support everyone currently in the group*/
	for(j = 1; j <= db[members].get_number_of_elements(); j++)
		db[support_list].set_element(0, db[members].get_index(j), "1");

	return(support_list);
}

static
int add_up_votes(dbref	members,
		 dbref	group,
		 char *	idstring)
{
	dbref	current_member;
	dbref	support_list;
	unsigned int	i;
	int	tmp;
	int	support = 0;
	int	woo;

	/* Otherwise count up the votes to see if we get in*/
	for(i = 1; i <= db[members].get_number_of_elements(); i++)
	{
		/*Check existence of member*/
		sscanf(db[members].get_index(i).c_str(),"#%d", &current_member);
		if(Typeof(current_member) != TYPE_PLAYER)
		{
			/*Delete current_member from list*/
			/*Because they are not a player anymore*/
			db[members].destroy_element(i);
		}
		else
		{
				/*Check existence of dict*/
				Matcher matcher (current_member, (CString)db[group].get_name(), TYPE_DICTIONARY, GOD_ID);
				matcher.match_variable();
				matcher.match_array_or_dictionary();
				if((support_list = matcher.match_result()) == NOTHING)
					/*Give them a new support list because they have mislaid the old one*/
					support_list = create_support_list(current_member, group, members);

				/*Check existence of canvasser in support list*/
				if((tmp = db[support_list].exist_element(idstring)))
				{
				/*Add to total*/
					woo = atoi(db[support_list].get_element(tmp).c_str());
					support += woo;
				/*Otherwise that member has not supported this new person*/
					notify(GOD_ID, "Support is: %d (%s)", support, db[support_list].get_element(tmp).c_str());
				}
		}
	}

	return(support);
}
