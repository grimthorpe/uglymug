static char SCCSid[] = "@(#)rob.c	1.17\t6/14/95";
#include "copyright.h"

/* rob and kill */

#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "colour.h"

void
context::do_give (
const	char	*recipient,
const	char	*object_or_amount)

{
	dbref	who;
	dbref	object;
	int	amount;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher player_matcher (player, recipient, TYPE_PLAYER, get_effective_id ());
	player_matcher.match_neighbor();
	player_matcher.match_me();
	if(Wizard(player) || Apprentice(player))
	{
		player_matcher.match_player();
		player_matcher.match_absolute();
	}

	/* ok, now we check to see if that person is here */

	if ((who = player_matcher.noisy_match_result()) == NOTHING)
		return;

	if((Typeof(who) != TYPE_PLAYER) && (Typeof(who) != TYPE_PUPPET))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only give to other players.");
		return;
	}

	Matcher thing_matcher (player, object_or_amount, TYPE_THING, get_effective_id ());
	thing_matcher.match_possession ();
	if (Wizard(get_effective_id ()))
		thing_matcher.match_absolute ();
	object = thing_matcher.match_result ();

	/* If we didn't find anything, we're giving something illegal or BPs */
	if (object < 0 || object > db.get_top ())
	{
		amount = atoi (value_or_empty (object_or_amount));

		/* Mortals can't give BPs */
		if ((!Wizard(get_effective_id ())) && (!Apprentice(get_effective_id ())))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
			return;
		}

		else if(amount == 0)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "You must specify a number of building points.");
			return;
		}

		if (player != GOD_ID)
			if ((Wizard(who)) || (Apprentice(who) && Apprentice(player) && !Wizard(player)))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
				return;
			}

		/* check recipient */
	
		if((db[who].get_pennies() + amount > MAX_BUILDING_POINTS) && !Wizard (player))
		{
			notify_colour(player, player, COLOUR_MESSAGES, "That player doesn't need that many building points!");
			return;
		}
	
		/* try to do the give - strictly not necessary, but cuts code changes down*/
		if(!payfor(get_effective_id (), amount))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have that many building points to give!");
		}
		else
		{
			/* they can do it */
			/* now do the 'good grammar' checks... */
			if (amount < 0)
			{
				notify_colour(player, player, COLOUR_MESSAGES, "You take %d %s away from %s.",
				       (-amount),
				       amount == -1 ? "building point" : "building points",
				       getname_inherited (who));
				if(Typeof(who) == TYPE_PLAYER)
				{
					notify_colour(who, player, COLOUR_ERROR_MESSAGES, "%s takes %d %s away from you.",
						getname_inherited (player),
						(-amount),
						amount == -1 ? "building point" : "building points");
					return_status = COMMAND_SUCC;
				}
			}
			else
			{
				notify_colour(player, player, COLOUR_MESSAGES, "You give %d %s to %s.",
					amount,
					amount == 1 ? "building point" : "building points",
					getname_inherited (who));
				if(Typeof(who) == TYPE_PLAYER)
				{
					notify_colour(who, player, COLOUR_MESSAGES, "%s gives you %d %s.",
						getname_inherited (player),
						amount,
						amount == 1 ? "building point" : "building points");
					return_status = COMMAND_SUCC;
				}
			}
			db[who].set_pennies(db[who].get_pennies() + amount);
		}
		return;
	}

	/* ok, so now we have got the victim and the object. */
	/* so now we check for container constraints......   */
	/* ie if the victim is on the object's contents list.*/
	/* Although this might seem silly, Wizards could in  */
	/* effect, mess up all the contents of the db, and   */
	/* make several players slightly unhappy about the   */
	/* the fact that their characters don't seem to be in*/
	/* the db any more. */

	/* We now yell at the player if they did something silly */
	if (thing_matcher.noisy_match_result () == NOTHING)
		return;
	if (Typeof (object) != TYPE_THING)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only give Things to other players.");
		return;
	}

	if (Container (object))
	{
		if (member (who, object))
		{
			notify_censor_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't give %s%s to %s.", getarticle (object, ARTICLE_LOWER_DEFINITE), getname_inherited (object), getname_inherited (who));
			return;
		}
	}
	/* now check to see if the 'victim' is able to get the object */
	context	who_context (who);

	if (!can_doit(who_context, object, "You can't give that player that object."))
	{
		notify_censor_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't give %s%s to %s.", getarticle (object, ARTICLE_LOWER_DEFINITE), getname_inherited (object), getname_inherited (who));
		return;
	}
	notify_censor_colour(player, player, COLOUR_MESSAGES, "You gave %s %s%s.", getname_inherited (who), getarticle (object, ARTICLE_LOWER_DEFINITE), getname_inherited (object));
	notify_public_colour(who, player, COLOUR_MESSAGES, "%s gives you %s%s.", getname_inherited (player), getarticle (object, ARTICLE_LOWER_INDEFINITE), getname_inherited (object));
	moveto (object, who);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}
