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


#include <stdlib.h>


#include "cgitelnet_config.h"
#include "../support/libxml/tree.h"


/* JPK Comment start */
void parseConfig (xmlDocPtr doc, cgitelnet_configuration* c);
/* JPK Comment end */


int
cgitelnet_config (cgitelnet_configuration* config)
{
  if (config == NULL) return -1;

  config->fixed_font = 1;
  config->break_lines = 1;
  config->column_width = 80;
  //config->foreground_color = "#000000";
  config->foreground_color = "#F0F0F0";
  // JPK config->background_color = "#C0C0C0";
  config->background_color = "#000000";
  config->escape = '';
  config->escape_replace = ' ';
  config->max_id_tries = 500;
  config->buffer_size = 19600;
  config->scroll_timeout = 50;
  config->scroll_by = 300;

  return 0;
/* JPK Comment start */
//  xmlDocPtr doc;
//  cgitelnet_configuration c;
//
//  doc = xmlParseFile ("cgitelnet.xml")
//  if (doc == NULL) {
//    return 0;
//  }
//  parseConfig (doc, &c);
//
//  return 1;
/* JPK Comment end */
}


/* JPK Comment start */
//void parseConfig (xmlDocPtr doc, cgitelnet_configuration* c)
//{
//    xmlNodePtr cur = doc->root;
//    
//    memset (c, 0, sizeof (cgitelnet_configuration));
//
//    /* we don't care what the top level element name is */
//    cur = cur->childs;
//    while (cur != NULL) {
//        if (!strcmp (cur->name, "fixed-font"))
//            c->fixed_font = xmlNodeListGetString (doc, cur->childs, 1);
//        if (!strcmp (cur->name, "col-width"))
//            c->col_width = xmlNodeListGetString (doc, cur->childs, 1);
//        cur = cur->next;
//    }
//}
/* JPK Comment end */

