/*  Copyright (C) 1999, 2000 VideoLAN 
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  Code borrowed from xmms 1.2.4
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licensse as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "intf_urldecode.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* URL-decode a file: URL path, return NULL if it's not what we expect */
void urldecode_path(char *encoded_path)
{
    char *tmp = NULL, *cur = NULL, *ext = NULL;
    int realchar;


    if (!encoded_path || *encoded_path == '\0' )
        return;
    
    cur = encoded_path ;
    
    tmp = calloc(strlen(encoded_path) + 1,  sizeof(char) );

    
    while ( ( ext = strchr(cur, '%') ) != NULL)
    {
        strncat(tmp, cur, (ext - cur) / sizeof(char));
        ext++;

        if (!sscanf(ext, "%2x", &realchar))
        {
            free(tmp);
            return;
        }

        tmp[strlen(tmp)] = (char)realchar;
        
        cur = ext + 2;
    }
    strcat(tmp, cur);
    strcpy(encoded_path,tmp);
}
