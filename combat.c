#include <time.h>
#include "match.h"
#include "db.h"
#include "objects.h"
#include "context.h"
#include "externs.h"
#include "command.h"
#include "interface.h"

/* I suppose all these should really be somewhere else, but then again... */
#define PROBABILITY_TWEAK	18 /* Messing around with this can be fun :) */
#define DEFAULT_WEAPON_VALUE	1
#define DEFAULT_ARMOUR_VALUE	1
#define MAX_PLAYER_FIELD_LIMIT	50
#define MIN_PLAYER_FIELD_LIMIT	4
#define MAX_WEAPON_DAMAGE	100
#define MAX_WEAPON_SPEED	100
#define MAX_WEAPON_RANGE	100
#define MAX_ARMOUR_PROTECTION	100
#define MAX_AMMO_COUNT		150


extern "C"	long	lrand48	();


void
context::do_attack (
const	char	*player_name,
const	char	*)

{
	int attack_total=PROBABILITY_TWEAK, level_a, level_v, roll, temp;
	dbref victim, weapon, armour;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if(in_command())	/* If we're in a command, they can get lost. */
	{
		notify(player, "Sorry, combat commands cannot be used within commands.");
		return;
	}

	if(!Fighting(player)) /* Doh! */
	{
		notify(player, "Only people set Fighting can attack each other.");
		return;
	}

	if(blank(value_or_empty(player_name))) /* Is anyone really this numb? */
	{
		notify(player, "You need to specify who you wish to attack!");
		return;
	}

/* So the attack seems valid so far, now we need the victim... */
/* Will eventually deal with the possibility of an NPC but for now, sack it. */

	Matcher victim_matcher(player, player_name, TYPE_PLAYER, player);
	victim_matcher.match_everything();

	if((victim=victim_matcher.noisy_match_result()) == NOTHING)
		return;

	if(victim==player)
	{
		notify(player, "Killing yourself won't solve the problem. Seek help instead.");
		return;
	}

	if(!Fighting(victim)) /* We can't go round killing bystanders */
	{
		notify(player, "That player is not set Fighting!");
		return;
	}

/* Ammo accounting for 'ranged' weapons. */
	if((weapon=db[player].get_weapon()) != NOTHING && db[weapon].get_ammo_parent()!=NOTHING)
	{
		if((temp=db[weapon].get_ammunition())>0)
			db[weapon].set_ammunition(temp-1);
		else
		{
			notify(player, "Sorry, but you've run out of ammunition!");
			return;
		}
	}

/* Now we have our attacker and our victim, we can really get started... */

/* M   M   A   IIIII N   N      CCC   OOO  M   M BBBB    A   TTTTT
   MM MM  A A    I   NN  N     C     O   O MM MM B   B  A A    T
   M M M A   A   I   N N N     C     O   O M M M BBBB  A   A   T
   M   M AAAAA   I   N  NN     C     O   O M   M B   B AAAAA   T
   M   M A   A IIIII N   N      CCC   OOO  M   M BBBB  A   A   T

		     H   H EEEEE RRRR  EEEEE
		     H   H E     R   R E
		     HHHHH EEE   RRRR  EEE
		     H   H E     R  R  E
		     H   H EEEEE R   R EEEEE
*/
	


	attack_total += level_a = db[player].get_level();
	attack_total += db[player].get_strength();
	attack_total -= level_v = db[victim].get_level();
	attack_total -= db[victim].get_dexterity();

	if(weapon == NOTHING)
		attack_total += DEFAULT_WEAPON_VALUE;
	else
	{
		attack_total += db[weapon].get_speed() * db[weapon].get_degradation() / 100;
	}

	if((armour=db[victim].get_armour()) == NOTHING)
		attack_total -= DEFAULT_ARMOUR_VALUE;
	else
	{
		attack_total -= db[armour].get_protection() * db[armour].get_degradation() / 100;
	}

	if(attack_total>50)
		attack_total=50;
	else if(attack_total<1)
		attack_total=1;


/* Time to roll the dice of fate... */

	roll=(lrand48() % 50)+1;
	
	if(roll==1)
	{
		/* We have critically hit :) */
		notify(player, "Critical hit!");
	}
	else if(roll==50)
		{
		/* We have critically fumbled :( */
			notify(player, "Critical miss!");
	     		return;
		}

	if(roll>attack_total)
	{
		/* We have missed the target :( */
		if((temp=(lrand48() % 20)-db[player].get_dexterity())>-1 && weapon!=NOTHING)
		{
			db[weapon].set_degradation(db[weapon].get_degradation()-temp);
			notify(player, "You miss your target and as a result your weapon degrades");
			notify_public(victim, player, "%s tries to hit you, but misses, damaging their weapon.", db[player].get_name());
		}
		else
		{
			notify(player, "You miss your target");
			notify_censor(victim, player, "%s tries to hit you, but misses.", db[player].get_name());
		}
		return;
	}
	else
	{
		/* We have hit the target :) */
		temp=db[player].get_strength() / 5;
		if(weapon!=NOTHING)
			temp+=db[weapon].get_damage() * db[weapon].get_degradation() / 100;
		if(armour!=NOTHING)
			temp-=db[armour].get_protection() * db[armour].get_degradation() /100;
		db[victim].set_hit_points(db[victim].get_hit_points() -temp);
		if(db[victim].get_hit_points() < 1)
		{
			/* It's worse than that, he's dead Jim. */
			notify_censor(victim, player,  "You have been hit by %s for %d hit points of damage, killing you.", db[player].get_name(), temp);
			temp=(level_a - level_v);
			if(temp<1)
			{
				if(temp<-4)
					temp=-4;
				db[victim].set_experience(db[victim].get_experience()*(9+temp)/10);
				temp=level_v*10;
			}
			else
			{
				db[victim].set_experience(db[victim].get_experience()*9/10);
				temp=level_v*10/temp;
			}

			while(level_v!=db[victim].get_level())
			{
				if(db[victim].get_max_hit_points()>db[victim].get_level()*10)
				{
					db[victim].set_max_hit_points(db[victim].get_max_hit_points()-lrand48() % 10-10-db[victim].get_constitution()/5);
					if(db[victim].get_max_hit_points()<db[victim].get_level()*10)
						db[victim].set_max_hit_points(db[victim].get_level()*10);
				}
				level_v--;
			}
			db[victim].set_hit_points(db[victim].get_max_hit_points());
			if(db[victim].get_constitution()>4)
				db[victim].set_constitution(db[victim].get_constitution()-1);
/*			moveto(victim,AFTERLIFE);*/

/* So thats the victim dealt with, now to reward(?) the victor */

			notify(player, "You hit and kill your victim!");
			db[player].set_experience(db[player].get_experience() + temp);
			notify(player, "You just gained %d experiece.", temp);
			if(level_a<(temp=db[player].get_level()))
			{
				notify(player, "This takes you to level %d.", temp);
				db[player].set_max_hit_points(db[player].get_max_hit_points()+(temp=lrand48() % 10+10+db[player].get_constitution()/5));
				db[player].set_hit_points(db[player].get_hit_points()+temp);
			}

		}	
		else 
		{
			notify(player, "You hit your victim.");
			notify_censor(victim, player, "You have been hit by %s for %d hit points of damage.", db[player].get_name(), temp);
		}

	}
}


void
context::do_attributes (
const	char	*item_name,
const	char	*)

{
	dbref item;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if(blank(value_or_empty(item_name)))
		item=player;
	else
	{
		Matcher item_matcher(player, item_name, TYPE_NO_TYPE, player);
		item_matcher.match_everything();
		if((item=item_matcher.noisy_match_result())==NOTHING)
			return;
	}
	
	if(!(CombatItem(item)) && (Typeof(item) != TYPE_PLAYER))
	{
		notify(player, "That object has no combat attributes.");
		return;
	}

	notify(player, "Requested Combat Attributes:");

	switch(Typeof(item))
	{
		case TYPE_PLAYER:
			notify(player, "Strength     : %d", db[item].get_strength());
			notify(player, "Dexterity    : %d", db[item].get_dexterity());
			notify(player, "Constitution : %d", db[item].get_constitution());
			notify(player, "Experience   : %d (Level %d)", db[item].get_experience(), db[item].get_level());
			notify(player, "Hit Points   : %d from a maximum of %d", db[item].get_hit_points(), db[item].get_max_hit_points());
			break;
		case TYPE_WEAPON:
			notify(player, "Hit Probability : %d", db[item].get_speed());
			notify(player, "Damage Modifier : %d", db[item].get_damage());
			notify(player, "Degradation     : %d\%", db[item].get_degradation());
			if(db[item].get_ammo_parent() != NOTHING)
			{
				notify(player, "Ammunition Left: %d %s", db[item].get_ammunition(), db[db[item].get_ammo_parent()].get_name());
				notify(player, "Range          : %d", db[item].get_range());
			}
			break;
		case TYPE_ARMOUR:
			notify(player, "Protection Value : %d", db[item].get_protection());
			notify(player, "Degradation      : %d\%", db[item].get_degradation());
			break;
		case TYPE_AMMUNITION:
			notify_censor(player, player, "There are %d %s", db[item].get_count(), db[db[item].get_parent()].get_name());
			break;
		default:
			notify(player, "I don't know how to deal with that type of combat item. Tell a wizard.");
			Trace( "BUG: Don't know type of combat item in do_attributes.\n");
			return;
	}
	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
}


void
context::do_wear (
const	char	*item_name,
const	char	*)

{
	dbref	item;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if(in_command())
	{
		notify(player, "Sorry, combat commands cannot be used within commands.");
		return;
	}

	if(blank(value_or_empty(item_name)))
	{
		db[player].set_armour(NOTHING);
		set_return_string (ok_return_string);
		return_status = COMMAND_SUCC;
		return;
	}

	Matcher matcher(player, item_name, TYPE_ARMOUR, player);
	matcher.match_possession();
	if((item = matcher.noisy_match_result()) == NOTHING)
		return;
	
	db[player].set_armour(item);

	notify_public(player, player, "You are now wearing %s.", unparse_object_inherited(*this, item));
	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;
}


void
context::do_wield (
const	char	*item_name,
const	char	*)

{
	dbref	item;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if(in_command())
	{
		notify(player, "Sorry, combat commands cannot be used within commands.");
		return;
	}

	if(blank(value_or_empty(item_name)))
	{
		db[player].set_weapon(NOTHING);
		set_return_string (ok_return_string);
		return_status = COMMAND_SUCC;
		return;
	}
		
	Matcher matcher(player, item_name, TYPE_WEAPON, player);
	matcher.match_possession();
	if((item = matcher.noisy_match_result()) == NOTHING)
		return;
	
	db[player].set_weapon(item);
	notify_public(player, player, "You are now wielding %s.", unparse_object_inherited(*this, item));

	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;
}


void
context::do_load (
const	char	*item_name,
const	char	*)

{
	dbref	item;
	dbref	weapon;
	int	temp;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if(in_command())
	{
		notify(player, "Sorry, combat commands cannot be used within commands.");
		return;
	}

	weapon = db[player].get_weapon();

	if(weapon == NOTHING)
	{
		notify_public(player, player, "You must wield a weapon to load ammo.");
		return;
	}

	if(db[weapon].get_ammo_parent() == NOTHING)
	{
		notify(player, "That weapon does not require ammunition. Doh!");
		return;
	}



	Matcher matcher(player, item_name, TYPE_AMMUNITION, player);
	matcher.match_possession();
	if((item = matcher.noisy_match_result()) == NOTHING)
		return;
	
	if(db[item].get_parent() != db[weapon].get_ammo_parent())
	{
		notify(player, "No matter hard you try, you can't fit this type of ammunition into your weapon!");
		return;
	}

	temp=db[weapon].get_ammunition() + db[item].get_count();
	if(temp>MAX_AMMO_COUNT)
	{
		db[item].set_count(temp-MAX_AMMO_COUNT);
		temp=MAX_AMMO_COUNT;
	}
	else
		db[item].set_count(0);
	db[weapon].set_ammunition(temp);
	notify_public(player, player, "Ammunition loaded, Weapon now has %d %s.", temp,db[db[weapon].get_ammo_parent()].get_name());
	db[item].set_flag(FLAG_ASHCAN);

	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;
}

void context::do_modify(const char * target_name, const char * settings)
{
	dbref	target;
	char *	field;
	int	value;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!Wizard(get_effective_id()))
	{
		notify(player, "Sorry, you may not modify that.");
		return;
	}

	Matcher matcher(player, target_name, TYPE_NO_TYPE, get_effective_id());
	matcher.match_everything();
	target = matcher.noisy_match_result();
	if(target == NOTHING)
		return;

	if(!(CombatItem(target)) && (Typeof(target) != TYPE_PLAYER))
	{
		notify(player, "Sorry, you can only modify players and combat items.");
		return;
	}

	if(blank(settings))
	{
		notify(player, "You must specify a field and a value.");
		return;
	}

	if(((field = strchr(settings, ' ')) == NULL) ||
		(*(field + 1) == '\0'))
	{
		notify(player, "You must specify a value after the field.");
		return;
	}

	*field++ = '\0';

	value = atoi(field);

	switch(Typeof(target))
	{
		case TYPE_PLAYER:
			if(!string_compare(settings, "strength"))
			{
				if(value>MAX_PLAYER_FIELD_LIMIT)
					value=MAX_PLAYER_FIELD_LIMIT;
				else if(value<MIN_PLAYER_FIELD_LIMIT)
					value=MIN_PLAYER_FIELD_LIMIT;
				db[target].set_strength(value);
				notify(player, "Strength set to %d.", value);
			}
			else if(!string_compare(settings, "dexterity"))
			{
				if(value>MAX_PLAYER_FIELD_LIMIT)
					value=MAX_PLAYER_FIELD_LIMIT;
				else if(value<MIN_PLAYER_FIELD_LIMIT)
					value=MIN_PLAYER_FIELD_LIMIT;
				db[target].set_dexterity(value);
				notify(player, "Dexterity set to %d.", value);
			}
			else if(!string_compare(settings, "constitution"))
			{
				if(value>MAX_PLAYER_FIELD_LIMIT)
					value=MAX_PLAYER_FIELD_LIMIT;
				else if(value<MIN_PLAYER_FIELD_LIMIT)
					value=MIN_PLAYER_FIELD_LIMIT;
				db[target].set_constitution(value);
				notify(player, "Constitution set to %d.", value);
			}
/*			else if(!string_compare(settings, "max_hitpoints"))
			{
				db[target].set_max_hit_points(value);
				notify(player, "Maximum hitpoints set to %d.", value);
			}
			else if(!string_compare(settings, "hitpoints"))
			{
				db[target].set_hit_points(value);
				notify(player, "HitPoints set to %d.", value);
			}
			else if(!string_compare(settings, "experience"))
			{
				db[target].set_experience(value);
				notify(player, "Experience set to %d.", value);
			}
I really don't think these should be able to be @modified... */
			else notify(player, "I'm afraid you can't modify that.");
			break;
		case TYPE_WEAPON:
			if(!string_compare(settings, "degradation"))
			{
				if(value>100)
					db[target].set_degradation(100);
				else if(value<0)
					db[target].set_degradation(0);
				else	
					db[target].set_degradation(value);
				notify(player, "Degredation set to %d.", db[target].get_degradation());
			}
			else if(!string_compare(settings, "damage"))
			{
				if(value>MAX_WEAPON_DAMAGE)
					value=MAX_WEAPON_DAMAGE;
				else if(value<0)
					value=0;
				db[target].set_damage(value);
				notify(player, "Damage set to %d.", value);
			}
			else if(!string_compare(settings, "probability"))
			{
				if(value>MAX_WEAPON_SPEED)
					value=MAX_WEAPON_SPEED;
				else if(value<0)
					value=0;
				db[target].set_speed(value);
				notify(player, "Hit Probability set to %d.", value);
			}
			else if(!string_compare(settings, "ammunition"))
			{
				if(value>MAX_AMMO_COUNT)
					value=MAX_AMMO_COUNT;
				else if(value<0)
					value=0;
				db[target].set_ammunition(value);
				notify(player, "Ammunition set to %d.", value);
			}
			else if(!string_compare(settings, "range"))
			{
				if(value>MAX_WEAPON_RANGE)
					value=MAX_WEAPON_RANGE;
				else if(value<0)
					value=0;
				db[target].set_range(value);
				notify(player, "Range set to %d.", value);
			}
			else
				notify(player, "On weapons you may set: degradation, damage, hit probability, ammunition and range.");
			break;
		case TYPE_ARMOUR:
			if(!string_compare(settings, "degradation"))
			{
				if(value>100)
					db[target].set_degradation(100);
				else if(value<0)
					db[target].set_degradation(0);
				else	
					db[target].set_degradation(value);
				notify(player, "Degredation set to %d.", db[target].get_degradation());
			}
			else if(!string_compare(settings, "protection"))
			{
				if(value>MAX_ARMOUR_PROTECTION)
					value=MAX_ARMOUR_PROTECTION;
				else if(value<0)
					value=0;
				db[target].set_protection(value);
				notify(player, "Protection set to %d.", value);
			}
			else
				notify(player, "On armour you may set: degradation and protection.");
			break;
		case TYPE_AMMUNITION:
			if(!string_compare(settings, "quantity"))
			{
				if(value>MAX_AMMO_COUNT)
					value=MAX_AMMO_COUNT;
				else if(value<0)
					value=0;
				db[target].set_count(value);
				notify(player, "Quantity set to %d.", value);
			}
			else
				notify(player, "On ammunition you can only set the quantity.");
			break;
		default:
			notify(player, "Sorry you cannot modify that type.");
			break;
	}
}
