/* static char SCCSid[] = "@(#)smd.c	1.23\t10/13/95"; */
/*
 * smd.c - site monitoring device
 */

#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "interface.h"
#include "externs.h"
#include "config.h"
#include "command.h"
#include "context.h"
#include "colour.h"

struct smd
{
	int		inet;		/* Inet or JANET site */
	u_long		host;		/* host address */
	u_long		mask;		/* mask for the address */
	int		flags;		/* Misc. flags */
	Boolean		temp;		/* whether it is a temporary (not written to file) ban */
	struct	smd	*next;		/* Next entry */
};

static struct smd *smd_list = NULL;

#define MALLOC(result, type) do {				\
	if (!((result) = (type *) malloc (sizeof (type))))	\
        	panic("Out of memory");				\
	} while (0)

#define SEPARATORS " \t\n"

/* Flags in the smd struct */
#define BANNED		1
#define DNS		2
#define	NOCREATE	4
#define NOGUESTS	8

#define DEFAULT_FLAGS (~BANNED & ~DNS & ~NOCREATE & ~NOGUESTS)/* addr: not banned, no DNS lookup, can create, can use guests */
#define	DEFAULT_MASK 0xffffffff		/* mask: address given is a machine */


static Boolean
is_in_smd_list (u_long a)
{
	struct smd *b;

	for (b = smd_list; b != NULL; b = b->next)
	{
		if (a == b->host)
			return True;
	}

	return False;
}

static void
replace_smd_entry (u_long a, struct smd *b)
{
	struct smd *c;
	
	for (c = smd_list; c != NULL; c = c->next)
	{
		if (a == c->host)
		{
			c->inet		= b->inet;
			c->host		= b->host;
			c->mask		= b->mask;
			c->flags	= b->flags;
			c->temp		= b->temp;
		}
	}
}

static Boolean
remove_smd_entry (u_long a)
{
	struct smd	*b,
			*x;

	if (! is_in_smd_list(a))
		return False;

	/*
	 * Found a matching entry - now we just need to remove it.
	 */

	for (b = smd_list; b != NULL; b = b->next)
	{
		if (b->host == a)
		{
			if (b == smd_list)
			{
				smd_list = b->next;
				free(b);
			}
			else
			{
				for (x = smd_list; x->next != b; x = x->next)
					;	/* Find the element before b */

				if (b->next == NULL)
					x->next = NULL;
				else
					x->next = b->next;

				free (b);
			}
		}
	}

	return True;
}
static int
get_smd_flags(u_long a)
{
	struct smd *b;

	for(b = smd_list; b != NULL; b = b->next)
	{
		if((a & (b -> mask)) == (b -> host))    /* Match */
			return (b -> flags);
	}
	return DEFAULT_FLAGS;
}

static u_long   
addr_numtoint(const char *addr, int inet)
{
	if(inet)
		return (ntohl(inet_addr(addr)));

	/* No JANET support yet... */
	return 0xffffffff;
}

static u_long
addr_nametoint(const char *addr, int inet)
{
	struct hostent *a;
	if(inet)
	{
		a = gethostbyname(addr);
		if(a!= NULL)
			return ntohl(((struct in_addr *)a->h_addr_list[0])->s_addr);
		else
			return 0xffffffff;
	}

	/* No JANET support yet... */
	return 0xffffffff;
}

static u_long
getmask(char *mask, int inet)
{
	if(inet)
		return strtoul(mask, (char **)NULL, 0);

	return 0xffffffff;
}

void
context::do_smdread (
const	char	*,
const	char	*)

{
	struct smd *a, *b;
	FILE *smdfile;
	char smdinp[ BUFFER_LEN ];
	char *inet;
	char *addr;
	char *flag;
	int eol = 0;
	u_long tmp_host;
	u_long tmp_inet;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Rereading of SMD list not allowed inside commands");
		return;
	}

	if (!Wizard(player))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You want to get yourself banned?");
		return;
	}

	/* Get rid of the old smd list */
	a = smd_list;
	while(a != NULL)
	{
		b = a;
		a = a -> next;
		free(b);
	}
	smd_list = NULL;

	if((smdfile = fopen(SMD_FILE, "r")) == NULL)
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "SMD file wont open");
		return;
	}

	while(fgets(smdinp, 1024, smdfile) != NULL)
	{
		if((inet = strtok(smdinp, SEPARATORS)) && (inet[0] != '#'))
		{
			addr = strtok(NULL, SEPARATORS);

			switch (inet[0])	/* Inet or JANET switch */
			{
			case 'i':	/* Inet */
			case 'I':
				tmp_inet = 1;
				break;
			default:
				tmp_inet = 0;
			}
			if((*addr >= '0') && (*addr <= '9'))
				tmp_host = addr_numtoint(addr, tmp_inet);
			else
				tmp_host = addr_nametoint(addr, tmp_inet);

			if(tmp_host == 0xffffffff)	/* If machine doesn't exist */
				continue;	/* forget it */

			MALLOC(a, struct smd);
			a -> temp = 0;			/* Not temporary */
			a -> host = tmp_host;
			a -> inet = tmp_inet;
			if(tmp_inet)
				a -> mask = DEFAULT_MASK;

			a -> flags = DEFAULT_FLAGS;

			while(((flag = strtok(NULL, SEPARATORS)) != NULL) &&
				(flag[0] != '#') && (!eol))
			{
				switch(flag[0])
				{
					case 'a': /* Allow */
					case 'A':
						a->flags &= ~BANNED;
						break;
					case 'b': /* BAN */
					case 'B':
						a->flags |= BANNED;
						break;
					case 'c': /* Create */
					case 'C':
						a->flags &= ~NOCREATE;
						break;
					case 'd': /* DNS */
					case 'D':
						a->flags |= DNS;
						break;
					case 'n': /* NODNS/NOCREATE */
					case 'N':
						switch(flag[2])
						{
							case 'd':
							case 'D':
								a->flags &= ~DNS;
								break;
							case 'c':
							case 'C':
								a->flags |= NOCREATE;
								break;
							case 'g':
							case 'G':
								a->flags |= NOGUESTS;
						}
						break;
					case 'm':
					case 'M': /* MASK */
						if((flag = strtok(NULL, SEPARATORS)) != NULL)
						{
							if(flag[0] != '#')
							{
								a -> mask = getmask(flag, a->inet);
								a -> host = (a -> host) & (a -> mask);
							}
							else
								eol = 1;
						}
						break;
				}
			}

			if(smd_list == NULL)
				smd_list = a;
			else
				b -> next = a;
			b = a;
			a -> next = NULL;
		}
	}
	fclose(smdfile);

	smd_updated();

	if(Connected(player))
		notify_colour(player, player, COLOUR_MESSAGES, "SMD file read");
	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;
}

static char*
addr_inttostr(struct smd *a)
{
static char z[80];
u_long b;
	b = a -> host;
	if(a -> inet)
	{
		sprintf(z, "inet %3ld.%3ld.%3ld.%3ld", (b>>24)&0xff, (b>>16)&0xff, (b>>8)&0xff, b&0xff);
	}
	else
		z[0] = 0;
	return z;
}

void
context::do_smd (
const	char	*arg1,
const	char	*arg2)
{
	struct smd *a;
	return_status = COMMAND_FAIL;
	const   char *const *const      ca = db[get_player()].get_colour_at();
	Boolean		matched = False;

	set_return_string (error_return_string);
	if(in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't do @smd in a command");
		return;
	}
	if(!Wizard(player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I'm sorry, but the SMD list is priveledged information");
		return;
	}

	if(blank(arg1))
	{
		notify(player, "%sUsage:%s  @smd <function> = <args>",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		notify(player, "        @smd check");
		notify(player, "        @smd read");
		notify(player, "        @smd list = <flag>");
		notify(player, "        @smd ban  = <host>[/<mask>]");
		notify(player, "%sType \"%shelp @smd%s\" for more information.%s", ca[COLOUR_MESSAGES],  ca[COLOUR_COMMANDS], ca[COLOUR_MESSAGES], COLOUR_REVERT );

		RETURN_FAIL;
	}

	if(strcasecmp(arg1, "list")==0)
	{
		matched = True;
		a = smd_list;
		notify_colour(player, player, COLOUR_MESSAGES,
			" Temporary  Host                  Mask        B  D  C  G ");

		if (Prettylook(player))
			notify_colour(player, player, COLOUR_UNDERLINES,
				      " ~~~~~~~~~  ~~~~                  ~~~~        ~~~~~~~~~~ ");
			
		while(a)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "%-11s %-21s 0x%08x  %-2s %-2s %-2s %-2s",
				      (a->temp == True) ? "<Temporary>" : "",
				      addr_inttostr(a),
				      (a->mask),
				      (a->flags & BANNED)?"b":"-",
				      (a->flags & DNS)?"d":"-",
				      (a->flags & NOCREATE)?"-":"c",
				      (a->flags & NOGUESTS)?"-":"g"
				      );
			a = a -> next;
		}

		notify_colour(player, player, COLOUR_MESSAGES, "[End of SMD List]");
		set_return_string (ok_return_string);

		RETURN_SUCC;
	}

	if(strcasecmp(arg1, "read")==0)
	{
		matched = True;
		do_smdread ((char *)NULL, (char *)NULL);
		RETURN_SUCC;
	}

	if(strcasecmp(arg1, "ban")==0)
	{
		unsigned	int	i;
		unsigned	long	host_num;
				char	*host;
				char	*mask;

		matched = True;

		/*
		 * Make sure we are trying to ban something
		 */
		if (blank(arg2))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Usage: @smd ban=aaa.bbb.ccc.ddd/www.xxx.yyy.zzz (host/mask)");
			RETURN_FAIL;
		}

		host = strdup(arg2);
		mask = strdup(arg2);

		MALLOC(a, struct smd);

		/*
		 * We expect the 2nd arg (host/mask) to look something like: aaa.bbb.ccc.ddd/255.255.255.0
		 */
		for (i=0; i < strlen(host); i++)
		{
			if (host[i] == '/')
				host[i] = '\0';
		}


		/*
		 * Check the site name seems ok
		 */

		host_num = addr_numtoint(host, 1);

		if (
		    (host_num == addr_numtoint("127.0.0.1", 1)) ||
		    (host_num == addr_numtoint("localhost", 1)) ||
		    (host_num == addr_numtoint("uglymug.org.uk", 1)) ||
		    (host_num == addr_numtoint("wyrm.compsoc.man.ac.uk", 1))
		   )
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Banning %s is strongly discouraged. Site not banned.", host);
			RETURN_FAIL;
		}

		if (host_num == 0xffffffff)
		{
			/* Might have enetered as name, not IP */
			host_num = addr_nametoint(host, 1);

			if (host_num == 0xffffffff)
			{
				 notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Failed to convert %s. Site not banned.", host);
				 RETURN_FAIL;
			}
		}

		/*
		 * If they don't specify a mask, tell them what default is being used.
		 * Also nullify mask so we know not to use it later.
		 */
		if (strlen(host) == strlen(arg2))
		{
#ifdef VERBOSE
			notify_colour(player, player, COLOUR_MESSAGES, "No mask specified. Using default mask of 0x%08x%s",
				      DEFAULT_MASK,
				      (DEFAULT_MASK == 0xffffffff) ? " (specific machine)" : ""
				      );
#endif
			mask = NULL;
		}
		else
			for (i = strlen(host) + 1; i < strlen(arg2); i++)
			{
				mask[i - (strlen(host) + 1)] = arg2[i];
				mask[(i - strlen(host)) + 2] = '\0';
			}

		
		/*
		 * Add the new ban
		 */
		a->inet		= 1;
		a->host		= host_num;
		a->flags	= DEFAULT_FLAGS | BANNED;
		a->temp		= True;

		if (mask == NULL)
			a->mask = DEFAULT_MASK;
		else
			a->mask	= addr_numtoint(mask, 1);

		/*
		 * See if we already have this site listed
		 */
		if (is_in_smd_list(host_num))
		{
			/*
			 * Relace the current entry
			 */
			replace_smd_entry(host_num, a);

			/*
			 * Tell player what they've set
			 */
			if (mask == NULL)
				notify_colour(player, player, COLOUR_MESSAGES, "Replacing existing entry for %s with a mask of 0x%08x", host, DEFAULT_MASK);
			else
				notify_colour(player, player, COLOUR_MESSAGES, "Replacing existing entry for %s with a mask of %s", host, mask);
		}
		else
		{
			/*
			 * Add to the start of the existing SMD list
			 */
			a->next = smd_list;
			smd_list = a;

			/*
			 * Tell player what they've set
			 */
			if (mask == NULL)
				notify_colour(player, player, COLOUR_MESSAGES, "Banning %s with a mask of 0x%08x", host, DEFAULT_MASK);
			else
				notify_colour(player, player, COLOUR_MESSAGES, "Banning %s with a mask of %s", host, mask);
		}
	}

	/*
	 * Remove an extry from the (run-time) SMD list
	 */
	if ((strcasecmp(arg1, "remove")==0) || (strcasecmp(arg1, "unban")==0))
	{
		matched = True;

		if (blank(arg2))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Usage: @smd unban=aaa.bbb.ccc.ddd");
			RETURN_FAIL;
		}

		if (remove_smd_entry(addr_nametoint(arg2, 1)))
			notify_colour(player, player, COLOUR_MESSAGES, "Ban lifted for %s", arg2);
		else
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Ban not found for %s", arg2);
	}

	/*
	 * Like @smdread _except_ it doesn't re-read the smd file
	 * (we may have done a temporary ban, which would get lost with a re-read of the file)
	 */
	if(strcasecmp(arg1, "check")==0)
	{
		matched = True;

		notify_colour(player, player, COLOUR_MESSAGES, "Checking current SMD list...");
		smd_updated();
		notify_colour(player, player, COLOUR_MESSAGES, "SMD Check complete.");
	}

	if (matched)
	{
		RETURN_SUCC;
	}
	else
	{
                notify(player, "%sUsage:%s  @smd <function> = <args>",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
                notify(player, "        @smd check");                                                          
                notify(player, "        @smd read");
                notify(player, "        @smd list  = <flag>");
                notify(player, "        @smd ban   = <host>[/<mask>]");
		notify(player, "        @smd unban = <host>");
                notify(player, "%sType \"%shelp @smd%s\" for more information.%s", ca[COLOUR_MESSAGES],  ca[COLOUR_COMMANDS], ca[COLOUR_MESSAGES], COLOUR_REVERT );

		RETURN_FAIL;
	}
}

/* Check to see if a specified address is banned */
int
is_banned ( u_long  addr )
{
	return (get_smd_flags(addr) & BANNED)!=0;
}

/* Check to see if a specified address should be looked up in the name server */
int
smd_dnslookup( u_long addr )
{
	return (get_smd_flags(addr) & DNS)!=0;
}

/* Check to see if a specified address should be allowed to create chars */
int
smd_cantcreate( u_long addr )
{
	return (get_smd_flags(addr) & NOCREATE)!=0;
}


int
smd_cantuseguests( u_long addr )
{
	return (get_smd_flags(addr) & NOGUESTS)!=0;
}

