#define HACKY_INSERT_PLAYERNAME		'!'
#define HACKY_INSERT_HAVENMESSAGE	'#'
#define HACKY_INSERT_SLEEPINGMESSAGE	'*'

#define	ADMIN_SPECIFIER			"admin"
#define FRIENDS_SPECIFIER		"friends"
#define MAX_LIST_TELLS			(10)

#define PLIST_IGNORE	0x01
#define	PLIST_INFORM	0x02
#define PLIST_FRIEND	0x04
#define	PLIST_FBLOCK	0x08

#define MAX_LIST_SIZE	64

typedef struct	player_list_entry
{
	int	player;
	bool	from_a_list;
	bool	included;
	struct	player_list_entry *next;
	struct	player_list_entry *prev;
} PLE;

class	Player_list
{
	private:
	Player_list(const Player_list&); // DUMMY
	Player_list& operator=(const Player_list&); // DUMMY

		bool	_include_unconnected;
		String	listnames[MAX_LIST_TELLS];
		int	listcount;
		int	count;
		int	filtered_size;
		int	originator;
		PLE	*current;
		PLE	*list;
		void	set_included(PLE *player, bool state, const char *message=NULL);
	public:
		Player_list(dbref a) :	_include_unconnected(false), listcount(0), count (0), filtered_size (0), originator (a), current (NULL), list (NULL) {}
		~Player_list();
		void		include_unconnected()	{ _include_unconnected= true; }
		void		add_list(const String&);
		void		eat_keiths_chips();
		int		get_realsize()		{ return count;}
		int		get_filtered_size()	{ return filtered_size;}
		dbref		get_first();
		dbref		get_next();
		bool		add_player(dbref	p, bool fromlist=false);
		PLE		*find_player(dbref p);
		PLE		*get_list() { return list; }
		int		build_from_text(dbref player, const String& text);
		const char	*generate_courtesy_string(dbref source, dbref dest, bool myself=false);
		void		notify(dbref player, const char *prefix, const char *suffix, const char *string);
		void		beep();

		int		include_if_unset(const int f);
		int		include_if_set(const int f);
		int		include(int who);
		int		include_from_list(dbref player, int flag);
		int		exclude_from_list(dbref player, int flag, const char *message=NULL);
		int		include_from_reverse_list(dbref player, int flag);
		int		exclude_from_reverse_list(dbref player, int flag, const char *message=NULL);
		int		filter_out_if_unset(const int f, const char *message = NULL);
		int		filter_out_if_set(const int f, const char *message = NULL);
		int		exclude(dbref who, const char *message=NULL);

		void		warn_me_if_idle();
		void		trigger_command(const char *, context &c);

};

extern dbref find_list_dictionary(dbref player, const char *which);
extern const char *reverseplist_dictionary;
extern const char *reverseclist_dictionary;
extern const char *list_dictionary;
extern const char *playerlist_dictionary;
