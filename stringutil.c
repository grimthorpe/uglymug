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

	return string_compare(s1.c_str(), s2.c_str());
}

int
string_compare (
const	String& s1,
const	char* s2)

{
	return string_compare(s1.c_str(), s2);
}

int
string_compare (
const	char* s1,
const	String& s2)

{
	return string_compare(s1, s2.c_str());
}

int
string_compare (
const	char* s1,
const	char* s2)

{
	if(!s1 && !s2)
		return 0;
	if(!s1)
		return -1;
	if(!s2)
		return 1;

	/* Both non-NULL; find common prefix, if any */
	while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		s1++, s2++;

	return(tolower(*s1) - tolower(*s2));
}


int
string_prefix (
const	char* string,
const	char* prefix)
{
	/* If prefix is NULL, any string matches */
	if(!prefix)
		return 1;
	/* If prefix is non-NULL and string is NULL, fail. */
	if(!string)
		return 0;

	/* Otherwise, we have to think about it */
	while (*string && *prefix && (tolower(*string) == tolower(*prefix)))
		string++, prefix++;

	return *prefix== '\0';
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

	return string_prefix(string.c_str(), prefix.c_str());
}


/*
 * string_match: accepts only nonempty matches starting at the beginning of a word
 */

bool
string_match (
const	String& csrc,
const	String& csub)

{
	if(!csub)
		return true;
	if(!csrc)
		return false;
	return string_match(csrc.c_str(), csub.c_str());
}

bool
string_match (
const	char* src,
const	char* sub)

{
	/* If substring is NULL, automatic match */
	if (!sub)
		return true;

	/* If substring is non-NULL and source is NULL, fail. */
	if (!src)
		return false;

	/* Otherwise, we have to hunt for it */
	if(*sub != '\0')
	{
		while(*src)
		{
			if(string_prefix(src, sub)) return true;
			/* else scan to beginning of next word */
			while(*src && (isalnum(*src) || (*src == '@'))) src++;
			while(*src && !(isalnum(*src) || (*src == '@'))) src++;
		}
	}

	return false;
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
String&	result,
dbref	player,
const	String& cstr)
{
	char c;
	const char* str = cstr.c_str();
	static const String subjective_u[4] = { "", "It", "She", "He" };
	static const String possessive_u[4] = { "", "Its", "Her", "His" };
	static const String objective_u[4] = { "", "It", "Her", "Him" };
	static const String subjective_l[4] = { "", "it", "she", "he" };
	static const String possessive_l[4] = { "", "its", "her", "his" };
	static const String objective_l[4] = { "", "it", "her", "him" };

	result = db[player].get_name();
	result += ' ';
	while (*str)
	{
		if(*str == '%')
		{
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
						result += db[player].get_name();
						break;
					case 'p':
					case 'P':
						result += db[player].get_name();
						result += "'s";
					default:
						result += *str;
						break;
				}
				str++;
			}
			else
			{
				switch (c)
				{
					case 's':
						result += subjective_l[ConvertGender(player)];
						break;
					case 'S':
						result += subjective_u[ConvertGender(player)];
						break;
					case 'p':
						result += possessive_l[ConvertGender(player)];
						break;
					case 'P':
						result += possessive_u[ConvertGender(player)];
						break;
					case 'o':
						result += objective_l[ConvertGender(player)];
						break;
					case 'O':
						result += objective_u[ConvertGender(player)];
						break;
					case 'n':
					case 'N':
						result += db[player].get_name();
						break;
					default:
						result += *str;
						break;
				} 
				str++;
			}
		}
		else
			result += *str++;
	}
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
	String lowerstring;

	const char* s = string.c_str();
	while(*s)
	{
		lowerstring += (char)(tolower(*(s++)));
	}

	excluded_words.erase(lowerstring);
	rude_words.insert(lowerstring);
	return true;
}

bool
add_excluded(
const String& string)
{
	String lowerstring;

	const char* s = string.c_str();
	while(*s)
	{
		lowerstring += (char)(tolower(*(s++)));
	}

	rude_words.erase(lowerstring);
	excluded_words.insert(lowerstring);
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

static size_t
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
	char c = tolower(string[0]);
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
