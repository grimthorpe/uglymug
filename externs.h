#include <sys/time.h>
#include "copyright.h"

#include "debug.h"

#ifndef	_DH_H
#include "db.h"
#endif	/* _DB_H */

#ifndef	_CONTEXT_H
#include "context.h"
#endif	/* _CONTEXT_H */

#ifndef MIN
#define			MIN(a,b)		((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define			MAX(a,b)		((a)>(b)?(a):(b))
#endif
#define			PLURAL(i)		(((i)==1)?(""):("s"))

struct flag_stuff
{
	const	char	*string;
	int		flag;
	char		quick_flag;
};

struct list_flags
{
	const	char	*string;
	int		flag;
};

extern	struct	flag_stuff	flag_list [];
extern	struct	list_flags	playerlist_flag_list [];
extern	const   char		*fit_errlist [];

/** External stuff, which shouldn't really be in here **/
/* Added the ifndef cos noone's got a linux box as broken as wyrm is - Abs */
/* #ifndef linux JPK */
#if !defined linux && !defined __FreeBSD__
extern "C"
{
//extern	unsigned long	inet_addr		(const char *cp);
extern	int		strcasecmp		(const char *s1, const char *s2);
//extern	int		strncasecmp		(const char *s1, const char *s2, int n);
#ifdef sun
/*  extern char     *tgetstr(char *, char **), */
/* extern  const char	*tgetstr	(const char *id, char **area); */
/* extern  int   tgetent(char *, char *), tgetflag(char *), tgetnum(char *), */
/* extern  char  *tparm(), *tgoto(), *tgetstr(), *tigetstr(), */
/* extern  int   tgetent(), tgetflag(), tgetnum(), */

#else
extern	char	*tgetstr	(const char *id, char **area);
extern	int	tgetent		(char *bp, const char *name);
extern	int	tgetnum		(const char *id);
#endif
};
#endif /* linux */

/* From alarm.c */
extern	void		db_patch_alarms		();
extern	void		insert_into_pending_queue (dbref command, int time);
extern	void		remove_from_pending_queue (dbref command);
extern	long		time_of_next_cron	(const char *cron_format);
extern	dbref		command_pending		(int time);

/* From colour.c */
extern		int	rank_colour		(dbref);
extern		cplay	*make_colour_play	(const dbref, const CString&);
extern	const	char*	player_colour		(dbref, dbref, int);
extern	const	int	find_number_of_players	(const CString&);

/* From create.c */
extern	dbref		parse_linkable_room	(dbref player, const CString& room_name);

/* From combat.c */
extern void remove_combat_stuff(dbref player);
extern void make_combat_stuff_ready(dbref player);

/*	from fuses.c */
extern	void		count_down_fuses	(context &c, dbref fuse, int tom_state);
#ifdef ABORT_FUSES
extern void		count_down_abort_fuses	(context &c, dbref fuse);
#endif

/* From get.c */
extern	const	char	*const format_alias_string	(dbref thing);
/* From game.c */
extern	void		mud_time_sync		();
extern	void		mud_command		(dbref player, const char *command);
extern	void		mud_run_dotcommand	(dbref player, const CString& command);
extern	void		mud_connect_player	(dbref player);
extern	void		mud_disconnect_player	(dbref player);
extern	void		mud_time_sync		();

/* From game_predicates.c */

enum	fit_result	{ SUCCESS=0, NO_MASS=1, NO_VOLUME=2, NOT_CONTAINER=3, NO_REASON=4 };

extern	bool		is_flag_allowed		(typeof_type type, flag_type flag);
extern	const	bool	could_doit		(const context &c, const dbref thing);
extern	const	bool	can_doit		(const context &c, const dbref thing, const char *default_fail_msg);
extern	bool		can_see			(context &c, dbref thing, bool can_see_location);
extern	int		payfor			(dbref who, int cost);
extern	bool		ok_name			(const CString& name);
extern	bool		ok_puppet_name		(const CString& puppet_name);
extern	bool		ok_player_name		(const CString& player_name);
extern	int		ok_password		(const CString& password);
#ifdef ALIASES
extern	int		ok_alias_string		(dbref player,const CString& alias);
#endif /* ALIASES */
extern	int		ok_who_string		(dbref player,const CString& string);
extern	int		can_reach		(dbref who, dbref what);
extern	int		allowable_command	(dbref who, char *command_string);
extern	enum fit_result	will_fit		(dbref victim, dbref destination);
extern	double		find_volume_of_contents	(dbref thing);
extern	double		find_mass_of_contents_except	(dbref thing, dbref exception);
extern	double		find_volume_of_object	(dbref thing);
extern	double		find_mass_of_object	(dbref thing);
extern	double		find_volume_of_object	(dbref thing);
extern	double		find_volume_limit_of_object (dbref thing);
extern	double		find_mass_limit_of_object (dbref thing);
extern	bool		abortable_command	(const char *command);
extern	bool		is_guest		(dbref player);
/* From interface.c */
extern	void		panic			(const char *);
extern	int		peak_users;

/* From log.c */
extern	void		log_recursion		(dbref player, dbref command, char* arg1, char* ar2);

/* From look.c */
extern	void		look_room		(context &, dbref room);
extern	const char	*flag_description	(dbref thing);
extern	char		*time_string		(time_t interval);
extern	char		*small_time_string	(time_t interval);

/* From move.c */
extern	bool		can_move		(context &c, const CString& direction);
extern	bool		moveto			(dbref what, dbref where);

/* From player.c */
extern	dbref		lookup_player		(dbref player, const CString& name);

/* From predicates.c */
extern	object_flag_type type_to_flag_map	[];
extern	const	bool	controls_for_read	(const dbref real_who, const dbref what, const dbref effective_who);
extern	const	bool	can_link_to		(const context &c, const dbref where);
extern	const	bool	can_link		(const context &c, const dbref exit);

/* From speech.c */
extern	void		notify_except		(dbref first, dbref originator, dbref exception, const char *msg);
extern	void		notify_except2		(dbref first, dbref originator, dbref exc1, dbref exc2, const char *msg);
extern	int		blank			(const char *s);
extern	const	char	*reconstruct_message	(const CString& arg1, const CString& arg2);

/* From stringutil.c */
extern	bool		semicolon_string_match	(const CString&, const CString&);
extern	void		init_strings		();
extern	void		pronoun_substitute	(char *result, unsigned int buffer_length, dbref player, const CString& str);
extern	int		string_compare		(const CString& s1, const CString& s2);
extern	int		string_prefix		(const CString& string, const CString& prefix);
extern	const	char	*string_match		(const CString& src, const CString& sub);
extern  const	char	*censor			(const CString& string);
extern	int		colour_strlen		(const CString& string);
extern	bool		add_rude		(const CString&);
extern	bool		add_excluded		(const CString&);
extern	bool		un_rude			(const CString&);
extern	bool		un_exclude		(const CString&);
extern	char		**rude_words;
extern	char		**excluded_words;
extern	int		rudes;
extern	int		excluded;
/* From utils.c */
extern	bool		contains		(dbref thing, dbref container);
extern	bool		contains_inherited	(dbref thing, dbref container);
extern  const   char    *getarticle		(int article_base, dbref thing);
extern	const	char	*getname		(dbref thing);
extern	const	char	*getname_inherited	(dbref thing);
extern	bool		in_area			(dbref thing, dbref area);
extern	bool		member			(dbref thing, dbref list);
extern	dbref		remove_first		(dbref first, dbref what);
extern	dbref		reverse			(dbref list);

/* From boolexp.c */
extern	struct	boolexp	*parse_boolexp		(const dbref player, const CString& buf);

/* From unparse.c */
extern	const	char	*const	unparse_objectandarticle		(const context &c, const dbref object, int articletype);
extern	const	char	*const	unparse_objectandarticle_inherited	(const context &c, const dbref object, int articletype);
extern	const	char	*const	unparse_object				(const context &c, const dbref object);
extern	const	char	*const	unparse_object_inherited		(const context &c, const dbref object);
extern	const	char	*const	unparse_for_return			(const context &c, const dbref object);
extern	const	char	*const	unparse_for_return_inherited		(const context &c, const dbref object);

/* From channel.c */
class Channel;
extern	void		channel_disconnect	(dbref, bool just_leave = false);

/* From smd.c */
extern	int		is_banned		(unsigned long a);
extern	int		smd_dnslookup		(unsigned long a);
extern	int		smd_cantcreate		(unsigned long addr );
extern	int		smd_cantuseguests	(unsigned long addr);

/* From netmud.c */
extern	int		dump_interval;
extern	const	char	*version;
extern	const	char	*release;

