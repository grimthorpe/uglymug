MAKEFLAGS=-k -B

by_default_just_make: netmud

# Whatever you put in for $(CC) must be able to grok ANSI C.
CC=gcc
CPLUSPLUS=g++
OPTIM=
 
####################################
#           Solaris 2.8
####################################
WHOAMI=who am i
LIBS= -lcurses -lm -ltermcap -lsocket -lnsl
# Normal
CFLAGS= -g -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion 
# Extra debug info
#CFLAGS= -ggdb -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion 

####################################
#              Linux
####################################
#WHOAMI=whoami
#LIBS= -lm -ldl -lcurses -lcrypt
# Normal
#CFLAGS= -g -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion
# Extra debug info
#CFLAGS= -ggdb -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion 

###############
# Old comment
# Linux + USE_TERMINFO in config.h

####################################
#            FreeBSD
####################################
#WHOAMI=whoami
#LIBS= -lm -lncurses -lcrypt 
# Normal
#CFLAGS= -g -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion 
# Extra debug info
#CFLAGS= -ggdb -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion 



HEADERS = \
	alarm.h \
	colour.h \
	command.h \
	concentrator.h \
	config.h \
	context.h \
	copyright.h \
	db.h \
	externs.h \
	game.h \
	game_predicates.h \
	interface.h \
	match.h \
	memory.h \
	mudstring.h \
	objects.h \
	regexp_interface.h \
	debug.h

# C_OBJECTS are compiled with gcc, not g++
C_OBJECTS = \
	regexp.o \
	scat.o \
	concentrator.o 

# OPTIM_OBJECTS are the object files compiled with $(OPTIM)
OPTIM_OBJECTS = \
	db.o \
	game.o \
	match.o \
	predicates.o \
	sanity-check.o \
	stringutil.o \
	variable.o

SOURCES	= \
	alarm.c \
	boolexp.c \
	channel.c \
	colouring.c \
	combat.c \
	command.c \
	colour.c \
	concentrator.c \
	container.c \
	context.c \
	create.c \
	db.c \
	decompile.c \
	destroy.c \
	eval.c \
	fuses.c \
	game.c \
	game_predicates.c \
	group.c \
	interface.c \
	lists.c \
	log.c \
	look.c \
	match.c \
	memory.c \
	move.c \
	netmud.c \
	objects.c \
	player.c \
	predicates.c \
	regexp.c \
	rob.c \
	sanity-check.c \
	scat.c\
	set.c \
	smd.c \
	speech.c \
	stringutil.c \
	unparse.c \
	utils.c \
	variable.c \
	wiz.c \
	debug.c

UTIL_OBJECTS = \
	db.o \
	objects.o \
	predicates.o \
	unparse.o \
	utils.o \
	debug.o

OBJECTS	= \
	$(UTIL_OBJECTS) \
	alarm.o \
	boolexp.o \
	channel.o \
	colour.o \
	combat.o \
	command.o \
	container.o \
	context.o \
	create.o \
	decompile.o \
	destroy.o \
	fuses.o \
	game.o \
	game_predicates.o \
	get.o \
	group.o \
	interface.o \
	lists.o \
	log.o \
	look.o \
	match.o \
	move.o \
	player.o \
	regexp.o \
	rob.o \
	sanity-check.o \
	set.o \
	smd.o \
	speech.o \
	stringutil.o \
	variable.o \
	wiz.o 


OUTFILES = \
	dump \
	extract \
	mondodestruct \
	netmud \
	paths \
	sanity-check \
	sadness \
	stats \
	concentrator

all: dump extract paths sanity-check netmud sadness stats colouring concentratormondodestruct

purify: netmud.purify

netmud: $(OBJECTS) netmud.o
	$(CPLUSPLUS) $(CFLAGS) -o netmud netmud.o $(OBJECTS) $(LIBS)

netmud.purify: netmud.o $(OBJECTS)
	purify -g++=yes -collector=/usr/local/lib/gcc-lib/sparc-sun-sunos4.1.4/2.7.2/ld $(CPLUSPLUS) $(CFLAGS) -o netmud.purify netmud.o $(OBJECTS) $(LIBS)

dump: dump.o $(UTIL_OBJECTS)
	-rm -f dump
	$(CPLUSPLUS) $(CFLAGS) -o dump dump.o $(UTIL_OBJECTS) $(LIBS)

sadness: sadness.o $(UTIL_OBJECTS)
	-rm -f sadness
	$(CPLUSPLUS) $(CFLAGS) -o sadness sadness.o $(OBJECTS) $(LIBS)

mondodestruct: mondodestruct.o $(UTIL_OBJECTS)
	-rm -f mondodestruct
	$(CPLUSPLUS) $(CFLAGS) -o mondodestruct mondodestruct.o $(OBJECTS) $(LIBS)

trawl: trawl.o $(UTIL_OBJECTS)
	-rm -f trawl
	$(CPLUSPLUS) $(CFLAGS) -o trawl trawl.o $(OBJECTS) $(LIBS)

ashcheck: ashcheck.o $(UTIL_OBJECTS) $(OBJECTS)
	-rm -f ashcheck
	$(CPLUSPLUS) $(CFLAGS) -o ashcheck ashcheck.o $(OBJECTS) $(LIBS)

stats: stats.o $(UTIL_OBJECTS)
	-rm -f stats
	$(CPLUSPLUS) $(CFLAGS) -o stats stats.o $(UTIL_OBJECTS) $(LIBS)

sanity-check: sanity-check.o $(UTIL_OBJECTS)
	-rm -f sanity-check
	$(CPLUSPLUS) $(CFLAGS) -o sanity-check sanity-check.o $(UTIL_OBJECTS) $(LIBS)

extract: extract.o $(UTIL_OBJECTS)
	-rm -f extract
	$(CPLUSPLUS) $(CFLAGS) -o extract extract.o $(UTIL_OBJECTS) $(LIBS)

colouring: colouring.o $(UTIL_OBJECTS)
	-rm -f colouring
	$(CPLUSPLUS) $(CFLAGS) -o colouring colouring.o $(OBJECTS) $(LIBS)

paths: paths.o $(UTIL_OBJECTS)
	-rm -f paths
	$(CPLUSPLUS) $(CFLAGS) -o paths paths.o $(UTIL_OBJECTS) $(LIBS)

scat: scat.o
	-rm -f scat
	$(CC) -o scat scat.o $(LIBS)

concentrator: concentrator.o 
	-rm -f concentrator
	$(CC) $(CFLAGS) -o concentrator concentrator.o 

tar: $(SOURCES) $(HEADERS) Makefile start.db
	tar -cf mudcode.tar *.c *.h Makefile start.db
#	tar -cf mudcode.tar $(SOURCES) $(HEADERS) Makefile start.db
	gzip mudcode.tar

clean:
	-rm -f *.o a.out core gmon.out $(OUTFILES)


$(C_OBJECTS):
	-rm -f $*.o
	$(CC) -c $(CFLAGS) $(OPTIM) $<

$(OPTIM_OBJECTS):
	-rm -f $*.o
	$(CPLUSPLUS) -c $(CFLAGS) $(OPTIM) $<

.SUFFIXES: .c .o

.c.o:
	-rm -f $*.o
	$(CPLUSPLUS) -c $(CFLAGS) $<

newversion:
	@if [ ! -f .version ]; then \
		echo 1 > .version; \
	else \
		expr `cat .version` + 1 > .version; \
	fi

version.h: newversion
	@echo \#define VERSION \"[UglyCODE Release `head -1 tag_list | sed 's,.Name:,,; s,[#-%],,'` build \#`cat .version` by `${WHOAMI} | sed 's, .*,,'` at `date`]\" > version.h

alarm.o : alarm.c \
	alarm.h \
	config.h \
	context.h \
	copyright.h \
	db.h \
	mudstring.h \
	externs.h \
	match.h \
	interface.h

boolexp.o: boolexp.c \
	context.h \
	copyright.h \
	db.h \
	mudstring.h \
	match.h \
	externs.h \
	config.h \
	interface.h

channel.o: channel.c \
	channel.h \
	colour.h \
	externs.h \
	context.h \
	interface.h \
	objects.h \
	db.h \
	mudstring.h \
	command.h

colour.o: colour.c \
	db.h \
	mudstring.h \
	colour.h \
	context.h \
	command.h \
	externs.h \
	interface.h

combat.o: combat.c \
	db.h \
	mudstring.h \
	match.h

command.o: command.c \
	db.h \
	mudstring.h \
	interface.h \
	command.h \
	context.h \
	match.h

container.o: container.c \
	db.h \
	mudstring.h \
	config.h \
	interface.h \
	externs.h \
	command.h

context.o: context.c \
	alarm.h \
	command.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h

create.o: create.c \
	db.h \
	mudstring.h \
	config.h \
	interface.h \
	externs.h \
	command.h \
	match.h \
	objects.h

db.o: db.c \
	db.h \
	mudstring.h \
	externs.h \
	config.h \
	interface.h \
	objects.h

decompile.o: decompile.c \
	db.h \
	mudstring.h \
	config.h \
	interface.h \
	externs.h \
	match.h \
	command.h

destroy.o: destroy.c \
	db.h \
	mudstring.h \
	config.h \
	interface.h \
	externs.h \
	lists.h \
	match.h \
	command.h

dump.o: dump.c \
	db.h \
	mudstring.h \
	copyright.h

extract.o: extract.c \
	mudstring.h \
	db.h

fuses.o: fuses.c \
	alarm.h \
	context.h \
	db.h \
	mudstring.h \
	interface.h \
	externs.h \
	command.h \
	match.h

game.o: game.c \
	db.h \
	mudstring.h \
	config.h \
	context.h \
	game.h \
	interface.h \
	match.h \
	externs.h

game_predicates.o: game_predicates.c \
	game_predicates.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	interface.h \
	externs.h

get.o: get.c \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	game_predicates.h \
	interface.h \
	match.h

group.o: group.c \
	command.h \
	objects.h \
	context.h

interface.o: interface.c \
	colour.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	config.h \
	command.h \
	concentrator.h \
	context.h \
	lists.h \
	telnet.h \
	descriptor.h \
	game_predicates.h

lists.o: lists.c \
	colour.h \
	context.h \
	command.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	objects.h

log.o: log.c \
	command.h \
	context.h \
	externs.h

look.o: look.c \
	colour.h \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	match.h \
	regexp_interface.h

match.o: match.c \
	db.h \
	mudstring.h \
	command.h \
	config.h \
	context.h \
	externs.h \
	match.h

move.o: move.c \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	match.h \
	stack.h

netmud.o: netmud.c \
	command.h \
	config.h \
	copyright.h \
	db.h \
	mudstring.h \
	externs.h \
	game.h \
	interface.h \
	version.h

npc.o:	npc.c \
	command.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	objects.h

objects.o: objects.c \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	objects.h

paths.o: paths.c \
	config.h \
	db.h \
	mudstring.h \
	interface.h

player.o: player.c \
	config.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	objects.h

predicates.o: predicates.c \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h

regexp.o: regexp.c

rob.o: rob.c \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	match.h

sanity-check.o: sanity-check.c \
	externs.h \
	db.h \
	mudstring.h \
	config.h

scat.o: scat.c

set.o: set.c \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	match.h

smd.o: smd.c \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h

speech.o: speech.c \
	colour.h \
	command.h \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	lists.h \
	match.h

stringutil.o: stringutil.c \
	externs.h \
	db.h \
	mudstring.h

unparse.o: unparse.c \
	config.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	colour.h

utils.o: utils.c \
	db.h \
	mudstring.h \
	interface.h \
	externs.h

variable.o: variable.c\
	db.h\
	mudstring.h \
	externs.h\
	config.h\
	command.h\
	context.h \
	match.h

wiz.o: wiz.c \
	command.h \
	context.h \
	db.h \
	mudstring.h \
	externs.h \
	interface.h \
	match.h

concentrator.o: concentrator.c \
	concentrator.h \
	config.h 

alarm.h: match.h

command.h: db.h \
	mudstring.h

config.h: copyright.h

context.h: command.h \
	match.h \
	stack.h

db.h: copyright.h \
	colour.h \
	mudstring.h

externs.h: db.h \
	mudstring.h \
	context.h \
	copyright.h

interface.h: db.h \
	mudstring.h \
	copyright.h

match.h: db.h \
	mudstring.h \
	copyright.h
