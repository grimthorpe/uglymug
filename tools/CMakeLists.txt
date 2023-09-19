set(tools
	dump
	extract
	mondodestruct
	mudwho
	redirector
	scat
	stats
	trawl)

foreach(tool ${tools})
	add_executable(${tool} ${tool}.cpp version.cpp)
	target_link_libraries(${tool} ugly crypt ncurses pcre)
endforeach(tool)

