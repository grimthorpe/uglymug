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
	Boolean	from_a_list;
	Boolean	included;
	struct	player_list_entry *next;
	struct	player_list_entry *prev;
} PLE;

class	Player_list
{
	private:
		Boolean	_include_unconnected;
		String	listnames[MAX_LIST_TELLS];
		int	listcount;
		int	count;
		int	filtered_size;
		int	originator;
		PLE	*current;
		PLE	*list;
		void	set_included(PLE *player, Boolean state, const char *message=NULL);
	public:
		Player_list(int a) :	_include_unconnected(False), listcount(0), count (0), filtered_size (0), originator (a), current (NULL), list (NULL) {}
		~Player_list();
		void		include_unconnected()	{ _include_unconnected= True; }
		void		add_list(const CString&);
		void		eat_keiths_chips();
		int		get_realsize()		{ return count;}
		int		get_filtered_size()	{ return filtered_size;}
		int		get_first();
		int		get_next();
		Boolean		add_player(int	p, Boolean fromlist=False);
		PLE		*find_player(int p);
		PLE		*get_list() { return list; }
		int		build_from_text(int player, const CString& text);
		const char	*generate_courtesy_string(int source, int dest, Boolean myself=False);
		void		notify(int player, const char *prefix, const char *suffix, const char *string);
		void		beep();

		int		include_if_unset(const int f);
		int		include_if_set(const int f);
		int		include(int who);
		int		include_from_list(int player, int flag);
		int		exclude_from_list(int player, int flag, const char *message=NULL);
		int		include_from_reverse_list(int player, int flag);
		int		exclude_from_reverse_list(int player, int flag, const char *message=NULL);
		int		filter_out_if_unset(const int f, const char *message = NULL);
		int		filter_out_if_set(const int f, const char *message = NULL);
		int		exclude(int who, const char *message=NULL);

		void		warn_me_if_idle();
		void		trigger_command(const char *, context &c);

};

extern int find_list_dictionary(int player, const char *which);
extern const char *reverseplist_dictionary;
extern const char *reverseclist_dictionary;
extern const char *list_dictionary;
extern const char *playerlist_dictionary;
