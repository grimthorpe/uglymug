/** \file
 * This little lot following is so that the database load/save knows the number
 * of the field to save for each field on each object. When adding fields to 
 * objects you need to make sure that all classes dependant on that also 
 * have the field added with the _same_ number (for obvious reasons).
 */

/* When adding new fields you will need to add the appropriate field read to
 * each switch statement for every object class dependant on it. Adding a 
 * new object class to be read requires a lot of lines of code for the new
 * switch statement necessary. I know this sounds a very bad way of doing
 * things but it enables object fields to be read in any order. Hand
 * patching databases should be simple. */

/* Beta(tagged) db format completed 1/3/96, Dale Thompson */

#ifndef	DB_FIELDS_H
#define	DB_FIELDS_H

enum object_fields { /* The base object fields */
OBJECT_FLAGS	= 128,
OBJECT_NAME	= 129,
OBJECT_LOCATION = 130,
OBJECT_NEXT	= 131,
OBJECT_OWNER	= 132
};


enum desc_object_fields { /* An extension of class object */
DOBJECT_FLAGS    = OBJECT_FLAGS,
DOBJECT_NAME     = OBJECT_NAME,
DOBJECT_LOCATION = OBJECT_LOCATION,
DOBJECT_NEXT     = OBJECT_NEXT,
DOBJECT_OWNER    = OBJECT_OWNER,
DOBJECT_DESC	 = 133,
};

enum lock_object_fields { /* An extension of Describable Object */
LOBJECT_FLAGS    = OBJECT_FLAGS,
LOBJECT_NAME     = OBJECT_NAME,
LOBJECT_LOCATION = OBJECT_LOCATION,
LOBJECT_NEXT     = OBJECT_NEXT,
LOBJECT_OWNER    = OBJECT_OWNER,
LOBJECT_DESC      = DOBJECT_DESC,
LOBJECT_KEY       = 134,
LOBJECT_FAIL      = 135,
LOBJECT_SUCC      = 136,
LOBJECT_DROP      = 137,
LOBJECT_OFAIL     = 138,
LOBJECT_OSUCC     = 139,
LOBJECT_ODROP     = 140
};

enum inherit_object_fields { /* An extension of Lockable Object */
IOBJECT_FLAGS    = OBJECT_FLAGS,
IOBJECT_NAME     = OBJECT_NAME,
IOBJECT_LOCATION = OBJECT_LOCATION,
IOBJECT_NEXT     = OBJECT_NEXT,
IOBJECT_OWNER    = OBJECT_OWNER,
IOBJECT_DESC      = DOBJECT_DESC,
IOBJECT_KEY       = LOBJECT_KEY,
IOBJECT_FAIL      = LOBJECT_FAIL,
IOBJECT_SUCC      = LOBJECT_SUCC,
IOBJECT_DROP      = LOBJECT_DROP,
IOBJECT_OFAIL     = LOBJECT_OFAIL,
IOBJECT_OSUCC     = LOBJECT_OSUCC,
IOBJECT_ODROP     = LOBJECT_ODROP,
IOBJECT_PARENT	  = 141 
};

enum old_object_fields { /* An extension of Inheritable Object */
OOBJECT_FLAGS    = OBJECT_FLAGS,
OOBJECT_NAME     = OBJECT_NAME,
OOBJECT_LOCATION = OBJECT_LOCATION,
OOBJECT_NEXT     = OBJECT_NEXT,
OOBJECT_OWNER    = OBJECT_OWNER,
OOBJECT_DESC      = DOBJECT_DESC,
OOBJECT_KEY       = LOBJECT_KEY,
OOBJECT_FAIL      = LOBJECT_FAIL,
OOBJECT_SUCC      = LOBJECT_SUCC,
OOBJECT_DROP      = LOBJECT_DROP,
OOBJECT_OFAIL     = LOBJECT_OFAIL,
OOBJECT_OSUCC     = LOBJECT_OSUCC,
OOBJECT_ODROP     = LOBJECT_ODROP,
OOBJECT_PARENT	= IOBJECT_PARENT,
OOBJECT_COMMANDS	= 142,
OOBJECT_CONTENTS	= 143,
OOBJECT_DESTINATION = 144,
OOBJECT_EXITS	= 145,
OOBJECT_FUSES	= 146,
OOBJECT_INFO	= 147
};

enum massy_object_fields { /* An extension of Old Object */
MOBJECT_FLAGS    = OBJECT_FLAGS,
MOBJECT_NAME     = OBJECT_NAME,
MOBJECT_LOCATION = OBJECT_LOCATION,
MOBJECT_NEXT     = OBJECT_NEXT,
MOBJECT_OWNER    = OBJECT_OWNER,
MOBJECT_DESC      = DOBJECT_DESC,
MOBJECT_KEY       = LOBJECT_KEY,
MOBJECT_FAIL      = LOBJECT_FAIL,
MOBJECT_SUCC      = LOBJECT_SUCC,
MOBJECT_DROP      = LOBJECT_DROP,
MOBJECT_OFAIL     = LOBJECT_OFAIL,
MOBJECT_OSUCC     = LOBJECT_OSUCC,
MOBJECT_ODROP     = LOBJECT_ODROP,
MOBJECT_PARENT  = IOBJECT_PARENT,
MOBJECT_COMMANDS        = OOBJECT_COMMANDS,
MOBJECT_CONTENTS        = OOBJECT_CONTENTS,
MOBJECT_DESTINATION = OOBJECT_DESTINATION,
MOBJECT_EXITS   = OOBJECT_EXITS,
MOBJECT_FUSES   = OOBJECT_FUSES,
MOBJECT_INFO    = OOBJECT_INFO,
MOBJECT_GF        = 148,
MOBJECT_MASS      = 149,
MOBJECT_VOLUME    = 150,
MOBJECT_MASSLIM   = 151,
MOBJECT_VOLUMELIM = 152 
};

enum Puppet_fields { /* An extended Massy Object */
PUPPET_FLAGS     = OBJECT_FLAGS,
PUPPET_NAME      = OBJECT_NAME,
PUPPET_LOCATION  = OBJECT_LOCATION,
PUPPET_NEXT      = OBJECT_NEXT,
PUPPET_OWNER     = OBJECT_OWNER,
PUPPET_DESC      = DOBJECT_DESC,
PUPPET_KEY       = LOBJECT_KEY,
PUPPET_FAIL      = LOBJECT_FAIL,
PUPPET_SUCC      = LOBJECT_SUCC,
PUPPET_DROP      = LOBJECT_DROP,
PUPPET_OFAIL     = LOBJECT_OFAIL,
PUPPET_OSUCC     = LOBJECT_OSUCC,
PUPPET_ODROP     = LOBJECT_ODROP,
PUPPET_PARENT    = IOBJECT_PARENT,
PUPPET_COMMANDS  = OOBJECT_COMMANDS,
PUPPET_CONTENTS  = OOBJECT_CONTENTS,
PUPPET_DESTINATION = OOBJECT_DESTINATION,
PUPPET_EXITS     = OOBJECT_EXITS,
PUPPET_FUSES     = OOBJECT_FUSES,
PUPPET_INFO      = OOBJECT_INFO,
PUPPET_GF        = MOBJECT_GF,
PUPPET_MASS      = MOBJECT_MASS,
PUPPET_VOLUME    = MOBJECT_VOLUME,
PUPPET_MASSLIM   = MOBJECT_MASSLIM,
PUPPET_VOLUMELIM = MOBJECT_VOLUMELIM,
PUPPET_PENNIES	 = 153,
PUPPET_SCORE	= 154,
PUPPET_RACE	= 155,
PUPPET_WHOSTRING = 156,
PUPPET_EMAIL	= 157,
PUPPET_BUILDID	= 160,
PUPPET_MONEY	= 163
};

enum Player_fields { /* An extension of puppet */
PLAYER_FLAGS     = OBJECT_FLAGS,
PLAYER_NAME      = OBJECT_NAME,
PLAYER_LOCATION  = OBJECT_LOCATION,
PLAYER_NEXT      = OBJECT_NEXT,
PLAYER_OWNER     = OBJECT_OWNER,
PLAYER_DESC      = DOBJECT_DESC,
PLAYER_KEY       = LOBJECT_KEY,
PLAYER_FAIL      = LOBJECT_FAIL,
PLAYER_SUCC      = LOBJECT_SUCC,
PLAYER_DROP      = LOBJECT_DROP,
PLAYER_OFAIL     = LOBJECT_OFAIL,
PLAYER_OSUCC     = LOBJECT_OSUCC,
PLAYER_ODROP     = LOBJECT_ODROP,
PLAYER_PARENT    = IOBJECT_PARENT,
PLAYER_COMMANDS  = OOBJECT_COMMANDS,
PLAYER_CONTENTS  = OOBJECT_CONTENTS,
PLAYER_DESTINATION = OOBJECT_DESTINATION,
PLAYER_EXITS     = OOBJECT_EXITS,
PLAYER_FUSES     = OOBJECT_FUSES,
PLAYER_INFO      = OOBJECT_INFO,
PLAYER_GF	 = MOBJECT_GF,
PLAYER_MASS	 = MOBJECT_MASS,
PLAYER_VOLUME	 = MOBJECT_VOLUME,
PLAYER_MASSLIM	 = MOBJECT_MASSLIM,
PLAYER_VOLUMELIM = MOBJECT_VOLUMELIM,
PLAYER_PENNIES	 = PUPPET_PENNIES,
PLAYER_SCORE	= PUPPET_SCORE,
PLAYER_RACE	= PUPPET_RACE,
PLAYER_WHOSTRING = PUPPET_WHOSTRING,
PLAYER_EMAIL	= PUPPET_EMAIL,
PLAYER_PASSWORD	= 158,
PLAYER_CONTROLLER = 159,
PLAYER_BUILDID	= 160,
PLAYER_COLOUR	= 161,
PLAYER_ALIASES	= 162,
PLAYER_MONEY	= PUPPET_MONEY
};

enum Room_fields { /* An extended Massy Object */
ROOM_FLAGS    = OBJECT_FLAGS,
ROOM_NAME     = OBJECT_NAME,
ROOM_LOCATION = OBJECT_LOCATION,
ROOM_NEXT     = OBJECT_NEXT,
ROOM_OWNER    = OBJECT_OWNER,
ROOM_DESC      = DOBJECT_DESC,
ROOM_KEY       = LOBJECT_KEY,
ROOM_FAIL      = LOBJECT_FAIL,
ROOM_SUCC      = LOBJECT_SUCC,
ROOM_DROP      = LOBJECT_DROP,
ROOM_OFAIL     = LOBJECT_OFAIL,
ROOM_OSUCC     = LOBJECT_OSUCC,
ROOM_ODROP     = LOBJECT_ODROP,
ROOM_PARENT  = IOBJECT_PARENT,
ROOM_COMMANDS        = OOBJECT_COMMANDS,
ROOM_CONTENTS        = OOBJECT_CONTENTS,
ROOM_DESTINATION = OOBJECT_DESTINATION,
ROOM_EXITS   = OOBJECT_EXITS,
ROOM_FUSES   = OOBJECT_FUSES,
ROOM_INFO    = OOBJECT_INFO,
ROOM_GF        = MOBJECT_GF,
ROOM_MASS      = MOBJECT_MASS,
ROOM_VOLUME    = MOBJECT_VOLUME,
ROOM_MASSLIM   = MOBJECT_MASSLIM,
ROOM_VOLUMELIM = MOBJECT_VOLUMELIM,
ROOM_LASTENTRY	= 153, 
ROOM_CONTSTR	= 154
};

enum Thing_fields { /* An extended Massy Object */
THING_FLAGS    = OBJECT_FLAGS,
THING_NAME     = OBJECT_NAME,
THING_LOCATION = OBJECT_LOCATION,
THING_NEXT     = OBJECT_NEXT,
THING_OWNER    = OBJECT_OWNER,
THING_DESC      = DOBJECT_DESC,
THING_KEY       = LOBJECT_KEY,
THING_FAIL      = LOBJECT_FAIL,
THING_SUCC      = LOBJECT_SUCC,
THING_DROP      = LOBJECT_DROP,
THING_OFAIL     = LOBJECT_OFAIL,
THING_OSUCC     = LOBJECT_OSUCC,
THING_ODROP     = LOBJECT_ODROP,
THING_PARENT  = IOBJECT_PARENT,
THING_COMMANDS        = OOBJECT_COMMANDS,
THING_CONTENTS        = OOBJECT_CONTENTS,
THING_DESTINATION = OOBJECT_DESTINATION,
THING_EXITS   = OOBJECT_EXITS,
THING_FUSES   = OOBJECT_FUSES,
THING_INFO    = OOBJECT_INFO,
THING_GF        = MOBJECT_GF,
THING_MASS      = MOBJECT_MASS,
THING_VOLUME    = MOBJECT_VOLUME,
THING_MASSLIM   = MOBJECT_MASSLIM,
THING_VOLUMELIM = MOBJECT_VOLUMELIM,
THING_CONTSTR	= 153,
THING_KEYNOFREE	= 154 
};

enum Dictionary_fields { /* An extension of class object */
DICTIONARY_FLAGS     = OBJECT_FLAGS,
DICTIONARY_NAME      = OBJECT_NAME,
DICTIONARY_LOCATION  = OBJECT_LOCATION,
DICTIONARY_NEXT      = OBJECT_NEXT,
DICTIONARY_OWNER     = OBJECT_OWNER,
DICTIONARY_ELEMENTS	= 133 
};

enum Array_storage_fields { /* Things that store their descs as arrays (everything) */
ARRAY_STORAGE_ELEMENTS = 170
};

enum Array_fields { /* An extension of class object */
ARRAY_FLAGS     = OBJECT_FLAGS,
ARRAY_NAME      = OBJECT_NAME,
ARRAY_LOCATION  = OBJECT_LOCATION,
ARRAY_NEXT      = OBJECT_NEXT,
ARRAY_OWNER     = OBJECT_OWNER,
ARRAY_ELEMENTS	= 133 
};

#endif	/* DB_FIELDS_H */
