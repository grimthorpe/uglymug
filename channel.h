#include <time.h>

#ifndef CHANNEL_H_INCLUDED
#define CHANNEL_H_INCLUDED

#define	CHANNEL_MAGIC_COOKIE	'#'	/* The character that identifies
					   a string as being a channel name */

enum channel_player_flags
{
	CHANNEL_PLAYER_NORMAL,
	CHANNEL_PLAYER_OPERATOR
};


struct	channel_player
{
	dbref		player;
	time_t		issued;
	enum channel_player_flags flags;

	struct channel_player *next, *prev;
};

enum channel_modes
{
	CHANNEL_PUBLIC,
	CHANNEL_PRIVATE
};

struct channel
{
	char			*name;
	enum	channel_modes	mode;
	struct channel_player	*players, *invites, *bans;

	struct channel		*next, *prev;
};

#endif /* CHANNEL_H_INCLUDED */

