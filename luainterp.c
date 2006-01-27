#include "mudstring.h"
#include "context.h"
#include "interface.h"
#include "externs.h"

#if !defined(LUA)
/*
 * If we don't have LUA support, then the whole of this file can't be used.
 */
Command_action
context::do_lua_command(dbref command, const String& cmdname, const String& arg1, const String& arg2)
{
	notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Lua support not available");
	return ACTION_HALT;
}

#else

/*
 * Ok, now we know we've got LUA support. Lets do things properly...
 */

#include "lunar.h"

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

class UM_lua_commands
{
public:
	// The commands
	static int Command(lua_State*);

	// The hooks
	static void _CountHook(lua_State*, lua_Debug*);

	// The context functions
	static int current_command(lua_State*);
	static int player(lua_State*);
	static int here(lua_State*);
	static int arg1(lua_State*);
	static int arg2(lua_State*);
	static int arg3(lua_State*);
private:
	// Helper functions
	static context* get_context(lua_State *L)
	{
		return (context*) lua_touserdata(L, lua_upvalueindex(1));
	}
};

static const luaL_reg luaUM_Library[] =
{
	{ "exec",		UM_lua_commands::Command},
	{ "this",		UM_lua_commands::current_command},
	{ "me",			UM_lua_commands::player},
	{ "here",		UM_lua_commands::here},
	{ "1",			UM_lua_commands::arg1},
	{ "2",			UM_lua_commands::arg2},
	{ "3",			UM_lua_commands::arg3},

	{NULL, NULL}
};

class UMObject
{
	dbref		m_object;
	context*	m_context;
	UMObject(context* c, dbref obj) :m_object(obj), m_context(c) {}
public:
	static const char className[];
	static Lunar<UMObject>::RegType methods[];

	static int	push_UMObject(lua_State* S, context* c, dbref obj);

	UMObject(lua_State* L)
	{
		lua_pushstring(L, "PermissionDenied");
		lua_error(L);
	}

	int name(lua_State* L)
	{
		lua_pushstring(L, db[m_object].get_name().c_str());

		return 1;
	}
	int description(lua_State* L)
	{
		lua_pushstring(L, db[m_object].get_description().c_str());

		return 1;
	}
	int owner(lua_State* L)
	{
		return push_UMObject(L, m_context, db[m_object].get_owner());
	}
};

const char UMObject::className[] = "UMObject";

#define method(class, name) {#name, &class::name}
Lunar<UMObject>::RegType UMObject::methods[] =
{
	method(UMObject, name),
	method(UMObject, description),
	method(UMObject, owner),

	{NULL, NULL}
};


Command_action
context::do_lua_command(dbref command, const String& cmdname, const String& arg1, const String& arg2)
{
	if((command <= NOTHING) || (Typeof(command) != TYPE_COMMAND))
	{
		return ACTION_HALT;
	}

	lua_State* L = lua_open();
	// Register the UM command name
	// Push the context onto the stack. Every time UM_lua_command is called we can retrieve this value
	lua_pushlightuserdata(L, (void*)this);
	luaL_openlib(L, "UM", luaUM_Library, 1);

	// Include the base library (to be removed soon)
	lua_baselibopen(L);

	Lunar<UMObject>::Register(L);

	String cmd = db[command].get_description();
	int err;
	err = luaL_loadbuffer(L, cmd.c_str(), cmd.length(), cmdname.c_str());
	if(!err)
	{
		lua_sethook(L, UM_lua_commands::_CountHook, LUA_MASKCOUNT, step_limit * 10);

		lua_pushstring(L, arg1.c_str());
		lua_pushstring(L, arg2.c_str());
		err = lua_pcall(L, 2, 2, 0);
	}

	if(err)
	{
		printf("lua: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
	else
	{
		// Need to set return values here
	}

	lua_close(L);

	if(err)
	{
		return ACTION_HALT;
	}
	return ACTION_STOP;
}


/*
 * Run a UM command line.
 * Does the normal variable and command expansion thing.
 * Result is returned as (Success, Return string) to calling code
 */
int
UM_lua_commands::Command(lua_State *L)
{
	context* c = get_context(L);

	// Increase the step count.
	c->command_executed();
	if(!c->allow_another_step())
	{
		lua_pushstring(L, "Recursionlimit");
		return lua_error(L);
	}

	int n = lua_gettop(L);
	if(n != 1)
	{
		lua_pushstring(L, "Invalid number of arguments");
		lua_error(L);
	}
	String cmd = lua_tostring(L, 1);

	Command_status returnstatus;

	String result = c->sneakily_process_basic_command(cmd, returnstatus);

	switch(returnstatus)
	{
	case COMMAND_HALT:
		lua_pushstring(L, "Recursionlimit");
		return lua_error(L);
		break;

	case COMMAND_SUCC:
		lua_pushnumber(L, 1);
		break;
	default:
		lua_pushnumber(L, 0);
		break;
	}
	lua_pushstring(L, result.c_str());

	return 2;
}

int
UM_lua_commands::current_command(lua_State* L)
{
	context* c = get_context(L);

	return UMObject::push_UMObject(L, c, c->get_current_command());
}

int
UM_lua_commands::player(lua_State* L)
{
	context* c = get_context(L);

	return UMObject::push_UMObject(L, c, c->get_player());
}

int
UM_lua_commands::here(lua_State* L)
{
	context* c = get_context(L);

	return UMObject::push_UMObject(L, c, db[c->get_player()].get_location());
}

int
UM_lua_commands::arg1(lua_State* L)
{
	context* c = get_context(L);

	lua_pushstring(L, c->get_arg1().c_str());
	return 1;
}
int
UM_lua_commands::arg2(lua_State* L)
{
	context* c = get_context(L);

	lua_pushstring(L, c->get_arg2().c_str());
	return 1;
}
int
UM_lua_commands::arg3(lua_State* L)
{
	context* c = get_context(L);

	lua_pushstring(L, reconstruct_message(c->get_arg1(), c->get_arg2()));
	return 1;
}

void
UM_lua_commands::_CountHook(lua_State* L, lua_Debug* ar)
{
	// We get called after step_limit * 10 VM instructions, which is reasonable for now.
	lua_pushstring(L, "Recursionlimit");
	lua_error(L);
}


int
UMObject::push_UMObject(lua_State* L, context* c, dbref obj)
{
	lua_pushvalue(L, Lunar<UMObject>::push(L, new UMObject(c, obj), true));
	return 1;
}
#endif

// Common code.
Lua_command_and_arguments::Lua_command_and_arguments
	(dbref c, context *co, const String& sc, const String& a1, const String& a2, dbref eid, Matcher *m, bool silent)
	: Compound_command_and_arguments(c, co, sc, a1, a2, eid, m, silent)
{
}

Lua_command_and_arguments::~Lua_command_and_arguments()
{
}

Command_action
Lua_command_and_arguments::step(context *co)
{
	return co->do_lua_command(get_command(), get_simple_command(), get_arg1(), get_arg2());
}

Command_action
Lua_command_and_arguments::step_once(context *co)
{
	return step(co);
}
