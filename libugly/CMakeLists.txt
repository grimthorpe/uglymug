
add_custom_target(Version
	${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/Version.cmake
)

set(source_code
	alarm.cpp
	boolexp.cpp
	channel.cpp
	colour.cpp
	command.cpp
	container.cpp
	context.cpp
	create.cpp
	db.cpp
	debug.cpp
	decompile.cpp
	destroy.cpp
	#editor.cpp
	#eval.cpp
	expression.cpp
	fuses.cpp
	game.cpp
	game_predicates.cpp
	get.cpp
	group.cpp
	interface.cpp
	lists.cpp
	log.cpp
	look.cpp
	luainterp.cpp
	match.cpp
	move.cpp
	mudstring.cpp
	#npc.cpp
	objects.cpp
	player.cpp
	predicates.cpp
	rob.cpp
	sanity-check.cpp
	set.cpp
	smd.cpp
	speech.cpp
	stringutil.cpp
	unparse.cpp
	utils.cpp
	variable.cpp
	wiz.cpp
	alarm.h
	colour.h
	command.h
	concentrator.h
	config.h
	context.h
	copyright.h
	db_fields.h
	db.h
	debug.h
	descriptor.h
	editor.h
	expression.h
	externs.h
	game.h
	game_predicates.h
	interface.h
	lists.h
	local_curses.h
	log.h
	lunar.h
	match.h
	mudstring.h
	objects.h
	os.h
	regexp_interface.h
	stack.h
	telnet.h
)

add_library(ugly STATIC ${source_code})

add_dependencies(ugly Version)

target_include_directories (ugly PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
