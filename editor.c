/* This houses the code for the hard-coded object editor.
 *
 * Reason: Current 'soft-code' editors are to slow, and hacky.
 *
 * Author: Chisel
 *
 * Commenced: 7/10/96
 * Completed: Currently Under Development
 *
 */

#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "editor.h"

void
context::do_editor (
const	char	*name,
const	char	*options)
{
	description_class	object_desc;
	dbref			object;

	return_status = COMMAND_FAIL;
	return_string = error_return_string;

	object_desc.add_line();
	object_desc.add_line();
	object_desc.delete_line(object_desc.num_lines_in_desc());

	object_desc.replace_line("This is a test string", 1);
	object_desc.view_all_lines();

	/* If nothing specified to decompile, give up */
	if (name == NULL || *name == '\0')
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You must specify something to edit.");
		return;
	}

	Matcher controller_matcher (player, name, TYPE_PLAYER, get_effective_id());

	/* Set up the object matcher*/
	Matcher thing_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	thing_matcher.match_everything ();

	/* get result */
	if((object = thing_matcher.noisy_match_result()) == NOTHING)
		return;

	if (db[object].get_flag(FLAG_READONLY))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't edit a ReadOnly object!");
		return;
	}

	if (!controls_for_write(object))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}
	 
	if (in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You cannot start the editor inside a command.");
		return;
	}

	if (Editing(player))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You are already set editing.");
	}
	else
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You are now set editing.");
		db[player].set_flag(FLAG_EDIT);
	}
		
	switch (Typeof(object))
	{
	  case TYPE_COMMAND:
		notify_colour(player, player, COLOUR_MESSAGES, "Object: %s%s%s (%s#%d%s)", EDIT_OBJECT_NAME, db[object].get_name(), REVERT, EDIT_OBJECT_ID, object, REVERT);
		break;
	  default:
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@edit cannot be used to edit objects of that type.");
	}
}


description_class::description_class (void)
{
  no_lines = 0;
  strcpy(desc_lines[1], "\0");
}

bool
description_class::add_line (void)
{
  const char *LINE = "HELLO WORLD!";

  fprintf(stderr, "Current number of lines: %02d\n", num_lines_in_desc());

  if (num_lines_in_desc() > 0)
    realloc (desc_lines, sizeof(desc_lines) / sizeof(char *));

  no_lines++;
  desc_lines[num_lines_in_desc() - 1] = (char *) malloc (sizeof(LINE));
  strcpy(desc_lines[num_lines_in_desc() - 1], LINE);
	
  fprintf(stderr, "New number of lines: %02d\n", num_lines_in_desc());
  
  fprintf(stderr, "edit: add line [%s]\n", view_line(num_lines_in_desc()));
	return true;
}

char *
description_class::view_line (int line_to_view)
{
  return desc_lines[line_to_view - 1];
}

void
description_class::view_all_lines(void)
{
  int count;

  for (count = 1; count <= num_lines_in_desc(); count++)
    fprintf(stderr, "[%02d] %s\n", count, view_line(count));
}

bool
description_class::delete_line (int line_to_delete)
{
  fprintf(stderr, "edit: delete line %d\n", line_to_delete);
	return true;
}

bool
description_class::replace_line (const char *text, int line_to_replace)
{
  if (line_to_replace > num_lines_in_desc())
  {
    fprintf(stderr, "There aren't that many lines in the description!\n");
    return false;
  }
  else
  {
    desc_lines[line_to_replace - 1] = (char *) malloc (sizeof (text));
    strcpy(desc_lines[line_to_replace - 1], text);
  }
  
  fprintf(stderr, "edit: replace line with '%s'\n", text);
	return true;
}

int
description_class::num_lines_in_desc (void)
{
	return no_lines;
}
