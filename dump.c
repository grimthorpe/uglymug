/* static char SCCSid[] = "@(#)dump.c	1.6\t6/14/95"; */
#include "copyright.h"

#include <stdio.h>

#include "db.h"
#include "externs.h"

#define FLAG(x, y) if (db[i].get_flags() & x) printf(y)


int main(int argc, char **argv)
{
    dbref owner = 1, lister;
    dbref thing;
    dbref i;

	if(argc < 1)
	{
		Trace( "Usage: %s [owner]\n", *argv);
		exit(1);
	}
    
	if(argc >= 2)
		owner = lister = atol(argv[1]);
	else
		lister = NOTHING;

	if(db.read(stdin) < 0)
	{
		Trace( "%s: bad input\n", argv[0]);
		exit(5);
	}

	for(i = 0; i < db.get_top(); i++)
	{
		/* don't show exits separately */
		/*	if((o->flags & TYPE_MASK) == TYPE_EXIT) continue;*/

		/* Don't show free slots */
		if (Typeof(i) == TYPE_FREE)
			continue;

		/* don't show it if it isn't owned by the right owner */
		if(lister != NOTHING && db[i].get_owner() != owner)
			continue;

		printf("#%d: %s [%s] ", i, db[i].get_name().c_str ()
		       , db[db[i].get_owner()].get_name().c_str ());
		printf("at %s ", unparse_object(owner, db[i].get_location()));

		printf(" Building Points: %d Type: ", db[i].get_pennies());

		switch(Typeof(i))
		{
			case TYPE_ROOM:
				printf("Room");
				break;
			case TYPE_EXIT:
				printf("Exit");
				break;
			case TYPE_THING:
				printf("Thing");
				break;
			case TYPE_PLAYER:
				printf("Player");
				break;
			case TYPE_VARIABLE:
				printf("Variable");
				break;
			case TYPE_FUSE:
				printf("Fuse");
				break;
			case TYPE_COMMAND:
				printf("Command");
				break;
			case TYPE_ALARM:
				printf("Alarm");
				break;
			case TYPE_FREE:
				printf("Free");
				break;
			default:
				printf("***UNKNOWN TYPE***");
				break;
		}

		/* handle flags */
		putchar('\n');
		if(db[i].get_flags() & ~TYPE_MASK)
		{
			printf("Flags: ");
			FLAG(FLAG_DARK, "DARK ");
			FLAG(FLAG_STICKY, "STICKY ");
			FLAG(FLAG_LISTEN, "LISTEN ");
			FLAG(FLAG_WIZARD, "WIZARD ");
			if(db[i].get_flags() & (FLAG_GENDER_MALE >> GENDER_SHIFT)) printf("MALE ");
			if(db[i].get_flags() & (FLAG_GENDER_FEMALE>> GENDER_SHIFT)) printf("FEMALE ");
			if(db[i].get_flags() & (FLAG_GENDER_NEUTER>> GENDER_SHIFT)) printf("NEUTER ");

			if(db[i].get_flags() & (FLAG_ARTICLE_SINGULAR_CONS_FLAG)) printf("SINGULARCONSONANT ") ;
			if(db[i].get_flags() & (FLAG_ARTICLE_SINGULAR_VOWEL_FLAG)) printf("SINGULARVOWEL ") ;
			if(db[i].get_flags() & (FLAG_ARTICLE_PLURAL_FLAG)) printf("PLURAL ");
		#ifdef RESTRICTED_BUILDING
			FLAG(FLAG_BUILDER, "BUILDER ");
		#endif /* RESTRICTED_BUILDING */
			FLAG(VISIBLE ,"VISIBLE ");
			FLAG(OPENABLE, "OPENABLE ");
			FLAG(OPEN, "OPEN ");
			FLAG(OPAQUE, "OPAQUE ");
			FLAG(LOCKED, "LOCKED ");
			FLAG(HAVEN, "HAVEN ");
			FLAG(CHOWN_OK, "CHOWN_OK ");
			FLAG(APPRENTICE, "APPRENTICE ");
			FLAG(FIGHTING, "FIGHTING ");
			FLAG(NUMBER, "NUMBER ");
		}
		putchar('\n');

		if(db[i].get_destination() != NOTHING)
		{
			switch(Typeof(i))
			{
			  case TYPE_ROOM:
				 printf("Dropto: ");
				 break;
			  case TYPE_EXIT:
				 printf("Destination: ");
				 break;
			  case TYPE_THING:
			  case TYPE_PLAYER:
				 printf("Home: ");
				 break;
			  case TYPE_ALARM:
				 printf("Triggered Command: ");
				 break;
			  case TYPE_VARIABLE:
			  case TYPE_FUSE:
			  case TYPE_COMMAND:
			  case TYPE_FREE:
				 break;
			}
			printf("%s\n", unparse_object(owner, db[i].get_destination()));
		}
		if(db[i].get_key() != TRUE_BOOLEXP) printf("Key: %s\n",
						  unparse_boolexp(owner, db[i].get_key()));
		if(db[i].get_description()) {
			 puts("Description: ");
			 puts(db[i].get_description());
		}

		if(db[i].get_succ_message()) {
			 printf("Success Message: %s\n", db[i].get_succ_message());
		}
		if(db[i].get_fail_message()) {
			 printf("Fail Message: %s\n", db[i].get_fail_message());
		}
		if(db[i].get_drop_message()) {
			 printf("Drop Message: %s\n", db[i].get_drop_message());
		}
		if(db[i].get_osuccess()) {
			 printf("Other Success Message: %s\n", db[i].get_osuccess());
		}
		if(db[i].get_ofail()) {
			 printf("Other Fail Message: %s\n", db[i].get_ofail());
		}
		if(db[i].get_odrop()) {
			 printf("Other Drop Message: %s\n", db[i].get_odrop());
		}
		if(Typeof(i) == TYPE_PLAYER)
		{
			printf(" PLAYER ABILITIES\n");
			printf("  Race: %s\n", db[i].get_race());
			printf("  Score: %d\n", db[i].get_score());
			printf("  Str: %d\n", db[i].get_strength());
			printf("  Con: %d\n", db[i].get_constitution());
			printf("  Dex: %d\n", db[i].get_dexterity());
			printf("  Per: %d\n", db[i].get_perception());
			printf("  Int: %d\n", db[i].get_intelligence());
			printf("  Hit: %d\n", db[i].get_hitpoints());
			printf("  Arm: %d\n", db[i].get_armourclass());
			printf("  Lev: %d\n", db[i].get_level());
			printf("  Exp: %d\n", db[i].get_experience());
		}
		if(db[i].get_contents() != NOTHING)
		{
			if (Typeof(i) == TYPE_THING)
			{
				printf ("Lock Key: %s\n", unparse_boolexp (owner,
						db[i].get_lock_key()));
				if (db[i].get_contents_string())
				{
					printf("Contents string: %s\n",
						db[i].get_contents_string());
				}
			}
			else
				printf("Contents:\n");
			DOLIST(thing, db[i].get_contents())
				printf ("  %s\n", unparse_object(owner, thing));
		}

		if (db[i].get_commands() != NOTHING)
		{
			printf("Commands:\n");
			DOLIST(thing, db[i].get_commands())
				printf ("  %s\n", unparse_object(owner, thing));
		}

		if (db[i].get_variables() != NOTHING)
		{
			printf("Variables:\n");
			DOLIST(thing, db[i].get_variables())
				printf ("  %s\n", unparse_object(owner, thing));
		}

		if (db[i].get_fuses() != NOTHING)
		{
			printf("Fuses :\n");
			DOLIST(thing, db[i].get_fuses())
				printf ("  %s\n", unparse_object(owner, thing));
		}

		if(db[i].get_exits() != NOTHING)
		{
			if(Typeof(i) == TYPE_ROOM)
			{
				puts("Exits:");
				DOLIST(thing, db[i].get_exits())
				{
					printf("  %s%s", getarticle (thing, ARTICLE_UPPER_INDEFINITE), getname (thing));
					if(db[thing].get_key() != TRUE_BOOLEXP)
					{
						printf(" KEY: %s",
						       unparse_boolexp(owner, db[thing].get_key()));
					}
					if(db[thing].get_destination() != NOTHING)
					{
						printf(" => %s%s(%d)\n",
							getarticle (db[thing].get_destination(), ARTICLE_LOWER_INDEFINITE),
							getname (db[thing].get_destination ()),
							db[thing].get_destination());
					}
					else
					{
						puts(" ***OPEN***");
					}
				}
			}
		}
		putchar('\n');
	}
	exit(0);
}
