#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include "copyright.h"

#include "db.h"

extern	 int		shutdown_flag;		/* if non-zero, interface should shut down */
extern	 dbref		shutdown_player;	/*The wizard shutting down*/
extern	 const	char	*shutdown_reason;	/*The reason for shutdown*/
extern	 time_t		game_start_time;	/*time game started*/

extern	 dbref	create_player		(const String& name, const String& password, bool effective_wizard = false);
extern	 dbref	connect_player		(const String& name, const String& password);
extern	 void	emergency_shutdown	(void);
extern	 void	boot_player		(dbref player, dbref booter);

extern	 int	init_game		(const char *infile, const char *outfile);
extern	 void	dump_database		(void);

extern	void	terminal_underline	(dbref player, const char *);
extern	void	notify_colour		(dbref player, dbref talker, int colour, const char *fmt, ...);
extern	void	notify_censor_colour	(dbref player, dbref talker, int colour, const char *fmt, ...);
extern	void	notify_public_colour	(dbref player, dbref talker, int colour, const char *fmt, ...);
extern	void	notify			(dbref player, const char *fmt, ...);
extern	void	notify_norecall		(dbref player, const char *fmt, ...);
extern	void	notify_norecall_conditional (String match,dbref player, const char *fmt, ...);
extern	void	notify_censor		(dbref player, dbref originator, const char *fmt, ...);

extern	void	notify_public		(dbref player, dbref originator, const char *fmt, ...);
extern	void	notify_all		(const char *fmt, ...);
extern	void	notify_listeners	(const char *fmt, ...);
extern	void	notify_area		(dbref loc, dbref originator, const char *fmt, ...);
extern	void	notify_wizard		(const char *fmt, ...);
extern	void	notify_wizard_natter	(const char *fmt, ...);
extern	void	notify_welcomer_natter	(const char *fmt, ...);

extern	void	beep			(dbref player);

extern	char	*boldify		(dbref player, const char *str);
extern	char	*underscorify		(dbref player, const char *str);

extern	int	connection_count	(dbref player);
extern	int	count_connect_types	(int);

extern	time_t	get_idle_time		(dbref player);
extern	dbref	match_connected_player	(const char * given);

extern	void	close_sockets		();
extern	void	set_signals		();
extern	void	mud_main_loop		(int argc, char** argv);

extern	void	smd_updated		();
