/*
 * cgitelnet - cgi gateway for telnet
 * Copyright (C) 1999, 2000  Roberto Arturo Tena Sánchez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Roberto Arturo Tena Sánchez  <arturo at users.sourceforge.net>
 */


#ifndef CGITELNET_CONFIG_H
#define CGITELNET_CONFIG_H


typedef struct {
  int fixed_font;
  int break_lines;
  int column_width;
  char* foreground_color;
  char* background_color;
  char escape;
  char escape_replace;
  int max_id_tries;
  int buffer_size;
  int scroll_timeout;
  int scroll_by;
} cgitelnet_configuration;


int cgitelnet_config (cgitelnet_configuration* c);


/* XXX */
#define CGITELNET_DEBUG


#endif /* CGITELNET_CONFIG_H */

