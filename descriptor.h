class descriptor
{
//	private:
	public:
		
		int			descriptor;
		enum connect_states	connect_state;
		dbref			player;
		char			*player_name;	/* Used at connect time - not necessarily correct outside check_connect */
		char			*password;	/* Used at connect time - not necessarily correct outside check_connect */
		int			connect_attempts;
		char			*output_prefix;
		char			*output_suffix;
		int			output_size;
		struct	text_queue	output;
		struct	text_queue	input;
		unsigned char		*raw_input;
		unsigned char		*raw_input_at;
		long			start_time;
		long			last_time;
		int			warning_level;
		int			quota;
		int			backslash_pending;
		char			hostname [MAXHOSTNAMELEN];
		struct	sockaddr_in	address;
		struct	descriptor_data	*next;
		struct	descriptor_data	**prev;
		int                     terminal_width;
		int			terminal_height;
		char			*terminal_type;
		struct		{
					char *bold_on;
					char *bold_off;
				}	termcap;
		int			terminal_xpos;
		int			terminal_lftocr;
		int			terminal_nobell;
		int			channel;

//	public:
					Descriptor();
					~Descriptor();
};
