/* static char SCCSid[] = "@(#)stringutil.c	1.13\t3/20/95"; */
#include "copyright.h"
#include "os.h"

/* String utilities */

#include <ctype.h>
#include "interface.h"
#include "externs.h"
#include "colour.h"

#include <set>

//#include <stdio.h>
//#include <string.h>

std::set<String>	rude_words;
std::set<String>	excluded_words;

#define REALLOC(p,s)	{ ((p)) ? ((p) = (char **)realloc((p),(s))) : ((p) = (char **) malloc((s)));}

/*Values for use in indexing the substitution tables*/
#define UNASSIGNED	0
#define NEUTER		1
#define FEMALE		2
#define MALE		3

#define ConvertGender(x)	(Male((x))?MALE : Neuter((x))?NEUTER : Female((x))?FEMALE : UNASSIGNED)


int colour_strlen(const String& string)
{
	int size=0;
	const char *p=string.c_str();
	int percent_primed=0;
	while (p && *p)
	{
		switch (*p)
		{
			case '%':
				if (percent_primed)
				{
					size++;
					percent_primed=0;
				}
				else
					percent_primed=1;
				break;
			case '\033': // Ansi
				while (*p && *p!='m')
					p++;
				break;
			default:
				if (!percent_primed)
					size++;
				else
					percent_primed=0;
		}
		if (*p)
			p++;
	}
	return size;
}

void init_strings (void)
{
/* Now some reasonable default exclusions.
   I had to ask someone what these words mean because I'm so innocent. - Luggs */

	add_rude("arse");
	add_rude("bastard");
	add_rude("bollocks");
	add_rude("cunt");
	add_rude("cvnt");
	add_rude("dick");
	add_rude("d1ck");
	add_rude("fuck");
	add_rude("wank");
	add_rude("w@nk");
	add_rude("bitch");
	add_rude("b1tch");

//	add_excluded("arsenal");	// Hm. I'm a Spurs fan. Do I stop Ugly printing '****nal' or not? :-)
	add_excluded("scunthorpe");	// JC Digita: The man who put the **** into scunthorpe.
}

bool
semicolon_string_match (
const	String& in_this,
const	String& is_this)

{
	const	char	*p = is_this.c_str();
	const	char	*m = in_this.c_str();

	/* Check for both NULL or both the same string */
	if (in_this.c_str() == is_this.c_str())
		return (true);

	/* Check for one NULL */
	if((!in_this) || (!is_this))
		return (false);

	while(*m)
	{
		/* check out this one */
		
		while(*m && *p && (tolower(*p) == tolower(*m)))
			p++, m++;

		if((*m == '\0' && *p == '\0') || (*m == EXIT_DELIMITER && *p == '\0'))
		{
			/* we got it */
			return(true);
		}

		/* we didn't get it, find next match */
		while(*m && (*m++ != EXIT_DELIMITER))
			;
			
		while (isspace (*m))
			m++;
		p = is_this.c_str();
	}

	return(false);
}

// JPK *FIXME*
//int string_substr_match(const   String& s1, const   String& s2)
//{
//strstr()
//}
/* Oh yeah, I know, we'll comment some code...                */
/* string_compare does a case insensetive comparison between  */
/* strings s1 and s2                                          */
/*   returns 0  if the strings are the same                   */
/*   returns -1 if s1 is null, but s2 isn't                   */
/*   returns 1  if s2 is null, but s1 isn't                   */
/*   returns the numerical difference between the lowercase   */
/*     values of the first letter that doesn't match(!)       */
int
string_compare (
const	String& s1,
const	String& s2)

{
	/* Check for both NULL or both the same string */
	if(!s1 && !s2)
		return (0);

	if (s1.c_str() == s2.c_str())
		return (0);

	/* Check for one NULL */
	if (!s1)
		return (-1);
	if (!s2)
		return (1);

	/* Both non-NULL; find common prefix, if any */
	const char* c1 = s1.c_str();
	const char* c2 = s2.c_str();
	while(*c1 && *c2 && (tolower(*c1) == tolower(*c2)))
		c1++, c2++;

	return(tolower(*c1) - tolower(*c2));
}


int
string_prefix (
const	String& string,
const	String& prefix)

{
	/* If prefix is NULL, any string matches */
	if (!prefix)
		return (1);

	/* If prefix is non-NULL and string is NULL, fail. */
	if (!string)
		return (0);

	const char* s = string.c_str();
	const char* p = prefix.c_str();

	/* Otherwise, we have to think about it */
	while (*s && *p && (tolower(*s) == tolower(*p)))
		s++, p++;

	return *p== '\0';
}


/*
 * string_match: accepts only nonempty matches starting at the beginning of a word
 */

const char *
string_match (
const	String& csrc,
const	String& csub)

{
const char* sub = csub.c_str();
const char* src = csrc.c_str();
	/* If substring is NULL, automatic match */
	if (!csub)
		return (src);

	/* If substring is non-NULL and source is NULL, fail. */
	if (!csrc)
		return (NULL);

	/* Otherwise, we have to hunt for it */
	if(*sub != '\0')
	{
		while(*src)
		{
			if(string_prefix(src, sub)) return src;
			/* else scan to beginning of next word */
			while(*src && (isalnum(*src) || (*src == '@'))) src++;
			while(*src && !(isalnum(*src) || (*src == '@'))) src++;
		}
	}

	return (NULL);
}


/*
 * pronoun_substitute()
 *
 * %-type substitutions for pronouns
 *
 * %s/%S for subjective pronouns (he/she/it, He/She/It)
 * %o/%O for objective pronouns (him/her/it, Him/Her/It)
 * %p/%P for possessive pronouns (his/her/its, His/Her/Its)
 * %n    for the player's name.
 */

void
pronoun_substitute (
char		*result,
unsigned int	buffer_length,
dbref		player,
const	String& cstr)

{
	char c;
	const char* str = cstr.c_str();
	static const char *subjective[4] = { "", "it", "she", "he" };
	static const char *possessive[4] = { "", "its", "her", "his" };
	static const char *objective[4] = { "", "it", "her", "him" };

	strcpy(result, db[player].get_name().c_str());
	if (buffer_length > strlen(result))
	{
		buffer_length -= strlen(result);
	}
  else
	{
		buffer_length = 0;
	}
	result += strlen(result);
	*result++ = ' ';
	while (*str && buffer_length > 0)
	{
		if(*str == '%')
		{
			*result = '\0';
			c = *(++str);
			if (Unassigned(player))
			{
				switch(c)
				{
					case 'n':
					case 'N':
					case 'o':
					case 'O':
					case 's':
					case 'S':
						if (buffer_length > db[player].get_name().length())
						{
							strcat(result, db[player].get_name().c_str());
						}
						else
						{
							buffer_length = 0;
						}
						break;
					case 'p':
					case 'P':
						if (buffer_length > (db[player].get_name().length()+2)) 
						{
							strcat(result, db[player].get_name().c_str());
							strcat(result, "'s");
						}
						else
						{
							buffer_length = 0;
						}
						break;
					default:
						*result++ = *str;
						break;
				}
				str++;
/* We know this is safe, since we tested buffer_length > strlen(result)
 * at each stage JPK
 */
				buffer_length -= strlen(result);
				result += strlen(result);
			}
			else
			{
				switch (c)
				{
					case 's':
					case 'S':
						if (buffer_length > strlen(subjective[ConvertGender(player)]))
						{
							strcat(result, subjective[ConvertGender(player)]);
						}
						else
						{
							buffer_length = 0;
						}
						break;
					case 'p':
					case 'P':
						if (buffer_length > strlen(possessive[ConvertGender(player)]))
						{
							strcat(result, possessive[ConvertGender(player)]);
						}
						else
						{
							buffer_length = 0;
						}
						break;
					case 'o':
					case 'O':
						if (buffer_length > strlen(objective[ConvertGender(player)]))
						{
							strcat(result, objective[ConvertGender(player)]);
						}
						else
						{
							buffer_length = 0;
						}
						break;
					case 'n':
					case 'N':
						if (buffer_length > db[player].get_name().length())
						{
							strcat(result, db[player].get_name().c_str());
						}
						else
						{
							buffer_length = 0;
						}
						break;
					default:
						*result = *str;
						result[1] = '\0';
						break;
				} 
				if(isupper(c) && islower(*result))
				{
					*result = toupper(*result);
				}
				buffer_length -= strlen(result);
				result += strlen(result);
				str++;
			}
		}
		else
			*result++ = *str++;
	}
	if (buffer_length == 0)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Buffer overflow doing a pronoun substitution.");
	}
	else *result = '\0';
} 

static int
swear_compare(int &skipped, const char *s1, const char *s2)
{
        /* This could be a rude word.. Is it? */
        /* We must disregard all non-alphanumerics, etc in s1*/
        
        while (*s1 && *s2)
        {
		if('%' == *s1)
		{
			s1++, skipped++;
			if(*s1)
				s1++, skipped++;
		}
		if (ispunct(*s1))
			s1++, skipped++;
		else if (tolower(*s1) != tolower(*s2))
			return (tolower(*s1) - tolower(*s2));
		else
			s1++, s2++, skipped++;
        }
        // Have we reached the end of the main string but not got a rude word?
        if ((*s2 != 0) && (*s1 == 0))
                skipped=0;
        return 0;
}

bool
add_rude(
const String& string)
{
	excluded_words.erase(string);
	rude_words.insert(string);
	return true;
}

bool
add_excluded(
const String& string)
{
	rude_words.erase(string);
	excluded_words.insert(string);
	return true;
}

bool
un_rude(
const String& string)
{
	rude_words.erase(string);
	return true;
}

bool
un_exclude(
const String& string)
{
	excluded_words.erase(string);
	return true;
}

int
rude_count (
char *string)
{
        /* Check for NULL */
        if (string == NULL)
                return (1);

        if (*string == 0)
                return 1;

/* First, check if this is a word that shouldn't be censored - EG 'scunthorpe' */

	std::set<String>::const_iterator it;
	char c = string[0];
	for(it = excluded_words.begin(); it != excluded_words.end(); it++)
	{
		if((*it).c_str()[0] == c) // Speed up match.
		{
			// Do we have a prefix match? If so, we've got a winner.
			if(string_prefix(string, (*it)))
			{
				return (*it).length();
			}
		}
	}

	// Not an excluded word - so is it a naughty?

	for(it = rude_words.begin(); it != rude_words.end(); it++)
	{
		if((*it).c_str()[0] == c) // Speed up match.
		{
			int skipped = 0;
			if(swear_compare (skipped, string, (*it).c_str()) == 0)
			{
				if(skipped > 0)
				{
					for(int i=0; i < skipped; i++)
					{
						if(*string == '%')
							string++, i++;	// Skip over colour controls
						if (!ispunct(*string))
							*string='*';
						string++;
					}
					return skipped; // counter is exactly the number of characters to '*'
				}
				return 1;
			}
		}
	}

        /* We get here if the word's not rude */
        return (1);
}

const char *censor(const String& string)
{
	/* This is horribly inefficient. Infact, it can't get more inefficient.
	   Some sort of Boyer-Moore algorithm might be better, but that is
	   intended for matching single words. */

    static char censored[MAX_OUTPUT];
    char *letter=censored;
    char t;

    strcpy(censored, string.c_str());
    while ((t= *letter))
    {
        if (!isalpha(t))
        {
            letter++;
            continue;
        }

        letter+= rude_count(letter);
    }
    *letter='\0';
    return censored;
}
