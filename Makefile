MAKEFLAGS=-k -B
# This makefile uses the following features that are limited to gmake:
# - Include lists that cause regeneration of the files (include *.d)
# - Variable substitution $(FRED:%=builds/$(VAR)/%)
#
# If you want to use another make, ensure that it supports these features.

# This Makefile requires lots of OS-specific detection and flag setting.
# Moved out of this Makefile by PJC 12/4/03 to see the wood for the trees.
include Makefile.os

# All files that are built go in BUILD_DIR - one per OS.
BUILDS_DIR:=builds
BUILD_DIR:=$(BUILDS_DIR)/$(BUILD_ENVIRONMENT)

by_default_just_make: $(BUILDS_DIR) $(BUILD_DIR) $(BUILD_DIR)/netmud

INCLUDE:=configs/$(BUILD_ENVIRONMENT)/include
# Whatever you put in for $(CC) must be able to grok ANSI C.
CC:=gcc $(INCLUDE:%=-I%)
CPLUSPLUS:=g++ $(INCLUDE:%=-I%) -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion
# TODO: Separate CPPFLAGS and CFLAGS in Makefile.os. PJC 12/4/03.
CPPFLAGS:= $(CFLAGS)

VERSION=`head -1 tag_list | sed 's,.Name:,,; s,[ $$],,g; s,^ *$$,TESTCODE,'`

SOURCES:=\
	alarm.c \
	ashcheck.c \
	boolexp.c \
	channel.c \
	chat.c \
	colour.c \
	colouring.c \
	command.c \
	concentrator.c \
	container.c \
	context.c \
	create.c \
	db.c \
	debug.c \
	decompile.c \
	destroy.c \
	dump.c \
	editor.c \
	eval.c \
	extract.c \
	fuses.c \
	game.c \
	game_predicates.c \
	get.c \
	group.c \
	interface.c \
	lists.c \
	log.c \
	look.c \
	match.c \
	memory.c \
	mondodestruct.c \
	move.c \
	mudstring.c \
	mudwho.c \
	netmud.c \
	npc.c \
	objects.c \
	paths.c \
	player.c \
	predicates.c \
	regexp.c \
	rob.c \
	sadness.c \
	sanity-check.c \
	scat.c \
	set.c \
	smd.c \
	speech.c \
	stack.c \
	stats.c \
	stringutil.c \
	trawl.c \
	unparse.c \
	utils.c \
	variable.c \
	wiz.c

# C_OBJECTS are compiled with gcc, not g++
C_OBJECTS = \
	regexp.o \
	scat.o \
	concentrator.o 

RAW_UTIL_OBJECTS:= \
	db.o \
	objects.o \
	predicates.o \
	unparse.o \
	utils.o \
	debug.o \
	mudstring.o

UTIL_OBJECTS:=$(RAW_UTIL_OBJECTS:%=$(BUILD_DIR)/%)

RAW_OBJECTS:= \
	$(RAW_UTIL_OBJECTS) \
	alarm.o \
	boolexp.o \
	channel.o \
	colour.o \
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

OBJECTS:=$(RAW_OBJECTS:%=$(BUILD_DIR)/%)

RAW_OUTFILES:= \
	dump \
	extract \
	mondodestruct \
	netmud \
	paths \
	sanity-check \
	sadness \
	stats \
	concentrator

OUTFILES:=$(RAW_OUTFILES:%=$(BUILD_DIR)/%)

all: $(BUILDS_DIR) $(BUILD_DIR) $(OUTFILES)

$(BUILDS_DIR):
	mkdir -p $(BUILDS_DIR)

$(BUILD_DIR): $(BUILDS_DIR)
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/netmud: $(OBJECTS) $(BUILD_DIR)/netmud.o
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/netmud.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/dump: $(BUILD_DIR)/dump.o $(UTIL_OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/dump.o $(UTIL_OBJECTS) $(LIBS)

$(BUILD_DIR)/sadness: $(BUILD_DIR)/sadness.o $(OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/sadness.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/mondodestruct: $(BUILD_DIR)/mondodestruct.o $(OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/mondodestruct.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/trawl: $(BUILD_DIR)/trawl.o $(OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/trawl.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/ashcheck: $(BUILD_DIR)/ashcheck.o $(OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/ashcheck.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/stats: $(BUILD_DIR)/stats.o $(UTIL_OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/stats.o $(UTIL_OBJECTS) $(LIBS)

$(BUILD_DIR)/sanity-check: $(BUILD_DIR)/sanity-check.o $(UTIL_OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/sanity-check.o $(UTIL_OBJECTS) $(LIBS)

$(BUILD_DIR)/extract: $(BUILD_DIR)/extract.o $(UTIL_OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/extract.o $(UTIL_OBJECTS) $(LIBS)

$(BUILD_DIR)/colouring: $(BUILD_DIR)/colouring.o $(OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/colouring.o $(OBJECTS) $(LIBS)

$(BUILD_DIR)/paths: $(BUILD_DIR)/paths.o $(UTIL_OBJECTS)
	-rm -f $@
	$(CPLUSPLUS) $(CPPFLAGS) -o $@ $(BUILD_DIR)/paths.o $(UTIL_OBJECTS) $(LIBS)

$(BUILD_DIR)/scat: $(BUILD_DIR)/scat.o
	-rm -f $@
	$(CC) -o $@ $< $(LIBS)

$(BUILD_DIR)/concentrator: $(BUILD_DIR)/concentrator.o 
	-rm -f $@
	$(CC) $(CFLAGS) -o $@ $<

clean:
	-rm -f $(BUILD_DIR)/*


$(C_OBJECTS):
	-rm -f $@
	$(CC) -c -o $@ $(CFLAGS) $<

$(OBJECTS):
	-rm -f $@
	$(CPLUSPLUS) -c -o $@ $(CPPFLAGS) $<

.SUFFIXES: .c .o

.c.o:
	-rm -f $@
	$(CPLUSPLUS) -c -o $@ $(CPPFLAGS) $<

newversion:
	@if [ ! -f .version ]; then \
		echo 1 > .version; \
	else \
		expr `cat .version` + 1 > .version; \
	fi

version.h: newversion
	@echo \#define RELEASE \"$(VERSION)\" > version.h
	@echo \#define VERSION \"[UglyCODE Release \" RELEASE \" build \#`cat .version` by `${WHOAMI} | sed 's, .*,,'` at `date`]\" >> version.h

# Generate our prerequisites.  Plagiarised from the gmake manual by PJC 12/4/03.
# C and C++ files...
$(BUILD_DIR)/%.d: %.c
	@echo Generating dependency file $@...
	@set -e; rm -f $@; \
	$(CPLUSPLUS) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed "s,\\($*\\)\\.o[ :]*,$(BUILD_DIR)/\\1.o $@ : ,g" < $@.$$$$ > $@; \
	rm -f $@.$$$$

# Obtain dependencies for all our files
include $(SOURCES:%.c=$(BUILD_DIR)/%.d)
