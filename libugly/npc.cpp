/*
 * Code for Non-Player Characters (essentially FSMs) - Ian Chard, Dec 95
 */

#include <stdarg.h>

#include "db.h"
#include "objects.h"
#include "context.h"
#include "externs.h"
#include "interface.h"
#include "command.h"

const char	*initial_state	= "start";	/* name of initial state dictionary */



/*
 * Send an event to an NPC, to trigger a state change.
 */

void context::do_at_event(const String& name, const String& event)
{
	dbref npc;


	set_return_string (error_return_string);
	return_status=COMMAND_FAIL;

	if(!name || !event)
	{
		notify(player, "Usage:  @event <NPC name> = <event>");
		return;
	}

	/* Find the NPC, and make sure we control it. */

	Matcher matcher(player, name, TYPE_PUPPET, get_effective_id());
	matcher.match_everything();
	if((npc=matcher.noisy_match_result())==NOTHING)
	{
		notify(player, "Can't find an NPC called \"%s\".", name);
		return;
	}

	if(Typeof(npc)!=TYPE_PUPPET)
	{
		notify(player, "@event can only be used on NPCs.");
		return;
	}

	if(!controls_for_write(npc))
	{
		notify(player, permission_denied.c_str());
		return;
	}

	db[npc].event(player, npc, event);
	Accessed (npc);

	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
}


/*
 * Find a player's state on an NPC, if they have one.
 */

struct puppet::fsm_state *puppet::find_state(dbref player)
{
	for(struct fsm_state *current=fsm_states; current; current=current->next)
		if(current->player==player)
			return current;
	
	return NULL;
}


/*
 * Make a new state on an NPC for a player, or return an existing one.
 */

struct puppet::fsm_state *puppet::make_state(dbref player)
{
	struct fsm_state *new_state;

	new_state=(struct fsm_state *) malloc(sizeof(struct fsm_state));
	new_state->next=fsm_states;
	new_state->prev=NULL;
	new_state->player=player;
	if(fsm_states)
		fsm_states->prev=new_state;
	fsm_states=new_state;

	return new_state;
}


/*
 * Given a dictionary, an element number, an fsm_state and an
 * NPC command string, execute the command (if any) and move
 * the state on.  Returns an error message, or NULL on success.
 */

static String process_npc_command(dbref npc, const char *cmd, struct puppet::fsm_state *state)
{
	char		*semicolon;
	char		*fail_state = NULL;
	char		*end_state;
	Command_status	status;
	context		*npc_context;
	dbref		newdict;

	/* Execute the command. */

	strcpy(scratch_buffer, cmd);
	if((semicolon=strchr(scratch_buffer, ';')))
		*semicolon='\0';

	npc_context=new context (npc);
	
	npc_context->calling_from_command();
	npc_context->process_basic_command(semicolon+1);
	mud_scheduler.push_new_express_job (npc_context);
	status=npc_context->get_return_status();

	delete npc_context;

	/* Is there any state information? */

	if(!semicolon || !*(semicolon+1))
	{
		String ret;
		ret.printf("[%s NPC warning:  infinite loop]", db[npc].get_name());
		return ret;
	}

	/* Find out if there are one or two possible states we can go to. */

	strcpy(scratch_buffer, semicolon+1);
	if((semicolon=strchr(scratch_buffer, ';')))
	{
		*semicolon='\0';
		if(*(semicolon+1))
			fail_state=semicolon+1;	/* scratch_buffer is the success state */
	}

	/* Decide which state we want to use... */

	if(fail_state && status==COMMAND_FAIL)
		end_state=fail_state;
	else
		end_state=scratch_buffer;
	
	/* ...and go there. */

	Matcher matcher(npc, end_state, TYPE_DICTIONARY, db[npc].get_owner());
	matcher.work_silent();
	matcher.match_array_or_dictionary();
	matcher.match_absolute();

	if((newdict=matcher.match_result())==NOTHING)
		state->state=newdict;
	else
	{
		String ret;
		ret.printf("[%s NPC error:  \"%s\" isn't a state]", db[npc].get_name(), end_state);
		return ret;
	}
	
	return NULLSTRING;
}


/*
 * Delete a player context from an NPC.
 */

void puppet::delete_state(struct fsm_state *old_state)
{
	if(old_state->prev)
		old_state->prev->next=old_state->next;
	if(old_state->next)
		old_state->next->prev=old_state->prev;
	if(fsm_states==old_state)
		fsm_states=fsm_states->next;

	free(old_state);
}


/*
 * Report an error to the NPC's owner, if SILENT isn't set on it.
 */

void puppet::fsm_error(const String& fmt, ...)
{
	va_list va;

	if(!get_flag(FLAG_SILENT))
	{
		va_start(va, fmt);
		String tmp;
		tmp.vprintf(fmt.c_str(), va);
		va_end(va);

		notify(get_owner(), "%s", tmp.c_str());
	}
}


/*
 * Receive an event, and possibly do something and change states.
 */

void puppet::event(dbref player, dbref npc, const String& event)
{
	struct fsm_state *state;
	int elem;
	dbref end_state; /* IAN I HAVE ADDED THIS TO MAKE IT COMPILE */

	if(!(state=find_state(player)))
	{
		/* If we don't have an initial state, we're scuppered. */

		Matcher matcher(npc, end_state, TYPE_DICTIONARY, db[npc].get_owner());
		matcher.work_silent();
		matcher.match_array_or_dictionary();
		matcher.match_absolute();
		state=make_state(player);
	} /* IAN, I ADDED THIS BRACKET TO MAKE IT COMPILE. CHARGE:
			5.4536p */

	/* Make sure the current state is still valid */

	if(Typeof(state->state)!=TYPE_DICTIONARY)
	{
		fsm_error("[%s NPC event:  state dictionary #%d no longer exists]", get_name(), state->state);
		return;
	}

	if((elem=db[state->state].exist_element(event)))
	{
		String msg=process_npc_command(npc, db[state->state].get_element(elem), state);

		if(msg)
			fsm_error(msg);
	}
	else
		fsm_error("[%s NPC event:  received \"%s\", no such event in #%d]", get_name(), event, state->state);
}

