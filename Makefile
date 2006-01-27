MAKEFLAGS=-k
# This makefile uses the following features that are limited to gmake:
# - Include lists that cause regeneration of the files (include *.d)
# - Variable substitution $(FRED:%=builds/$(VAR)/%)
#
# If you want to use another make, ensure that it supports these features.

# This Makefile requires lots of OS-specific detection and flag setting.
# Moved out of this Makefile by PJC 12/4/03 to see the wood for the trees.
include Makefile.os
CONFIG_DIR:=configs/$(BUILD_ENVIRONMENT)
include $(CONFIG_DIR)/Makefile.os

# All files that are built go in BUILD_DIR - one per OS.
BUILDS_DIR:=builds
BUILD_DIR:=$(BUILDS_DIR)/$(BUILD_ENVIRONMENT)

# Need to decide if we're using old regexp or new PCRE for regular expressions.
# Use the following if you're using PCRE. NOTE: On something you will need to use -R as well.
include $(BUILD_DIR)/Makefile.local

LOCALLIBS=$(LD_REGEXP) $(LD_LUA)
LOCALCCFLAGS=$(CC_REGEXP) $(CC_LUA)

by_default_just_make: $(BUILDS_DIR) $(BUILD_DIR) $(BUILD_DIR)/netmud$(EXESUFFIX)

checkenvironment:
	@echo Your environment appears to be: '$(BUILD_ENVIRONMENT)'

INCLUDE:=configs/$(BUILD_ENVIRONMENT)/include
# Whatever you put in for $(CC) must be able to grok ANSI C.
CC:=gcc $(INCLUDE:%=-I%) $(CC_REGEXP) $(CC_LUA)
CPLUSPLUS:=g++ $(INCLUDE:%=-I%) -Wall -Wcast-qual -Wparentheses -Wwrite-strings -Wconversion $(CC_REGEXP) $(CC_LUA)
# TODO: Separate CPPFLAGS and CFLAGS in Makefile.os. PJC 12/4/03.
CPPFLAGS:= $(CFLAGS)
LIBUGLY:= $(BUILD_DIR)/libugly.a

VERSION=`head -1 tag_list | sed 's,.Name:,,; s,[ $$],,g; s,^ *$$,TESTCODE,'`

SOURCES:=\
	alarm.c \
	ashcheck.c \
	boolexp.c \
	channel.c \
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
	stats.c \
	stringutil.c \
	trawl.c \
	unparse.c \
	utils.c \
	variable.c \
	wiz.c \
	luainterp.c

RAW_LIB_OBJECTS:= \
	alarm.o \
	boolexp.o \
	channel.o \
	colour.o \
	command.o \
	container.o \
	context.o \
	create.o \
	db.o \
	debug.o \
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
	mudstring.o \
	objects.o \
	player.o \
	predicates.o \
	regexp.o \
	rob.o \
	sanity-check.o \
	set.o \
	smd.o \
	speech.o \
	stringutil.o \
	unparse.o \
	utils.o \
	variable.o \
	wiz.o \
	luainterp.o

RAW_OUTFILES:= \
	trawl \
	dump \
	extract \
	mondodestruct \
	netmud \
	paths \
	sadness \
	scat \
	stats \
	concentrator 

# Include anything that needs doing for a specific build environment. PJC 20/4/2003.
include $(CONFIG_DIR)/Makefile

LIB_OBJECTS:=$(RAW_LIB_OBJECTS:%=$(BUILD_DIR)/%) $(OS_RAW_LIB_OBJECTS:%=$(BUILD_DIR)/%)

OUTFILES:=$(RAW_OUTFILES:%=$(BUILD_DIR)/%$(EXESUFFIX))

# OUTERMEDIATES contains a list of intermediate output files - the .o files
# needed to compile into the eventual binaries.  PJC 20/4/2003.
OUTERMEDIATES:=$(RAW_OUTFILES:%=$(BUILD_DIR)/%.o)

all: $(BUILDS_DIR) $(BUILD_DIR) $(OUTFILES)

$(BUILDS_DIR):
	mkdir -p $(BUILDS_DIR)

$(BUILD_DIR): $(BUILDS_DIR)
	mkdir -p $(BUILD_DIR)

$(LIBUGLY): $(LIB_OBJECTS)
	ar rv $@ $?
	ranlib $@

$(BUILD_DIR)/netmud$(EXESUFFIX): newversion $(LIBUGLY) $(BUILD_DIR)/netmud.o
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/netmud.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/dump$(EXESUFFIX): $(BUILD_DIR)/dump.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/dump.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/sadness$(EXESUFFIX): $(BUILD_DIR)/sadness.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/sadness.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/mondodestruct$(EXESUFFIX): $(BUILD_DIR)/mondodestruct.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/mondodestruct.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/trawl$(EXESUFFIX): $(BUILD_DIR)/trawl.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/trawl.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/ashcheck$(EXESUFFIX): $(BUILD_DIR)/ashcheck.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/ashcheck.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/stats$(EXESUFFIX): $(BUILD_DIR)/stats.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/stats.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/extract$(EXESUFFIX): $(BUILD_DIR)/extract.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/extract.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/colouring$(EXESUFFIX): $(BUILD_DIR)/colouring.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/colouring.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/paths$(EXESUFFIX): $(BUILD_DIR)/paths.o $(LIBUGLY)
	-rm -f $@
	$(CPLUSPLUS) $(CPPLDFLAGS) -o $@ $(BUILD_DIR)/paths.o $(LIBUGLY) $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/scat$(EXESUFFIX): $(BUILD_DIR)/scat.o
	-rm -f $@
	$(CPLUSPLUS) -o $@ $< $(LOCALLIBS) $(LIBS)

$(BUILD_DIR)/concentrator$(EXESUFFIX): $(BUILD_DIR)/concentrator.o 
	-rm -f $@
	$(CPLUSPLUS) $(CFLAGS) -o $@ $<

clean:
	-rm -f $(BUILD_DIR)/*


$(LIB_OBJECTS) $(OUTERMEDIATES):
	-rm -f $@
	$(CPLUSPLUS) -c -o $@ $(CPPFLAGS) $<

newversion:
	@echo "Making new version..."
	@if [ ! -f .version ]; then \
		echo 1 > .version; \
	else \
		expr `cat .version` + 1 > .version; \
	fi

version.h:
	@echo \#define RELEASE \"$(VERSION)\" > version.h
	@echo \#define VERSION \"[UglyCODE Release \" RELEASE \" build \#`cat .version` by `${WHOAMI}` at `date`]\" >> version.h

# Generate our prerequisites.  Plagiarised from the gmake manual by PJC 12/4/03.
# C and C++ files...
$(BUILD_DIR)/%.d: %.c version.h
	@echo Generating dependency file $@...
	@set -e; rm -f $@; \
	$(CPLUSPLUS) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed "s,\\($*\\)\\.o[ :]*,$(BUILD_DIR)/\\1.o $@ : ,g" < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(BUILD_DIR)/Makefile.local: configure
	sh ./configure $(BUILD_DIR)/Makefile.local

# regexp_interface.h changes depending on the local configuration.
regexp_interface.h: $(BUILD_DIR)/Makefile.local

# Obtain dependencies for all our files
include $(SOURCES:%.c=$(BUILD_DIR)/%.d)
