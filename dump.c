/**\file
 * Print to stdout the objects owned by an owner whose ID is passed as the first parameter to the program.
 */

#include "copyright.h"
#include <iostream>
#include "db.h"
#include "externs.h"

using std::cout;

#define FLAG(x, y) if (db[i].get_flag(x)) cout << y
#define FLAG_PREDICATE(x, y) if (x(i)) cout << y


/**
 * Return a string representing the name of a given object type.
 */

const char *
type_name (const int i)

{
	switch(Typeof(i))
	{
		case TYPE_ROOM:
			return ("Room");
		case TYPE_EXIT:
			return ("Exit");
		case TYPE_THING:
			return ("Thing");
		case TYPE_PLAYER:
			return ("Player");
		case TYPE_VARIABLE:
			return ("Variable");
		case TYPE_FUSE:
			return ("Fuse");
		case TYPE_COMMAND:
			return ("Command");
		case TYPE_ALARM:
			return ("Alarm");
		case TYPE_FREE:
			return ("Free");
		default:
			return ("***UNKNOWN TYPE***");
	}
}

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

	context ctx (owner, context::DEFAULT_CONTEXT);

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

		cout << '#' << i
			<< ": " << db[i].get_name()
			<< " [" << db[db[i].get_owner()].get_name() << ']'
			<< std::endl;
		cout << "at " << unparse_object(ctx, db[i].get_location())
			<< std::endl;

		cout << " Building Points: " << db[i].get_pennies()
			<< " Type: " << type_name (i) << std::endl;


		/* handle flags */
		cout << std::endl << "Flags: ";
		FLAG_PREDICATE(Dark, "DARK ");
		FLAG_PREDICATE(Sticky, "STICKY ");
		FLAG_PREDICATE(Listen, "LISTEN ");
		FLAG_PREDICATE(Wizard, "WIZARD ");
		FLAG_PREDICATE(Male, "MALE ");
		FLAG_PREDICATE(Female, "FEMALE ");
		FLAG_PREDICATE(Neuter, "NEUTER ");

		FLAG (FLAG_ARTICLE_SINGULAR_CONS, "SINGULARCONSONANT ") ;
		FLAG (FLAG_ARTICLE_SINGULAR_VOWEL, "SINGULARVOWEL ") ;
		FLAG (FLAG_ARTICLE_PLURAL, "PLURAL ");
#ifdef RESTRICTED_BUILDING
		FLAG_PREDICATE(Builder, "BUILDER ");
#endif /* RESTRICTED_BUILDING */
		FLAG_PREDICATE(Visible,"VISIBLE ");
		FLAG_PREDICATE(Openable, "OPENABLE ");
		FLAG_PREDICATE(Open, "OPEN ");
		FLAG_PREDICATE(Opaque, "OPAQUE ");
		FLAG_PREDICATE(Locked, "LOCKED ");
		FLAG_PREDICATE(Haven, "HAVEN ");
		FLAG_PREDICATE(Chown_ok, "CHOWN_OK ");
		FLAG_PREDICATE(Apprentice, "APPRENTICE ");
		FLAG_PREDICATE(Fighting, "FIGHTING ");
		FLAG_PREDICATE(Number, "NUMBER ");
		cout << std::endl;

		if(db[i].get_destination() != NOTHING)
		{
			switch(Typeof(i))
			{
			  case TYPE_ROOM:
				 cout << "Dropto: ";
				 break;
			  case TYPE_EXIT:
				 cout << "Destination: ";
				 break;
			  case TYPE_THING:
			  case TYPE_PLAYER:
				 cout << "Home: ";
				 break;
			  case TYPE_ALARM:
				 cout << "Triggered Command: ";
				 break;
			  case TYPE_VARIABLE:
			  case TYPE_FUSE:
			  case TYPE_COMMAND:
			  case TYPE_FREE:
				 break;
			}
			cout << unparse_object(ctx, db[i].get_destination()) << std::endl;
		}
		if(db[i].get_key() != TRUE_BOOLEXP)
			cout << "Key: " << db[i].get_key()->unparse (ctx) << std::endl;
		if(db[i].get_description())
			 cout << "Description: " << std::endl << db[i].get_description() << std::endl;
		if(db[i].get_succ_message())
			 cout << "Success Message: " << db[i].get_succ_message() << std::endl;
		if(db[i].get_fail_message())
			 cout << "Fail Message: " << db[i].get_fail_message() << std::endl;
		if(db[i].get_drop_message())
			 cout << "Drop Message: " << db[i].get_drop_message() << std::endl;
		if(db[i].get_osuccess())
			 cout << "Other Success Message: " << db[i].get_osuccess() << std::endl;
		if(db[i].get_ofail())
			 cout << "Other Fail Message: " << db[i].get_ofail() << std::endl;
		if(db[i].get_odrop())
			 cout << "Other Drop Message: " << db[i].get_odrop() << std::endl;
		if(Typeof(i) == TYPE_PLAYER)
		{
			cout << " PLAYER ABILITIES" << std::endl;
			cout << "  Race: " << db[i].get_race() << std::endl;
			cout << "  Score: " << db[i].get_score() << std::endl;
		}
		if(db[i].get_contents() != NOTHING)
		{
			if (Typeof(i) == TYPE_THING)
			{
				cout << "Lock Key: "
					<< db[i].get_lock_key()->unparse (ctx)
					<< std::endl;
				if (db[i].get_contents_string())
					cout << "Contents string: " << db[i].get_contents_string() << std::endl;
			}
			else
				cout << "Contents:" << std::endl;
			DOLIST(thing, db[i].get_contents())
				cout << "  " << unparse_object(ctx, thing) << std::endl;
		}

		if (db[i].get_commands() != NOTHING)
		{
			cout << "Commands:" << std::endl;
			DOLIST(thing, db[i].get_commands())
				cout << "  " << unparse_object(ctx, thing) << std::endl;
		}

		if (db[i].get_variables() != NOTHING)
		{
			cout << "Variables:" << std::endl;
			DOLIST(thing, db[i].get_variables())
				cout << "  " << unparse_object(ctx, thing) << std::endl;
		}

		if (db[i].get_fuses() != NOTHING)
		{
			cout << "Fuses:" << std::endl;
			DOLIST(thing, db[i].get_fuses())
				cout << "  " << unparse_object(ctx, thing) << std::endl;
		}

		if(db[i].get_exits() != NOTHING)
		{
			if(Typeof(i) == TYPE_ROOM)
			{
				cout << "Exits:" << std::endl;
				DOLIST(thing, db[i].get_exits())
				{
					cout << "  "
						<< getarticle (thing, ARTICLE_UPPER_INDEFINITE)
						<< getname (thing)
						<< std::endl;
					if(db[thing].get_key() != TRUE_BOOLEXP)
						cout << " KEY: " << db[thing].get_key()->unparse (ctx) << std::endl;

					if(db[thing].get_destination() != NOTHING)
						cout << " => "
							<< getarticle (db[thing].get_destination(), ARTICLE_LOWER_INDEFINITE)
							<< getname (db[thing].get_destination ())
							<< '(' << db[thing].get_destination() << ')'
							<< std::endl;
					else
						cout << " ***OPEN***" << std::endl;
				}
			}
		}
		cout << std::endl;
	}
	exit(0);
}
