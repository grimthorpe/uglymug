static char SCCSid[] = "@(#)stringutil.c	1.13\t3/20/95";
#include "copyright.h"

/* String utilities */

#include <ctype.h>
#include "interface.h"
#include "externs.h"
#include "colour.h"

//#include <stdio.h>
//#include <string.h>

char	downcase_table[128];
char	downcase_compare[128*128];
char	rude_letters[128];
char	excluded_letters[128];
char	**rude_words=NULL;
char	**excluded_words=NULL;
int	rudes=0;
int	excluded=0;

#define DOWNCASE(x) (downcase_table[x])
#define DOWNCOMP(x,y) (downcase_compare[(x)+((y)*128)])
#define REALLOC(p,s)	{ ((p)) ? ((p) = (char **)realloc((p),(s))) : ((p) = (char **) malloc((s)));}

/*Values for use in indexing the substitution tables*/
#define UNASSIGNED	0
#define NEUTER		1
#define FEMALE		2
#define MALE		3

#define ConvertGender(x)	(Male((x))?MALE : Neuter((x))?NEUTER : Female((x))?FEMALE : UNASSIGNED)


int colour_strlen(const char *string)
{
	int size=0;
	const char *p=string;
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
	int	x;
	int	y;

	for (x = 0; x < 128; x++)
	{
		rude_letters[x]=0;
		if (isupper (x))
			downcase_table[x] = tolower (x);
		else
			downcase_table[x] = x;
	}
	for (x = 0; x < 128; x ++)
		for (y = 0; y < 128; y ++)
			downcase_compare[x+y*128] = (DOWNCASE(x) == DOWNCASE(y));

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

Boolean
semicolon_string_match (
const	char	*in_this,
const	char	*is_this)

{
	const	char	*p = is_this;
	const	char	*m = in_this;

	/* Check for both NULL or both the same string */
	if (in_this == is_this)
		return (True);

	/* Check for one NULL */
	if (in_this == NULL)
		return (False);
	if (is_this == NULL)
		return (True);

	while(*m)
	{
		/* check out this one */
		
		while(*m && *p && (DOWNCASE(*p) == DOWNCASE(*m)))
			p++, m++;

		if((*m == '\0' && *p == '\0') || (*m == EXIT_DELIMITER && *p == '\0'))
		{
			/* we got it */
			return(True);
		}

		/* we didn't get it, find next match */
		while(*m && (*m++ != EXIT_DELIMITER))
			;
			
		while (isspace (*m))
			m++;
		p = is_this;
	}

	return(False);
}


int
string_compare (
const	char	*s1,
const	char	*s2)

{
	/* Check for both NULL or both the same string */
	if (s1 == s2)
		return (0);

	/* Check for one NULL */
	if (s1 == NULL)
		return (-1);
	if (s2 == NULL)
		return (1);

	/* Both non-NULL; find common prefix, if any */
	while(*s1 && *s2 && DOWNCOMP(*s1,*s2))
		s1++, s2++;

	return(DOWNCASE(*s1) - DOWNCASE(*s2));
}


int
string_prefix (
const	char	*string,
const	char	*prefix)

{
	/* If prefix is NULL, any string matches */
	if (prefix == NULL)
		return (1);

	/* If prefix is non-NULL and string is NULL, fail. */
	if (string == NULL)
		return (0);

	/* Otherwise, we have to think about it */
	while (*string && *prefix && DOWNCOMP(*string,*prefix))
		string++, prefix++;

	return *prefix == '\0';
}


/*
 * string_match: accepts only nonempty matches starting at the beginning of a word
 */

const char *
string_match (
const	char	*src,
const	char	*sub)

{
	/* If substring is NULL, automatic match */
	if (sub == NULL)
		return (src);

	/* If substring is non-NULL and source is NULL, fail. */
	if (src == NULL)
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
int		buffer_length,
dbref		player,
const	char	*str)

{
	char c;
	static const char *subjective[4] = { "", "it", "she", "he" };
	static const char *possessive[4] = { "", "its", "her", "his" };
	static const char *objective[4] = { "", "it", "her", "him" };

	strcpy(result, db[player].get_name());
	buffer_length -= strlen(result);
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
						if (buffer_length > strlen(db[player].get_name())) strcat(result, db[player].get_name());
						else buffer_length = -1;
						break;
					case 'p':
					case 'P':
						if (buffer_length > strlen(db[player].get_name())) 
						{
							strcat(result, db[player].get_name());
							strcat(result, "'s");
						}
						else buffer_length = -1;
						break;
					default:
						*result++ = *str;
						break;
				}
				str++;
				buffer_length -= strlen(result);
				result += strlen(result);
			}
			else
			{
				switch (c)
				{
					case 's':
					case 'S':
						if (buffer_length > strlen(subjective[ConvertGender(player)])) strcat(result, subjective[ConvertGender(player)]);
						else buffer_length = -1;
						break;
					case 'p':
					case 'P':
						if (buffer_length > strlen(possessive[ConvertGender(player)])) strcat(result, possessive[ConvertGender(player)]);
						else buffer_length = -1;
						break;
					case 'o':
					case 'O':
						if (buffer_length > strlen(objective[ConvertGender(player)])) strcat(result, objective[ConvertGender(player)]);
						else buffer_length = -1;
						break;
					case 'n':
					case 'N':
						if (buffer_length > strlen(db[player].get_name())) strcat(result, db[player].get_name());
						else buffer_length = -1;
						break;
					default:
						*result = *str;
						result[1] = '\0';
						break;
				} 
				if(isupper(c) && islower(*result))
					*result = toupper(*result);
				buffer_length -= strlen(result);
				result += strlen(result);
				str++;
			}
		}
		else
			*result++ = *str++;
	}
	if (buffer_length <= 0)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Buffer overflow doing a pronoun substitution.");
	}
	else *result = '\0';
} 



int
swear_compare(int *skipped, char *s1, char *s2)
{
        /* This could be a rude word.. Is it? */
        /* We must disregard all non-alphanumerics, etc in s1*/
        
        while (*s1 && *s2)
        {
            if (ispunct(*s1))
                s1++, (*skipped)++;
            else if (DOWNCOMP(*s1, *s2) == 0)
                return (DOWNCASE(*s1) - DOWNCASE(*s2));
            else
                s1++, s2++, (*skipped)++;
        }
        // Have we reached the end of the main string but not got a rude word?
        if ((*s2 != 0) && (*s1 == 0))
                *skipped=0;
        return 0;
}

Boolean
add_rude(
const char *string)
{
	int		value;
	int		i;

	if (!(string && *string))
		return False;

	if (rude_words==NULL)
	{
		rude_words=(char **)malloc(sizeof(char *));
		rude_words[0]= strdup(string);
		rudes=1;
		rude_letters[tolower(*string)]=1;
		return True;
	}
	i=0;
	while ((i < rudes) && (strcasecmp(string, rude_words[i]) > 0))
		i++;
	if ((i < rudes) && (!strcasecmp(string, rude_words[i])))
		return True;  // Word already in list

	value=rudes++;
	REALLOC(rude_words, (rudes*sizeof(char *)));

	// i tells us where to put the new word 

	while (value > i)
		rude_words[value]=rude_words[--value];


	rude_words[i]=strdup(string);
	rude_letters[tolower(*string)]++;
	return True;
}


Boolean
add_excluded(
const char *string)
{
	int		value;
	int		i;

	if (!(string && *string))
		return False;

	if (excluded_words==NULL)
	{
		excluded_words=(char **)malloc(sizeof(char *));
		excluded_words[0]= strdup(string);
		excluded=1;
		excluded_letters[tolower(*string)]=1;
		return True;
	}
	i=0;

	while ((i < excluded) && (strcasecmp(string, excluded_words[i]) > 0))
		i++;
	if ((i < excluded) && (!strcasecmp(string, excluded_words[i])))
		return True;  // Word already in list

	value=excluded++;
	REALLOC(excluded_words, (excluded*sizeof(char *)));

	// i tells us where to put the new word 

	while (value > i)
		excluded_words[value]=excluded_words[--value];


	excluded_words[i]=strdup(string);
	excluded_letters[tolower(*string)]++;
	return True;
}

Boolean
un_rude(
const char *string)
{
	int		value;
	int		i;

	if (!(string && *string))
		return True;
	if (rude_words==NULL)
		return True;
	i=0;
	while ((i < rudes) && (strcasecmp(string, rude_words[i]) > 0))
		i++;
	if (i == rudes)
		return True;  // Word not censored
	rude_letters[tolower(*string)]--;
	while (i < rudes)
		rude_words[i]=rude_words[++i];
	REALLOC(rude_words, (--rudes*sizeof(char *)));
	if (rudes==0)
		rude_words= NULL;
	return True;
}

Boolean
un_exclude(
const char *string)
{
	int		value;
	int		i;

	if (!(string && *string))
		return True;
	if (excluded_words==NULL)
		return True;
	i=0;
	while ((i < excluded) && (strcasecmp(string, excluded_words[i]) > 0))
		i++;
	if (i == excluded)
		return True;  // Word not censored
	excluded_letters[tolower(*string)]--;
	while (i < excluded)
		excluded_words[i]=excluded_words[++i];
	REALLOC(excluded_words, (--excluded*sizeof(char *)));
	if (excluded==0)
		excluded_words=NULL;
	return True;
}

int
rude_count (
char *string)
{
        int             bottom = 0;
        int             top;
        int             mid;
        int             value;
        int             counter;
        int             i;

        /* Check for NULL */
        if (string == NULL)
                return (1);

        if (*string == 0)
                return 1;

/* First, check if this is a word that shouldn't be censored - EG 'scunthorpe' */

	if (excluded_letters[tolower(*string)])
	{
		top= excluded-1;
		while (bottom <= top)
		{
			mid = (top + bottom) / 2;
			counter=0;
			value = strncasecmp(string, excluded_words [mid], strlen(excluded_words[mid]));

			if (value < 0)
				top = mid - 1;
			else if (value == 0)
				return (strlen(excluded_words[mid]));
			else /* value > 0 */
				bottom = mid + 1;
                }
	}
	// Not an excluded word - so is it a naughty?

        if (!(rude_letters[tolower(*string)])) // Speeds up checking
                return 1;

        top = rudes-1;
        while (bottom <= top)
        {
                mid = (top + bottom) / 2;
                counter=0;
                value = swear_compare (&counter, string, rude_words [mid]);

                if (value < 0)
                        top = mid - 1;
                else
                {
                    if (value == 0)
                    {
                        if (counter>0)
                        {
                            for(i=0; i < counter; i++)
                            {
                                if (!ispunct(*string))
                                     *string='*';
                                string++;
                            }
                        }
                        return counter+1;
                    }
                    else /* value > 0 */
                        bottom = mid + 1;
                }
        }
        /* We get here if the word's not rude */
        return (1);
}

const char *censor(const char *string)
{
	/* This is horribly inefficient. Infact, it can't get more inefficient.
	   Some sort of Boyer-Moore algorithm might be better, but that is
	   intended for matching single words. */

    static char censored[MAX_OUTPUT];
    char *letter=censored;
    char t;

    strcpy(censored, string);
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


